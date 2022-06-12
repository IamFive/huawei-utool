/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `sendipmirawcmd`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ipmi.h>
#include <securec.h>
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

static const char *const usage[] = {
        "sendipmirawcmd [-b bridge] [-t target] -n netfun -c command [-d data-list]",
        NULL,
};


static void ValidateSubCommandOptions(UtoolIPMIRawCmdOption *option, UtoolResult *result);

int BuildOutputResult(const char *ipmiCmdOutput, char **result);

/**
* send IPMI raw command, command handler for `sendipmirawcmd`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSendIPMIRawCommand(UtoolCommandOption *commandOption, char **outputStr)
{

    char *commandOutput = NULL;
    UtoolResult *result = &(UtoolResult) {0};
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('b', "bridge", &(sendIpmiCommandOption->bridge),
                        "specifies the destination channel for bridged request", NULL, 0, 0),
            OPT_STRING ('t', "target", &(sendIpmiCommandOption->target),
                        "specifies the bridge request to remote target address", NULL, 0, 0),
            OPT_STRING ('n', "netfun", &(sendIpmiCommandOption->netfun),
                        "specifies the NetFun of ipmi command", NULL, 0, 0),
            OPT_STRING ('c', "command", &(sendIpmiCommandOption->command),
                        "specifies the ipmi command to sent", NULL, 0, 0),
            OPT_STRING ('d', "data-list", &(sendIpmiCommandOption->data),
                        "specifies the data to sent, if data is hex, 0x is required", NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateIPMIConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubCommandOptions(sendIpmiCommandOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    commandOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
    if (result->broken || commandOutput == NULL) {
        goto FAILURE;
    }

    result->code = BuildOutputResult(commandOutput, &(result->desc));
    goto DONE;

FAILURE:
    // data is malloced in ${ValidateSubCommandOptions} method,
    // if program returns before that, will raise 'free(): invalid pointer' if free option->data in DONE.
    FREE_OBJ(sendIpmiCommandOption->data);
    goto DONE;

DONE:
    FREE_OBJ(commandOutput);
    *outputStr = result->desc;
    return result->code;
}

void ReplaceChar(const char *str, char find, char replace)
{
    char *currentPos = strchr(str, find);
    while (currentPos) {
        *currentPos = replace;
        currentPos = strchr(currentPos, find);
    }
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void ValidateSubCommandOptions(UtoolIPMIRawCmdOption *option, UtoolResult *result)
{

    if (UtoolStringIsEmpty(option->netfun)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("netfun")),
                                              &(result->desc));
        goto FAILURE;
    } else if (strnlen(option->netfun, 33) > 32) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ILLEGAL("netfun")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->command)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("command")),
                                              &(result->desc));
        goto FAILURE;
    } else if (strnlen(option->command, 33) > 32) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ILLEGAL("command")),
                                              &(result->desc));
        goto FAILURE;
    }


    /* replace all dot in data to space */
    if (!UtoolStringIsEmpty(option->data)) {
        char *data = UtoolStringNDup(option->data, strnlen(option->data, MAX_IPMI_CMD_LEN));
        if (data != NULL) {
            ReplaceChar(data, ',', ' ');
            option->data = data;
        } else {
            result->code = UTOOLE_INTERNAL;
            goto FAILURE;
        }
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

int BuildOutputResult(const char *ipmiCmdOutput, char **result)
{
    int ret = UTOOLE_CREATE_JSON_NULL;

    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        goto DONE;
    }

    if (cJSON_AddStringToObject(response, RESULT_KEY_IPMI_RSP, ipmiCmdOutput) == NULL) {
        goto DONE;
    }

    char *pretty = cJSON_Print(response);
    if (pretty != NULL) {
        *result = pretty;
        ret = UTOOLE_OK;
    }

    goto DONE;

DONE:
    FREE_CJSON(response)
    return ret;
}
