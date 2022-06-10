/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command handler for `getipmiwhitelist`
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <securec.h>
#include <ipmi.h>
#include "commons.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "string_utils.h"

#define WHITELIST_ENABLED "01 01"

#define DFT_SUB_FUNC "ff"
#define OEM_NETFUNC "30"
#define OEM_SUB_FUNC_PREFIX "db 07 00"

static const char *const usage[] = {
        "getipmiwhitelist",
        NULL,
};

static const char *const GET_IPMI_WHITELIST_CONF = "0x30 0x93 0xdb 0x07 0x00 0x4c";
static const char *const GET_IPMI_WHITELIST_CONF_XFUSION = "0x30 0x93 0x14 0xe3 0x00 0x4c";
static const int SINGLE_BYTE_LEN = 3;
static const int DATA_PART_POS = 9;

// %x stands for the index of white list ipmi command to query
static const char *const GET_IPMI_WHITELIST = "0x30 0x93 0xdb 0x07 0x00 0x4b 0x01 0x%02x";
static const char *const GET_IPMI_WHITELIST_XFUSION = "0x30 0x93 0x14 0xe3 0x00 0x4b 0x01 0x%02x";

static UtoolIPMICommand *getIpmiWhitelistCommand(UtoolCommandOption *commandOption, int index, UtoolResult *result);
static bool vendorIdXFUSION = false;

void FreeIpmiCommand(UtoolIPMICommand *command);

void FreeIpmiCommand(UtoolIPMICommand *command)
{
    if (command != NULL) {
        FREE_OBJ(command->netfun)
        FREE_OBJ(command->command)
        FREE_OBJ(command->data)
        FREE_OBJ(command)
    }
}

/**
 * get a single IPMI whitelist command.
 *
 * @param commandOption
 * @param index             the index of whitelist command list
 * @param result
 * @return
 */
UtoolIPMICommand *getIpmiWhitelistCommand(UtoolCommandOption *commandOption, int index, UtoolResult *result)
{
    char *ipmiCmdOutput = NULL;
    char **segments = NULL;
    UtoolIPMIRawCmdOption *sendIpmiCommandOption = &(UtoolIPMIRawCmdOption) {0};

    UtoolIPMICommand *command = malloc(sizeof(UtoolIPMICommand));
    if (command == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    command->length = NULL;
    command->total = NULL;
    command->netfun = NULL;
    command->command = NULL;
    command->data = NULL;

    char getIpmiWhitelistCmd[MAX_IPMI_CMD_LEN] = {0};
    UtoolWrapSecFmt(getIpmiWhitelistCmd, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1,
        (vendorIdXFUSION ? GET_IPMI_WHITELIST_XFUSION : GET_IPMI_WHITELIST), index);
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

    // update command data part
    size_t ipmiCmdOutputLen = strnlen(ipmiCmdOutput, MAX_IPMI_CMD_LEN);
    int dataPartStartAt = SINGLE_BYTE_LEN * (DATA_PART_POS - 1) + 1;
    if (ipmiCmdOutputLen > dataPartStartAt) {
        int size = ipmiCmdOutputLen - dataPartStartAt;
        command->data = (char *) malloc(sizeof(char) * (size + 1));
        result->code = UtoolAssetMallocNotNull(command->data);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        errno_t ok = strncpy_s(command->data, size + 1, ipmiCmdOutput + dataPartStartAt, size);
        if (ok != EOK) {
            perror("Failed to `strncpy`.");
            exit(EXIT_SECURITY_ERROR);
        }
    }

    // split function will update orignal string
    segments = UtoolStringSplit(ipmiCmdOutput, ' ');
    if (segments == NULL) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    if (segments[3] != NULL) {
        command->total = (int) strtol(segments[3], NULL, 16);
    }

    if (segments[4] != NULL) {
        command->length = (int) strtol(segments[4], NULL, 16);

        if (command->length > 1) {
            char *netfun = segments[5];
            command->netfun = UtoolStringNDup(netfun, strnlen(netfun, MAX_COMMAND_NAME_LEN));
            result->code = UtoolAssetMallocNotNull(command->netfun);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (command->length > 2) {
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
    result->broken = 1;
    goto DONE;

DONE:
    FREE_OBJ(ipmiCmdOutput)
    UtoolStringFreeArrays(segments);
    return command;
}

/**
* get IPMI command white list for server internal channel (LPC/USB etc.), command handler for `getipmiwhitelist`
 *
 * Test tips:
 *      - whitelist enabled/disable
 *      - whitelist enabled, but no command
 *      - whitelist enabled, one command
 *      - whitelist enabled, multiple commands
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

    bool whitelistEnabled = false;
    UtoolIPMICommand *first = NULL;
    UtoolIPMICommand **whitelists = NULL;
    UtoolResult *vendorIdResult = &(UtoolResult) {0};

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

    vendorIdXFUSION = UtoolIPMIGetVendorId(commandOption, vendorIdResult);
    if (vendorIdResult->broken) {
        goto FAILURE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // get ipmi white list feature status
    sendIpmiCommandOption->data = vendorIdXFUSION ? GET_IPMI_WHITELIST_CONF_XFUSION : GET_IPMI_WHITELIST_CONF;
    ipmiCmdOutput = UtoolIPMIExecRawCommand(commandOption, sendIpmiCommandOption, result);
    ZF_LOGI("query IPMI whitelist status resp: %s", ipmiCmdOutput);
    if (result->broken) {
        goto FAILURE;
    }

    whitelistEnabled = UtoolStringEndsWith(ipmiCmdOutput, WHITELIST_ENABLED);
    ZF_LOGI("IPMI whitelist status: %s", whitelistEnabled ? ENABLED : DISABLED);
    FREE_OBJ(ipmiCmdOutput)

    // add Enabled output node
    cJSON *whitelistEnabledNode = cJSON_AddStringToObject(output, "Enable", whitelistEnabled ? ENABLED : DISABLED);
    result->code = UtoolAssetCreatedJsonNotNull(whitelistEnabledNode);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // if white list is enabled, we should get all whitelist commands
    if (whitelistEnabled) {
        first = getIpmiWhitelistCommand(commandOption, 1, result);
        if (result->broken) {
            goto FAILURE;
        }
        totalCount = first->total;
        ZF_LOGI("Total count of IPMI whitelist is: %d", totalCount);

        if (totalCount == 0) {
            // output to outputStr
            result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
            goto DONE;
        }

        if (totalCount == NULL || totalCount < 0 && totalCount > 0xFF) {
            result->code = UTOOLE_UNEXPECT_IPMITOOL_RESULT;
            goto FAILURE;
        }

        // pre-malloc whitelists
        whitelists = malloc(totalCount * sizeof(struct UtoolIPMICommand *));
        result->code = UtoolAssetMallocNotNull(whitelists);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        whitelists[0] = first;

        commands = cJSON_AddArrayToObject(output, "Command");
        result->code = UtoolAssetCreatedJsonNotNull(commands);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        for (int index = 2; index <= totalCount; index++) {
            UtoolIPMICommand *command = getIpmiWhitelistCommand(commandOption, index, result);
            // add whitelist command to whitelist
            whitelists[index - 1] = command;
            if (result->broken) {
                goto FAILURE;
            }
        }

        // we can not group netfunc or subfunc for now. so, we should just plain output as it is.
        for (int index = 0; index < totalCount; index++) {
            cJSON *item = cJSON_CreateObject();
            result->code = UtoolAssetCreatedJsonNotNull(item);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }

            UtoolIPMICommand *command = whitelists[index];
            if (command->netfun != NULL) {
                char netFunc[MAX_IPMI_CMD_LEN] = {0};
                UtoolWrapSecFmt(netFunc, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, "0x%s", command->netfun);
                cJSON *netfun = cJSON_AddStringToObject(item, "NetFunction", netFunc);
                result->code = UtoolAssetCreatedJsonNotNull(netfun);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            }

            if (command->command != NULL) {
                cJSON *cmdList = cJSON_AddArrayToObject(item, "CmdList");
                result->code = UtoolAssetCreatedJsonNotNull(cmdList);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }

                char commandName[MAX_IPMI_CMD_LEN] = {0};
                UtoolWrapSecFmt(commandName, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, "0x%s", command->command);
                cJSON *cmd = cJSON_CreateString(commandName);
                result->code = UtoolAssetCreatedJsonNotNull(cmd);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
                cJSON_bool succeed = cJSON_AddItemToArray(cmdList, cmd);
                if (!succeed) {
                    result->code = UTOOLE_CREATE_JSON_NULL;
                    goto FAILURE;
                }
            }

            // validate whether null.
            if (command->data != NULL) {
                char *data = command->data;
                // if netfun is 0x30
                if (UtoolStringCaseEquals(command->netfun, OEM_NETFUNC)) {
                    // if data starts with db 07 00, we should remove it.
                    if (UtoolStringCaseStartsWith(data, OEM_SUB_FUNC_PREFIX)) {
                        data = command->data + 9;
                    }
                }

                if (!UtoolStringCaseEquals(data, "ff")) {
                    char prefixedData[MAX_IPMI_CMD_LEN] = {0};
                    UtoolWrapSecFmt(prefixedData, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, "0x%s", data);

                    cJSON *dataNode = cJSON_CreateString(prefixedData);
                    result->code = UtoolAssetCreatedJsonNotNull(dataNode);
                    if (result->code != UTOOLE_OK) {
                        goto FAILURE;
                    }

                    cJSON *subFunctionList = cJSON_AddArrayToObject(item, "SubFunction");
                    result->code = UtoolAssetCreatedJsonNotNull(subFunctionList);
                    if (result->code != UTOOLE_OK) {
                        goto FAILURE;
                    }
                    cJSON_bool succeed = cJSON_AddItemToArray(subFunctionList, dataNode);
                    if (!succeed) {
                        result->code = UTOOLE_CREATE_JSON_NULL;
                        goto FAILURE;
                    }
                }
            }

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

    if (whitelistEnabled) {
        if (whitelists) {
            for (int index = 0; index < totalCount; index++) {
                UtoolIPMICommand *command = whitelists[index];
                if (command == NULL) {
                    break;
                }
                FreeIpmiCommand(command);
            }
            FREE_OBJ(whitelists)
        } else {
            FREE_OBJ(first)
        }
    }

    *outputStr = result->desc;
    return result->code;
}
