/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command handler for `setipmiwhitelist`
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <securec.h>
#include <ipmi.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"
#include "string_utils.h"
#include "url_parser.h"

#define OPERATION_ADD "Add"
#define OPERATION_REMOVE "Remove"

static const char *const ENABLE_IPMI_WHITELIST = "0x30 0x93 0xdb 0x07 0x0 0x4a 0x01 0x01 0x00";
static const char *const DISABLE_IPMI_WHITELIST = "0x30 0x93 0xdb 0x07 0x0 0x4a 0x00 0xff";

/**
 * 0x30 0x93 0xdb 0x07 0x00 0x3f 0x01 0x00 0x01 0x0c 0x02 0x08 0x01 0x10 0x00 0x00
 * 0x30 0x93 0xdb 0x07 0x00 0x3f 0x01  %s  0x01  %s    %s 0x08  %s
 * 0x30 0x93 0xdb 0x07 0x00 0x3f 0x01 0x00 0x01 netfn cmd chan data
 *                                |    |                    |
 *        black 00; white 01 <----|    |                    |
 *           Add: 00; Remove: 01; <----|                    |
 *   Net: 0x01; BT: 0x08; whitelist only support BT  <------|
 */
static const char *const AD_WHITELIST = "0x30 0x93 0xdb 0x07 0x00 0x3f 0x01 %s 0x01 %s %s 0x08 %s";


static const char *OPERATION_CHOICES[] = {
        OPERATION_ADD, OPERATION_REMOVE, NULL
};

static const char *OPT_ENABLED_ILLEGAL = "Error: option `enabled` is illegal, available choices: Enabled, Disabled.";
static const char *OPT_OPERATION_ILLEGAL = "Error: option `operation` is illegal, available choices: Add, Remove.";
static const char *OPT_OPERATION_REQUIRED = "Error: option `operation` is required to add/remove whitelist.";
static const char *OPT_CMD_FILE_EXCLUSION = "Error: option `cmd-file` and option `netfun`, `command`, `sub-function` "
                                            "are mutually exclusive.";

static const char *OPT_CMD_OPTION_REQUIRED = "Error: option `cmd-file` or option `netfun` is required if you want to "
                                             "operation IPMI whitelist.";
static const char *OPT_NETFN_IS_REQUIRED = "Error: option `netfun` is required when `command` option present.";
static const char *OPT_CMD_IS_REQUIRED = "Error: option `command` is required when `sub-function` option present.";

static const char *OPT_WHITELIST_DISABLED = "Error: all other options is disabled when option `enabled` is set to "
                                            "'Disabled'.";

static const char *OPT_JSON_FILE_ILLEGAL = "Error: input whitelist JSON file is not well formed.";
static const char *OPT_JSON_FILE_STRUCT_UNKNOWN = "Error: input whitelist JSON file is not well structured.";


static const char *const usage[] = {
        "setipmiwhitelist [-e enable] [-n netfun] [-c command] [-d sub-function] [-f cmd-file] [-o operation]",
        NULL,
};


typedef struct _SetIpmiWhitelistOption {
    char *enabled;
    char *netfun;
    char *command;
    char *subFunc;
    char *importFilePath;
    char *operation;
    FILE *importFileFP;

} UtoolSetIpmiWhitelistOption;

static cJSON *BuildPayload(UtoolSetIpmiWhitelistOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetIpmiWhitelistOption *option, UtoolRedfishServer *server,
                                      UtoolResult *result);

void
ToggleIpmiWhitelist(UtoolCommandOption *commandOption, const UtoolSetIpmiWhitelistOption *option, UtoolResult *result);

void HandleWhitelistAction(UtoolCommandOption *commandOption, const UtoolSetIpmiWhitelistOption *option,
                           UtoolResult *result);

void HandleWhitelistJsonFile(UtoolCommandOption *commandOption, UtoolSetIpmiWhitelistOption *option, UtoolResult *result);

/**
* get IPMI command white list for server internal channel (LPC/USB etc.), command handler for `getipmiwhitelist`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetIpmiWhitelist(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;
    size_t numbytes;
    char *fileContent = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetIpmiWhitelistOption *option = &(UtoolSetIpmiWhitelistOption) {0};

    char *ipmiCmdOutput = NULL;
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('e', "enabled", &(option->enabled),
                        "specifies whether IPMI whitelist is enabled, available choices: {Enabled, Disabled}",
                        NULL, 0, 0),
            OPT_STRING ('o', "operation", &(option->operation),
                        "specifies the whitelist operation type, available choices: {Add, Remove}", NULL, 0, 0),
            OPT_GROUP("Single whitelist command"),
            OPT_STRING ('n', "netfun", &(option->netfun),
                        "specifies the netfun of IPMI command to add/remove", NULL, 0, 0),
            OPT_STRING ('c', "command", &(option->command),
                        "specifies the command of IPMI command to add/remove", NULL, 0, 0),
            OPT_STRING ('d', "sub-function", &(option->subFunc),
                        "specifies the sub-function of IPMI command to add/remove", NULL, 0, 0),
            OPT_GROUP("Multiple whitelist commands"),
            OPT_STRING ('f', "cmd-file", &(option->importFilePath),
                        "specifies JSON file path which indicates IPMI commands to apply to whitelist.",
                        NULL, 0, 0),

            OPT_END()
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubcommandOptions(option, server, result);
    if (result->broken) {
        goto DONE;
    }

    ToggleIpmiWhitelist(commandOption, option, result);
    if (result->broken) {
        goto FAILURE;
    }

    // handle single IPMI whitelist add/remove
    if (!UtoolStringIsEmpty(option->netfun)) {
        HandleWhitelistAction(commandOption, option, result);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // handle multiple IPMI whitelist add/remove
    if (option->importFileFP) {
        HandleWhitelistJsonFile(commandOption, option, result);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    if (option->importFileFP) {                  /* close FP */
        fclose(option->importFileFP);
        option->importFileFP = NULL;
    }

    *outputStr = result->desc;
    return result->code;
}

void HandleWhitelistJsonFile(UtoolCommandOption *commandOption, UtoolSetIpmiWhitelistOption *option, UtoolResult *result)
{
    cJSON *payload = NULL;
    size_t numbytes;
    char *fileContent = NULL;

    // handle multiple IPMI whitelist add/remove
    if (option->importFileFP) {
        ZF_LOGI("Try to parse whitelist JSON file: %s now.", option->importFilePath);
        char realFilePath[PATH_MAX] = {0};

        /* Get the number of bytes */
        fseek(option->importFileFP, 0L, SEEK_END);
        numbytes = ftell(option->importFileFP);
        /* reset the file position indicator to the beginning of the file */
        fseek(option->importFileFP, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text */
        fileContent = (char *) calloc(numbytes + 1, sizeof(char));
        /* memory error */
        if (fileContent == NULL) {
            result->code = UTOOLE_INTERNAL;
            goto FAILURE;
        }

        /* copy all the text into the buffer */
        size_t size = fread(fileContent, sizeof(char), numbytes, option->importFileFP);
        fileContent[numbytes] = '\0';
        ZF_LOGI("Whitelist JSON file content is: %s", fileContent);

        /** parse file content */
        payload = cJSON_Parse(fileContent);
        if (size != numbytes || !payload) {
            ZF_LOGI("File format is illegal, not well json formed, position: %s.", cJSON_GetErrorPtr());
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_JSON_FILE_ILLEGAL), &(result->desc));
            goto FAILURE;
        }

        cJSON *root = cJSONUtils_GetPointer(payload, "/Command");
        if (root == NULL || !cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
            goto STRUCT_UNKNOWN;
        }

        cJSON *element = NULL;
        cJSON_ArrayForEach(element, root) {
            cJSON *netfun = cJSON_GetObjectItem(element, "NetFunction");
            cJSON *commandList = cJSON_GetObjectItem(element, "CmdList");
            cJSON *subFuncList = cJSON_GetObjectItem(element, "SubFunction");
            if (netfun == NULL || netfun->valuestring == NULL) {
                goto STRUCT_UNKNOWN;
            }

            //TODO(turnbig), other fields validation?
            option->netfun = netfun->valuestring;
            option->command = commandList->valuestring;
            option->subFunc = subFuncList->valuestring;
            HandleWhitelistAction(commandOption, option, result);
            if (result->broken) {
                goto FAILURE;
            }
        }
    }

    goto DONE;

STRUCT_UNKNOWN:
    result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_JSON_FILE_STRUCT_UNKNOWN), &(result->desc));
    goto FAILURE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_OBJ(fileContent)

    if (option->importFileFP) {                  /* close FP */
        fclose(option->importFileFP);
        option->importFileFP = NULL;
    }

}

void HandleWhitelistAction(UtoolCommandOption *commandOption, const UtoolSetIpmiWhitelistOption *option,
                           UtoolResult *result)
{
    char *ipmiCmdOutput = NULL;
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    ZF_LOGI("%s whitelist:: netfun: %s, command: %s, sub-function: %s", option->operation, option->netfun, option
            ->command, option->subFunc);
    char *operation = UtoolStringEquals(option->operation, OPERATION_ADD) ?
                      ACTION_ADD_WHITELIST : ACTION_DEL_WHITELIST;
    char *command = UtoolStringIsEmpty(option->command) ? "" : option->command;
    char *subFunc = UtoolStringIsEmpty(option->subFunc) ? "" : option->subFunc;

    // FIXME(turnbig): what last "data" comes from.
    /**
     * 0x30 0x93 0xdb 0x07 0x00 0x3f 0x01  %s  0x01  %s    %s 0x08  %s
     * 0x30 0x93 0xdb 0x07 0x00 0x3f 0x01 0x00 0x01 netfn cmd chan data
     *                                |    |                    |
     *        black 00; white 01 <----|    |                    |
     *           Add: 00; Remove: 01; <----|                    |
     *   Net: 0x01; BT: 0x08; whitelist only support BT  <------|
     */
    char ipmiCmd[MAX_IPMI_CMD_LEN] = {0};
    snprintf_s(ipmiCmd, sizeof(ipmiCmd), sizeof(ipmiCmd), AD_WHITELIST, operation, option->netfun,
               command, subFunc);
    sendIpmiCommandOption->data = ipmiCmd;
    ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
    ZF_LOGI("ipmi raw command resp: %s", ipmiCmdOutput);
    FREE_OBJ(ipmiCmdOutput);
}

void
ToggleIpmiWhitelist(UtoolCommandOption *commandOption, const UtoolSetIpmiWhitelistOption *option, UtoolResult *result)
{
    char *ipmiCmdOutput = NULL;
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    if (!UtoolStringIsEmpty(option->enabled)) {
        // enable/disable whitelist feature
        if (UtoolStringEquals(option->enabled, DISABLED)) {
            ZF_LOGI("Disable IPMI whitelist feature now.");
            sendIpmiCommandOption->data = DISABLE_IPMI_WHITELIST;
            ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
            ZF_LOGI("Disable IPMI whitelist action resp: %s", ipmiCmdOutput);
            FREE_OBJ(ipmiCmdOutput)
        } else if (UtoolStringEquals(option->enabled, ENABLED)) {
            ZF_LOGI("Enable IPMI whitelist feature now.");
            sendIpmiCommandOption->data = ENABLE_IPMI_WHITELIST;
            ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
            ZF_LOGI("Enable IPMI whitelist action resp: %s", ipmiCmdOutput);
            FREE_OBJ(ipmiCmdOutput)
        }
    }

    goto DONE;

DONE:
    FREE_OBJ(ipmiCmdOutput)
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void
ValidateSubcommandOptions(UtoolSetIpmiWhitelistOption *option, UtoolRedfishServer *server, UtoolResult *result)
{
    FILE *importFileFP = NULL;

    if (!UtoolStringIsEmpty(option->enabled)) {
        // validate enabled choices list if present
        if (!UtoolStringInArray(option->enabled, g_UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        // if target action is disable whitelist, all other options is disabled.
        if (UtoolStringEquals(option->enabled, DISABLED)) {
            if (!UtoolStringIsEmpty(option->operation) || !UtoolStringIsEmpty(option->netfun)
                || !UtoolStringIsEmpty(option->command) || !UtoolStringIsEmpty(option->subFunc)
                || !UtoolStringIsEmpty(option->importFilePath)) {
                result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_WHITELIST_DISABLED),
                                                      &(result->desc));
                goto FAILURE;
            }
        }
    }

    // validate enabled choices list if present
    if (!UtoolStringIsEmpty(option->operation)) {
        if (!UtoolStringInArray(option->operation, OPERATION_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_OPERATION_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->netfun) && !UtoolStringIsEmpty(option->command)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_NETFN_IS_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->command) && !UtoolStringIsEmpty(option->subFunc)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_CMD_IS_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->importFilePath)) {

        if (!UtoolStringIsEmpty(option->netfun) || !UtoolStringIsEmpty(option->command) ||
            !UtoolStringIsEmpty(option->subFunc)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_CMD_FILE_EXCLUSION),
                                                  &(result->desc));
            goto FAILURE;
        }

        struct stat fileInfo;
        char realFilePath[PATH_MAX] = {0};
        UtoolFileRealpath(option->importFilePath, realFilePath);
        if (realFilePath == NULL) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        }

        importFileFP = fopen(realFilePath, "rb"); /* open file to upload */
        if (!importFileFP) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        } /* can't continue */

        ///* get the file size */
        if (fstat(fileno(importFileFP), &fileInfo) != 0) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
            goto FAILURE;
        } /* can't continue */

        option->importFileFP = importFileFP;
    }

    // if user does not want to operation whitelist, operation option should not present
    if (UtoolStringIsEmpty(option->netfun) && UtoolStringIsEmpty(option->importFilePath)) {
        if (!UtoolStringIsEmpty(option->operation)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_CMD_OPTION_REQUIRED),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    // if user does not want to operation whitelist, operation option should not present
    if (!UtoolStringIsEmpty(option->netfun) || !UtoolStringIsEmpty(option->importFilePath)) {
        if (UtoolStringIsEmpty(option->operation)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_OPERATION_REQUIRED),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    return;
}

static cJSON *BuildPayload(UtoolSetIpmiWhitelistOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "Type", "URI");
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}