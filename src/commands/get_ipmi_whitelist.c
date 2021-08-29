/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command handler for `getipmiwhitelist`
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

#define WHITELIST_ENABLED "01 01"

static const char *const usage[] = {
        "getipmiwhitelist",
        NULL,
};

static const char *const GET_IPMI_WHITELIST_CONF = "0x30 0x93 0xdb 0x07 0x00 0x4c";

// %x stands for the index of white list ipmi command to query
static const char *const GET_IPMI_WHITELIST = "0x30 0x93 0xdb 0x07 0x00 0x4b 0x01 0x%02x";

static UtoolIPMICommand *getIpmiWhitelistCommand(UtoolCommandOption *commandOption, int index, UtoolResult *result);


void FreeIpmiCommand(UtoolIPMICommand *command);

/**
* get IPMI command white list for server internal channel (LPC/USB etc.), command handler for `getipmiwhitelist`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdGetIpmiWhitelist(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL;
    cJSON *commands = NULL;

    int totalCount = 0;
    char *ipmiCmdOutput = NULL;
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};
    UtoolIPMICommand **whitelists = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END()
    };

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // get ipmi white list feature status
    sendIpmiCommandOption->data = GET_IPMI_WHITELIST_CONF;
    ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
    ZF_LOGI("query IPMI whitelist status resp: %s", ipmiCmdOutput);
    if (result->broken) {
        goto FAILURE;
    }

    bool isWhitelistEnabled = UtoolStringEndsWith(ipmiCmdOutput, WHITELIST_ENABLED);
    ZF_LOGI("IPMI whitelist status: %s", isWhitelistEnabled ? ENABLED : DISABLED);
    FREE_OBJ(ipmiCmdOutput)

    cJSON *node = cJSON_AddStringToObject(output, "Enable", isWhitelistEnabled ? ENABLED : DISABLED);
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // if white list is enabled, we should get
    if (isWhitelistEnabled) {
        UtoolIPMICommand *first = getIpmiWhitelistCommand(commandOption, 1, result);
        if (result->broken) {
            goto FAILURE;
        }

        totalCount = *(first->total);
        if (totalCount > 0 && totalCount <= 0xFF) {
            whitelists = (UtoolIPMICommand **) malloc(sizeof(UtoolIPMICommand *) * totalCount);
            result->code = UtoolAssetMallocNotNull(whitelists);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
            *whitelists = first;

            commands = cJSON_AddArrayToObject(output, "Command");
        } else {
            FreeIpmiCommand(first);
        }

        for (int index = 2; index <= totalCount; index++) {
            UtoolIPMICommand *command = getIpmiWhitelistCommand(commandOption, index, result);
            if (!result->broken) {
                *(whitelists + index - 1) = command;
            }
        }

        // FIXME(turnbig): group command by netfun
        for (int index = 0; index < totalCount; index++) {
            UtoolIPMICommand *command = *(whitelists + index);
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "NetFunction", command->netfun);
            cJSON_AddStringToObject(item, "CmdList", command->command);
            cJSON_AddStringToObject(item, "SubFunction", command->data);
            cJSON_AddItemToArray(commands, item);
        }
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_OBJ(ipmiCmdOutput)
    UtoolFreeRedfishServer(server);

    for (int index = 0; index < totalCount; index++) {
        UtoolIPMICommand *command = *(whitelists + index);
        FreeIpmiCommand(command);
    }
    FREE_OBJ(whitelists)

    *outputStr = result->desc;
    return result->code;
}

void FreeIpmiCommand(UtoolIPMICommand *command)
{
    if (command != NULL) {
        FREE_OBJ(command->netfun)
        FREE_OBJ(command->command)
        FREE_OBJ(command->data)
        FREE_OBJ(command->total)
        FREE_OBJ(command->length)
        FREE_OBJ(command)
    }
}

UtoolIPMICommand *getIpmiWhitelistCommand(UtoolCommandOption *commandOption, int index, UtoolResult *result)
{
    char *data = NULL;
    char *ipmiCmdOutput = NULL;
    char **segments = NULL;

    UtoolIPMICommand *command = NULL;
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    command = (UtoolIPMICommand *) malloc(sizeof(UtoolIPMICommand));
    if (command == NULL) {
        result->broken = 1;
        result->code = UTOOLE_INTERNAL;
        goto DONE;
    }

    command->length = NULL;
    command->total = NULL;
    command->netfun = NULL;
    command->command = NULL;
    command->data = NULL;

    char getIpmiWhitelistCmd[MAX_IPMI_CMD_LEN] = {0};
    UtoolWrapSnprintf(getIpmiWhitelistCmd, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, GET_IPMI_WHITELIST, index);
    sendIpmiCommandOption->data = getIpmiWhitelistCmd;

    /**
     * response sample: db 07 00 03 07 0c 02 01 01 03 00 00
     *                           |  |  |  |  |  |---------|
     *                   count --|  |  |  |  |       |
     *                       len  --|  |  |  |       |
     *                        netfun --|  |  |       |
     *                          command --|  |       |
     *                             channel --|       |
     *                                        data --|
     */
    ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
    ZF_LOGI("Get IPMI whitelist with index %d resp：%s", index, ipmiCmdOutput);
    if (result->broken) {
        goto FAILURE;
    }

    size_t ipmiCmdOutputLen = strnlen(ipmiCmdOutput, MAX_IPMI_CMD_LEN);
    // FIXME(turnbig): get data
    if (ipmiCmdOutputLen > 25) {
        int size = ipmiCmdOutputLen - 25;
        command->data = (char *) malloc(sizeof(char) * (size + 1));
        result->code = UtoolAssetMallocNotNull(command->data);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        errno_t ok = strncpy_s(command->data, size + 1, ipmiCmdOutput + 25, size);
        if (ok != EOK) {
            perror("Failed to `strncpy`.");
            exit(EXIT_SECURITY_ERROR);
        }
    }

    segments = UtoolStringSplit(ipmiCmdOutput, ' ');
    command->total = (int *) malloc(sizeof(int));
    result->code = UtoolAssetMallocNotNull(command->total);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (segments[3] != NULL) {
        *(command->total) = (int) strtol(segments[3], NULL, 16);
    }

    if (segments[4] != NULL) {
        command->length = (int *) malloc(sizeof(int));
        result->code = UtoolAssetMallocNotNull(command->length);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        *(command->length) = (int) strtol(segments[4], NULL, 16);

        if (*command->length > 1) {
            char *netfun = segments[5];
            command->netfun = UtoolStringNDup(netfun, strnlen(netfun, MAX_COMMAND_NAME_LEN));
            result->code = UtoolAssetMallocNotNull(command->netfun);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (*command->length > 2) {
            char *cmd = segments[6];
            command->command = UtoolStringNDup(cmd, strnlen(cmd, MAX_COMMAND_NAME_LEN));
            result->code = UtoolAssetMallocNotNull(command->command);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_OBJ(ipmiCmdOutput)
    UtoolStringFreeArrays(segments);
    return command;
}
