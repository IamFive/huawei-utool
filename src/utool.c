#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons.h>
#include <curl/curl.h>
#include <cJSON_Utils.h>
#include "typedefs.h"
#include "constants.h"
#include "utool.h"
#include "redfish.h"
#include "argparse.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "log.h"
#include "zf_log.h"
#include <pthread.h>
#include <stdbool.h>

static bool initialized = false;
static pthread_mutex_t mutex;

/**
 * All support commands
 */
static UtoolCommand commands[] = {
        {.name = "getproduct", .pFuncExecute=UtoolGetProduct, .type=GET},
        {.name = "getfw", .pFuncExecute=UtoolGetFirmware, .type=GET},
};


/**
 * argparse usage description
 */
static const char *const usage[] = {
        "utool -H HOST [-p PORT] -U USERNAME -P PASSWORD sub-command ...",
        NULL,
};


/**
 *
 * @param commandOption
 * @param argc
 * @param argv
 * @return OK if succeed else failed
 */
static int utool_parse_command_option(UtoolCommandOption *commandOption, int argc, const char **argv, char **result)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag),
                        "show this help message.",
                        UtoolGetHelpOptionCallback, 0, OPT_NONEG),
            OPT_BOOLEAN('V', "version", &(commandOption->flag),
                        "show tool's version number.",
                        UtoolGetVersionOptionCallback, 0, 0),
            OPT_GROUP  ("Server Authentication Options:"),
            OPT_STRING ('H', "Host", &(commandOption->host),
                        "domain name, IPv4 address, or [IPv6 address].",
                        NULL, 0, 0),
            OPT_INTEGER('p', "Port", &(commandOption->port),
                        "IPMI port, 623 by default.",
                        NULL, 0, 0),
            OPT_STRING ('U', "Username", &(commandOption->username),
                        "local or LDAP username.",
                        NULL, 0, 1),
            OPT_STRING ('P', "Password", &(commandOption->password),
                        "password for given username.",
                        NULL, 0, ARGPARSE_STOP_AT_NON_OPTION),
            OPT_END(),
    };

    struct argparse argparse = {0};
    argparse_init(&argparse, options, usage, 1);
    argparse_describe(&argparse, TOOL_DESC, TOOL_EPI_LOG);
    argc = argparse_parse(&argparse, argc, argv);

    if (!commandOption->port) {
        commandOption->port = HTTPS_PORT
    }

    commandOption->commandArgc = argc;
    commandOption->commandArgv = argv;

    /**
     * TODO Qianbiao.NG
     *  - searching a lib support sub-parser ?
     *  - validating user input
     */

    if (commandOption->flag == FEAT_VERSION) {
        char buff[100] = {0};
        snprintf(buff, 100, "HUAWEI server management command-line tool version v%s", UTOOL_VERSION);
        return UtoolBuildOutputResult(STATE_SUCCESS, cJSON_CreateString(buff), result);
    }
    else if (commandOption->flag == FEAT_HELP) {
        return UtoolBuildOutputResult(STATE_SUCCESS, cJSON_CreateString(HELP_ACTION_OUTPUT_MESSAGE), result);
    }
    else if (argc == 0) {
        ZF_LOGW("Option input error : sub-command is required.");
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE,
                                      cJSON_CreateString("Error: positional option `sub-command` is required."),
                                      result);
    }

    return OK;
}

/**
 * initialize zf-log & curl
 *
 * @param result
 * @return
 */
static int initialize(char **result)
{
    if (!initialized) {
        // get mutex
        pthread_mutex_lock(&mutex);
        CURLcode flag = CURLE_OK;
        if (!initialized) {
            // init log file
            UtoolSetLogFilePath("utool.log.txt");
            ZF_LOGI("Initialize zf-log done.");

            ZF_LOGI("Start to global initialize CURL.");
            // init curl
            flag = curl_global_init(CURL_GLOBAL_ALL);
            if (flag != CURLE_OK) {
                ZF_LOGE("Failed to global initialize curl, reason: %s", curl_easy_strerror(flag));
                UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(curl_easy_strerror(flag)), result);
            }
            atexit(curl_global_cleanup);

            ZF_LOGI("Global initialize CURL done.");

            // update state
            initialized = true;
        }

        // release mutex
        pthread_mutex_destroy(&mutex);

        if (flag != CURLE_OK) { return flag; }
    }

    return CURLE_OK;
}


int utool_main(int argc, char *argv[], char **result)
{
    int ret;

    ret = initialize(result);
    if (ret != OK) {
        return ret;
    }

    ZF_LOGI("Receive new command, start processing now.");

    /**
     * 1. parsing redfish server connection properties from argv
     */
    const char **convert = (const char **) argv;

    /** malloc and zero initialize a command option */
    UtoolCommandOption *commandOption = &(UtoolCommandOption) {0};
    if (!commandOption) {
        ZF_LOGW("Failed to malloc memory for Redfish Connection struct.");
        return UTOOLE_INTERNAL;
    }

    ret = utool_parse_command_option(commandOption, argc, convert, result);
    if (ret != OK || commandOption->flag != EXECUTABLE) {
        goto return_statement;
    }

    /**
     * 2. try to find command function
     */
    const char *commandName = commandOption->commandArgv[0];
    ZF_LOGI("Parsing sub-command from argv : %s", commandName);
    ZF_LOGI("Try to find the command handler for %s.", commandName);

    UtoolCommand *targetCommand = NULL;
    int commandCount = sizeof(commands) / sizeof(UtoolCommand);
    for (int idx = 0; idx < commandCount; idx++) {
        UtoolCommand _command = commands[idx];
        if (strncasecmp(commandName, _command.name, MAX_COMMAND_NAME_LEN * sizeof(char)) == 0) {
            targetCommand = &_command;
            break;
        }
    }

    if (targetCommand) {
        ZF_LOGI("A command handler matched for %s found, try to execute now.", commandName);
        ret = targetCommand->pFuncExecute(commandOption, result);
        if (ret != OK) {
            char *errorString = UtoolGetStringError(ret);
            // we can not use cJSON to build result here, because it may cause problems...
            // UtoolBuildStringOutputResult(STATE_FAILURE, errorString, result);
            char *buffer = malloc(MAX_OUTPUT_LEN);
            snprintf(buffer, MAX_OUTPUT_LEN, OUTPUT_JSON, STATE_FAILURE, errorString);
            *result = buffer;
        }
        goto return_statement;
    }
    else {
        ZF_LOGW("Can not find command handler for %s.", commandName);
        ret = UtoolBuildOutputResult(STATE_FAILURE,
                                     cJSON_CreateString("Error: Not Support. Sub-command is not supported."), result);
        goto return_statement;
    }

return_statement:
    return ret;
}
