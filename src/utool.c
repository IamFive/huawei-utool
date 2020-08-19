/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: utool API implementation
* Author:
* Create: 2019-06-16
* Notes:
*/
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
#include <ipmi.h>
#include <securec.h>

static bool initialized = false;
static pthread_mutex_t mutex;


/**
 * All support commands
 */
UtoolCommand g_UtoolCommands[] = {
        {.name = "getcapabilities", .pFuncExecute=UtoolCmdGetCapabilities, .type=GET},
        {.name = "getproduct", .pFuncExecute=UtoolCmdGetProduct, .type=GET},
        {.name = "getfw", .pFuncExecute=UtoolCmdGetFirmware, .type=GET},
        {.name = "getip", .pFuncExecute=UtoolCmdGetBmcIP, .type=GET},
        {.name = "getcpu", .pFuncExecute=UtoolCmdGetProcessor, .type=GET},
        {.name = "getmemory", .pFuncExecute=UtoolCmdGetMemory, .type=GET},
        {.name = "gettemp", .pFuncExecute=UtoolCmdGetTemperature, .type=GET},
        {.name = "getvolt", .pFuncExecute=UtoolCmdGetVoltage, .type=GET},
        {.name = "getpsu", .pFuncExecute=UtoolCmdGetPowerSupply, .type=GET},
        {.name = "getfan", .pFuncExecute=UtoolCmdGetFan, .type=GET},
        {.name = "getraid", .pFuncExecute=UtoolCmdGetRAID, .type=GET},
        {.name = "getpdisk", .pFuncExecute=UtoolCmdGetPhysicalDisks, .type=GET},
        {.name = "getldisk", .pFuncExecute=UtoolCmdGetLogicalDisks, .type=GET},
        {.name = "getnic", .pFuncExecute=UtoolCmdGetNIC, .type=GET},
        {.name = "getuser", .pFuncExecute=UtoolCmdGetUsers, .type=GET},
        {.name = "getservice", .pFuncExecute=UtoolCmdGetServices, .type=GET},
        {.name = "geteventsub", .pFuncExecute=UtoolCmdGetEventSubscriptions, .type=GET},
        {.name = "getpwrcap", .pFuncExecute=UtoolCmdGetPowerCapping, .type=GET},
        {.name = "getmgmtport", .pFuncExecute=UtoolCmdGetMgmtPort, .type=GET},
        {.name = "gettrap", .pFuncExecute=UtoolCmdGetSNMP, .type=GET},
        {.name = "getvnc", .pFuncExecute=UtoolCmdGetVNC, .type=GET},
        {.name = "gethealth", .pFuncExecute=UtoolCmdGetHealth, .type=GET},
        {.name = "getsysboot", .pFuncExecute = UtoolCmdGetSystemBoot, .type = GET},
        {.name = "getsensor", .pFuncExecute = UtoolCmdGetSensor, .type = GET},
        {.name = "getbios", .pFuncExecute = UtoolCmdGetBiosSettings, .type = GET},
        {.name = "getbiossetting", .pFuncExecute = UtoolCmdGetPendingBiosSettings, .type = GET},
        {.name = "getbiosresult", .pFuncExecute = UtoolCmdGetBiosResult, .type = GET},
        {.name = "gethealthevent", .pFuncExecute = UtoolCmdGetHealthEvent, .type = GET},
        {.name = "geteventlog", .pFuncExecute = UtoolCmdGetEventLog, .type = GET},
        {.name = "getpcie", .pFuncExecute = UtoolCmdGetPCIe, .type = GET},
        {.name = "gettime", .pFuncExecute = UtoolCmdGetTime, .type = GET},

        {.name = "adduser", .pFuncExecute = UtoolCmdAddUser, .type = SET},
        {.name = "setpwd", .pFuncExecute = UtoolCmdSetPassword, .type = SET},
        {.name = "setpriv", .pFuncExecute = UtoolCmdSetUserPriv, .type = SET},
        {.name = "deluser", .pFuncExecute = UtoolCmdDeleteUser, .type = SET},
        {.name = "mountvmm", .pFuncExecute = UtoolCmdMountVMM, .type = SET},
        {.name = "clearsel", .pFuncExecute = UtoolCmdClearSEL, .type = SET},
        //{.name = "settime", .pFuncExecute = UtoolCmdSetTime, .type = SET},
        {.name = "settimezone", .pFuncExecute = UtoolCmdSetTimezone, .type = SET},
        {.name = "powercontrol", .pFuncExecute = UtoolCmdSystemPowerControl, .type = SET},
        {.name = "restorebios", .pFuncExecute = UtoolCmdRestoreBIOS, .type = SET},
        {.name = "restorebmc", .pFuncExecute = UtoolCmdRestoreBMC, .type = SET},
        {.name = "setservice", .pFuncExecute = UtoolCmdSetService, .type = SET},
        {.name = "setip", .pFuncExecute = UtoolCmdSetIP, .type = SET},
        {.name = "setvlan", .pFuncExecute = UtoolCmdSetVLAN, .type = SET},
        {.name = "resetbmc", .pFuncExecute = UtoolCmdResetBMC, .type = SET},
        {.name = "settrapcom", .pFuncExecute = UtoolCmdSetSNMPTrapNotification, .type = SET},
        {.name = "settrapdest", .pFuncExecute = UtoolCmdSetSNMPTrapNotificationDest, .type = SET},
        {.name = "fwupdate", .pFuncExecute = UtoolCmdUpdateOutbandFirmware, .type = SET},
        {.name = "collect", .pFuncExecute = UtoolCmdCollectAllBoardInfo, .type = SET},
        {.name = "locateserver", .pFuncExecute = UtoolCmdSetIndicatorLED, .type = SET},
        {.name = "setvnc", .pFuncExecute = UtoolCmdSetVNCSettings, .type = SET},
        {.name = "setsysboot", .pFuncExecute = UtoolCmdSetBootSourceOverride, .type = SET},
        {.name = "delvncsession", .pFuncExecute = UtoolCmdDeleteVNCSession, .type = SET},
        {.name = "setadaptiveport", .pFuncExecute = UtoolCmdSetAdaptivePort, .type = SET},
        {.name = "setbios", .pFuncExecute = UtoolCmdSetBIOS, .type = SET},
        {.name = "importbmccfg", .pFuncExecute = UtoolCmdImportBMCCfg, .type = SET},
        {.name = "exportbmccfg", .pFuncExecute = UtoolCmdExportBMCCfg, .type = SET},
        {.name = "setfan", .pFuncExecute = UtoolCmdSetFan, .type = SET},
        {.name = "locatedisk", .pFuncExecute = UtoolCmdLocateDisk, .type = SET},
        {.name = "sendipmirawcmd", .pFuncExecute = UtoolCmdSendIPMIRawCommand, .type = SET},

        // Test purpose start
        {.name = "upload", .pFuncExecute = UtoolCmdUploadFileToBMC, .type = DEBUG},
        {.name = "scp", .pFuncExecute = UtoolCmdScpFileToBMC, .type = DEBUG},
        {.name = "download", .pFuncExecute = UtoolCmdDownloadBMCFile, .type = DEBUG},
        {.name = "waittask", .pFuncExecute = UtoolCmdWaitRedfishTask, .type = DEBUG},
        {.name = "gettaskstate", .pFuncExecute = UtoolCmdGetTasks, .type = DEBUG},
        {0},
};

/**
 * argparse usage description
 */
static const char *const usage[] = {
        "utool -H host [-p port] -U username -P password sub-command ...",
        NULL,
};

/**
 *
 * @param commandOption
 * @param argc
 * @param argv
 * @return OK if succeed else failed
 */
static int utool_parse_command_option(UtoolCommandOption *commandOption, int argc, const char **argv, char **result) {
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag),
                        "show this help message.",
                        UtoolGetHelpOptionCallback, 0, OPT_NONEG),
            OPT_BOOLEAN('V', "version", &(commandOption->flag),
                        "show tool's version number.",
                        UtoolGetVersionOptionCallback, 0, 0),
            OPT_GROUP  ("Server Authentication Options:"),
            OPT_STRING ('H', "host", &(commandOption->host),
                        "domain name, IPv4 address, or [IPv6 address].",
                        NULL, 0, 0),
            OPT_INTEGER('p', "port", &(commandOption->ipmiPort),
                        "IPMI port, 623 by default.",
                        NULL, 0, 0),
            OPT_STRING ('U', "username", &(commandOption->username),
                        "local or LDAP username.",
                        NULL, 0, 1),
            OPT_STRING ('P', "password", &(commandOption->password),
                        "password for given username.",
                        NULL, 0, ARGPARSE_STOP_AT_NON_OPTION),
            OPT_END(),
    };

    struct argparse parser = {0};
    argparse_init(&parser, options, usage, 1);
    argparse_describe(&parser, TOOL_DESC, TOOL_EPI_LOG);
    argc = argparse_parse(&parser, argc, argv);
    if (parser.error) {
        commandOption->flag = ILLEGAL;
        int ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(parser.reason), result);
        FREE_OBJ(parser.reason);
        return ret;
    }

    if (!commandOption->ipmiPort) {
        commandOption->ipmiPort = IPMI_PORT;
    }

    commandOption->commandArgc = argc;
    commandOption->commandArgv = argv;

    /**
     * TODO Qianbiao.NG
     *  - searching a lib support sub-parser ?
     *  - validating user input
     */

    if (commandOption->flag == FEAT_VERSION) {
        char buff[MAX_FAILURE_MSG_LEN] = {0};
        snprintf_s(buff, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN,
                   "HUAWEI server management command-line tool version v%s", UTOOL_VERSION);
        return UtoolBuildOutputResult(STATE_SUCCESS, cJSON_CreateString(buff), result);
    } else if (commandOption->flag == FEAT_HELP) {
        //return UtoolBuildOutputResult(STATE_SUCCESS, cJSON_CreateString(HELP_ACTION_OUTPUT_MESSAGE), result);
        return UTOOLE_OK;
    } else if (argc == 0) {
        ZF_LOGW("Option input error : sub-command is required.");
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE,
                                      cJSON_CreateString("Error: positional option `sub-command` is required."),
                                      result);
    }

    return UTOOLE_OK;
}

/**
 * initialize zf-log & curl
 *
 * @param result
 * @return
 */
static int initialize(char **result) {
    if (!initialized) {
        // get mutex
        pthread_mutex_lock(&mutex);
        CURLcode flag = CURLE_OK;
        if (!initialized) {
            // init log file
            zf_log_set_output_level(ZF_LOG_INFO);
            int ret = UtoolSetLogFilePath("utool.log.txt");
            if (!ret) {
                ZF_LOGI("Initialize zf-log done.");
            } else {
                ret = UTOOLE_CREATE_LOG_FILE;
                //fprintf(stderr, "Failed to create log file `utool.log.txt`");
                return ret;
            }

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
        pthread_mutex_unlock(&mutex);

        if (flag != CURLE_OK) {
            return flag;
        }
    }

    return CURLE_OK;
}


int utool_main(int argc, char *argv[], char **result) {
    int ret;
    const char *errorString = NULL;

    ret = initialize(result);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
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
        ret = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    ret = utool_parse_command_option(commandOption, argc, convert, result);
    if (ret != UTOOLE_OK || commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    /**
     * 2. try to find command function
     */
    const char *commandName = commandOption->commandArgv[0];
    ZF_LOGI("Parsing sub-command from argv : %s", commandName);
    ZF_LOGI("Try to find the command handler for %s.", commandName);

    UtoolCommand *targetCommand = NULL;
    for (int idx = 0;; idx++) {
        UtoolCommand *_command = g_UtoolCommands + idx;
        if (_command->name != NULL) {
            if (strncasecmp(commandName, _command->name, MAX_COMMAND_NAME_LEN * sizeof(char)) == 0) {
                targetCommand = _command;
                break;
            }
        } else {
            break;
        }
    }

    if (targetCommand) {
        ZF_LOGI("A command handler matched for %s found, try to execute now.", commandName);
        ret = targetCommand->pFuncExecute(commandOption, result);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // if ret is ok, it means everything has been processed by command handler
        goto DONE;
    } else {
        ZF_LOGW("Can not find command handler for %s.", commandName);
        char buffer[MAX_FAILURE_MSG_LEN];
        snprintf_s(buffer, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN, "Error: Sub-command `%s` is not supported.",
                   commandName);
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(buffer), result);
        goto DONE;
    }


FAILURE:
    errorString = (ret > UTOOLE_OK && ret < ((int) CURL_LAST)) ?
                  curl_easy_strerror((CURLcode) ret) : UtoolGetStringError((UtoolCode) ret);
    // we can not use cJSON to build result here, because it may cause problems...
    // UtoolBuildStringOutputResult(STATE_FAILURE, errorString, result);
    char *buffer = (char *) malloc(MAX_OUTPUT_LEN);
    if (buffer != NULL) {
        snprintf_s(buffer, MAX_OUTPUT_LEN, MAX_OUTPUT_LEN, OUTPUT_JSON, STATE_FAILURE, STATE_FAILURE, errorString);
        *result = buffer;
    } else {
        *result = OUTPUT_INTERNAL_FAILED_JSON;
    }
    goto DONE;

DONE:
    if (ret != UTOOLE_CREATE_LOG_FILE) {
        ZF_LOGI("Command processed, return code is: %d, result is: %s", ret, *result);
    }
    return ret;
}
