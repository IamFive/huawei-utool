/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setvlan`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

static const char *OPT_ENABLED_ILLEGAL = "Error: option `enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_VLAN_ID_ILLEGAL = "Error: option `vlan-id` is illegal, value range should be: 1~4094";

static const char *const usage[] = {
        "setvlan [-e enabled] [-v vlan-id]",
        NULL,
};

typedef struct _SetVlanOption
{
    char *state;
    int frequency;
} UtoolSetVlanOption;

static cJSON *BuildPayload(UtoolSetVlanOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetVlanOption *option, UtoolResult *result);

/**
* set BMC NCSI port VLAN, command handler for `setvlan`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetVLAN(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL, *getEthernetInterfacesRespJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetVlanOption *option = &(UtoolSetVlanOption) {.frequency = DEFAULT_INT_V};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('e', "enabled", &(option->state),
                        "specifies whether VLAN is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_INTEGER('v', "vlan-id", &(option->frequency),
                        "specifies VLAN Id if enabled, range: 1~4094", NULL, 0, 0),
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
    if (result->interrupt) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    getEthernetInterfacesRespJson = result->data;

    cJSON *linkNode = cJSONUtils_GetPointer(getEthernetInterfacesRespJson, "/Members/0/@odata.id");
    char *url = linkNode->valuestring;

    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
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
    FREE_CJSON(getEthernetInterfacesRespJson)
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
static void ValidateSubcommandOptions(UtoolSetVlanOption *option, UtoolResult *result)
{

    if (!UtoolStringIsEmpty(option->state)) {
        if (!UtoolStringInArray(option->state, g_UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (option->frequency != DEFAULT_INT_V) {
        if (option->frequency < 1 || option->frequency > 4094) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_VLAN_ID_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->state) && option->frequency == DEFAULT_INT_V) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetVlanOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *vlan = cJSON_AddObjectToObject(payload, "VLAN");
    result->code = UtoolAssetCreatedJsonNotNull(vlan);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->state)) {
        cJSON *node = cJSON_AddBoolToObject(vlan, "VLANEnable", UtoolStringEquals(option->state, ENABLED));
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (option->frequency != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(vlan, "VLANId", option->frequency);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    return payload;

FAILURE:
    result->interrupt = 1;
    FREE_CJSON(payload)
    return NULL;
}