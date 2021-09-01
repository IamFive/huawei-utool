/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setadaptiveport`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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


static const char *OPT_PORT_LIST_ILLEGAL = "Error: option `port-list` is illegal, "
                                           "it must be formed with: NIC1,PORT1;[NIC2,PORT2;...]. "
                                           "Allowable NIC type: Dedicated, Aggregation, LOM, ExternalPCIe, LOM2.";

static const char *OPT_EMPTY_ALLOWABLE_PORT = "Error: could not load allowable adaptive ports.";
static const char *OPT_PORT_NOT_EXISTS = "Error: your input `%s` does not match any allowable port.";

static const char *const usage[] = {
        "setadaptiveport -p port-list",
        NULL,
};

typedef struct _SetAdaptivePortOption
{
    char *adaptivePortListStr;
} UtoolSetAdaptivePortOption;

static cJSON *BuildPayload(UtoolRedfishServer *server, cJSON *ethernet, UtoolSetAdaptivePortOption *option,
                           UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetAdaptivePortOption *option, UtoolResult *result);

/**
* set adaptive port for management port, command handler for `setadaptiveport`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetAdaptivePort(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL,
            *getEthernetRespJson = NULL,
            *getEthernetsRespJson = NULL;


    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetAdaptivePortOption *option = &(UtoolSetAdaptivePortOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('p', "port-list", &(option->adaptivePortListStr),
                        "specifies adaptive ports, format: NIC1,PORT1;[NIC2,PORT2;...], example: 'Dedicated,0;LOM,1;'."
                        " Allowable NIC type: Dedicated, Aggregation, LOM, ExternalPCIe, LOM2.", NULL, 0, 0),
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

    ValidateSubcommandOptions(option, result);
    if (result->broken) {
        goto DONE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    getEthernetsRespJson = result->data;

    cJSON *linkNode = cJSONUtils_GetPointer(result->data, "/Members/0/@odata.id");
    result->code = UtoolAssetJsonNodeNotNull(linkNode, "/Members/0/@odata.id");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }
    char *url = linkNode->valuestring;


    /* load all allowed ports */
    UtoolRedfishGet(server, url, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    getEthernetRespJson = result->data;

    // build payload
    payload = BuildPayload(server, getEthernetRespJson, option, result);
    if (result->broken) {
        goto FAILURE;
    }

    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(getEthernetRespJson)
    FREE_CJSON(getEthernetsRespJson)
    UtoolFreeRedfishServer(server);

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
static void ValidateSubcommandOptions(UtoolSetAdaptivePortOption *option, UtoolResult *result)
{
    if (UtoolStringIsEmpty(option->adaptivePortListStr)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("port-list")),
                                              &(result->desc));
        goto FAILURE;
    }

    /* because glibc is forbidden by Huawei, so regex.h is forbidden too. */
    /* try to validate user input format
    regex_t regex;
    int invalid = regcomp(&regex, "^((Dedicated|Aggregation|LOM|ExternalPCIe|LOM2),[0-9]+;)+$", REG_EXTENDED);
    if (invalid) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    int ret = regexec(&regex, option->adaptivePortListStr, 0, NULL, 0);
    if (ret == REG_NOERROR) {
        goto DONE;
    }
    else if (ret == REG_NOMATCH) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PORT_LIST_ILLEGAL),
                                              &(result->desc));
        goto FAILURE;
    }
    else {
        char msg[128];
        regerror(ret, &regex, msg, sizeof(msg));
        ZF_LOGE("Port List regex match failed, reason -> %s", msg);
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(msg), &(result->desc));
        goto FAILURE;
    }*/

    goto DONE;

FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    return;
    /* regfree(&regex); */
}

static cJSON *BuildPayload(UtoolRedfishServer *server, cJSON *ethernet, UtoolSetAdaptivePortOption *option,
                           UtoolResult *result)
{
    cJSON *payload = NULL;
    char *inputPorts = NULL;
    char **selectedPortStrArray = NULL;
    char *nextp = NULL;

    inputPorts = strdup(option->adaptivePortListStr);
    if (!inputPorts) {
        result->code = UTOOLE_INTERNAL;
        goto FAILURE;
    }

    payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *mode = cJSON_AddStringToObject(payload, "NetworkPortMode", "Automatic");
    result->code = UtoolAssetCreatedJsonNotNull(mode);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *adaptivePortArray = cJSON_AddArrayToObject(payload, "AdaptivePort");
    result->code = UtoolAssetCreatedJsonNotNull(mode);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }


    /* load & validate allowable ports */
    cJSON *allowableValues = UtoolGetOemNode(server, ethernet, "ManagementNetworkPort@Redfish.AllowableValues");
    if (!(cJSON_IsArray(allowableValues) && cJSON_GetArraySize(allowableValues) > 0)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_EMPTY_ALLOWABLE_PORT),
                                              &(result->desc));
        goto FAILURE;
    }


    /* parse & validate user input */
    selectedPortStrArray = UtoolStringSplit(inputPorts, ';');


    for (int idx = 0; *(selectedPortStrArray + idx); idx++) {
        char *selectedPortStr = *(selectedPortStrArray + idx);
        char msg[MAX_FAILURE_MSG_LEN];
        UtoolWrapSnprintf(msg, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN - 1, OPT_PORT_NOT_EXISTS, selectedPortStr);

        char *nic = UtoolStringTokens(selectedPortStr, ",", &nextp);
        char *left = UtoolStringTokens(NULL, ",", &nextp);
        if (!UtoolStringIsNumeric(left)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PORT_LIST_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        long p = strtol(left, NULL, 0);
        bool matched = false;
        cJSON *allowableValue = NULL;
        cJSON_ArrayForEach(allowableValue, allowableValues) {
            cJSON *portType = cJSON_GetObjectItem(allowableValue, "Type");
            cJSON *portNumber = cJSON_GetObjectItem(allowableValue, "PortNumber");

            /* no expect allowable ports format */
            if (!(cJSON_IsString(portType) && cJSON_IsNumber(portNumber))) {
                result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_EMPTY_ALLOWABLE_PORT),
                                                      &(result->desc));
                goto FAILURE;
            }

            if (UtoolStringEquals(nic, portType->valuestring) && portNumber->valueint == p) {
                matched = true;

                cJSON *cloned = cJSON_Duplicate(allowableValue, 1);
                result->code = UtoolAssetCreatedJsonNotNull(cloned);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }

                cJSON_AddItemToArray(adaptivePortArray, cloned);

                cJSON_DeleteItemFromObject(cloned, "LinkStatus");
                cJSON *flag = cJSON_AddBoolToObject(cloned, "AdaptiveFlag", cJSON_True);
                result->code = UtoolAssetCreatedJsonNotNull(flag);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
                break;
            }
        }

        if (!matched) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(msg), &(result->desc));
            goto FAILURE;
        }
    }

    cJSON *wrapped = UtoolWrapOem(server->oemName, payload, result);
    if (result->broken) {
        goto FAILURE;
    }
    payload = wrapped;

    goto DONE;


FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    goto DONE;

DONE:
    free(inputPorts);
    if (selectedPortStrArray != NULL) {
        for (int idx = 0; *(selectedPortStrArray + idx); idx++) {
            free(*(selectedPortStrArray + idx));
        }
        free(selectedPortStrArray);
    }
    return payload;
}