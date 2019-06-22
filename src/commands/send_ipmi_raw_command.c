/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
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

static const char *OPT_TIMEZONE_REQUIRED = "Error: option `datetime-local-offset` is required.";

typedef struct _SendIPMICommandOption {
    char *command;
    char *data;
} UtoolSendIPMICommandOption;

static const char *const usage[] = {
        "sendipmirawcmd -n netfun -c command [-b bridge] [-t target] [-d data-list]",
        NULL,
};

static void ValidateSubCommandOptions(UtoolSendIPMICommandOption *option, UtoolResult *result);

/**
* ssend IPMI raw command, command handler for `sendipmirawcmd`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSendIPMIRawCommand(UtoolCommandOption *commandOption, char **outputStr) {

    char *commandOutput = NULL;
    UtoolResult *result = &(UtoolResult) {0};
    UtoolSendIPMICommandOption *sendIpmiCommandOption = &(UtoolSendIPMICommandOption) {0};


    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
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

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubCommandOptions(commandOption, result);
    if (result->interrupt) {
        goto DONE;
    }

    char ipmiCmd[MAX_IPMI_CMD_LEN] = {0};
    char *data = sendIpmiCommandOption->data == NULL ? "" : sendIpmiCommandOption->data;
    snprintf(ipmiCmd, sizeof(ipmiCmd), "%s %s", sendIpmiCommandOption->command, data);

    commandOutput = UtoolIPMIExecCommand(commandOption, ipmiCmd, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    UtoolBuildStringOutputResult(STATE_SUCCESS, commandOutput, &(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    free(commandOutput);
    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void ValidateSubCommandOptions(UtoolSendIPMICommandOption *option, UtoolResult *result) {
    if (UtoolStringIsEmpty(option->command)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("command")),
                                              &(result->desc));
        goto FAILURE;
    }
    return;

FAILURE:
    result->interrupt = 1;
    return;
}
