//
// Created by qianbiao on 5/8/19.
//
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

static const char *OPT_ENABLED_ILLEGAL = "Error: option `Enabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_VLAN_ID_ILLEGAL = "Error: option `VLANID` is illegal, value range should be: 1~4094";

static const char *const usage[] = {
        "utool setvlan [-e Enabled] [-v VLANID]",
        NULL,
};

typedef struct _SetVlanOption
{
    char *enabled;
    int vlanId;
} UtoolSetVlanOption;

static cJSON *BuildPayload(UtoolSetVlanOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetVlanOption *option, UtoolResult *result);

/**
* set BMC NCSI port  VLAN, command handler for `setvlan`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetVLAN(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL, *payload = NULL, *getEthernetInterfacesRespJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetVlanOption *option = &(UtoolSetVlanOption) {.vlanId = DEFAULT_INT_V, 0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('e', "Enabled", &(option->enabled),
                        "specifies whether VLAN is enabled, available choices: {Enabled, Disabled}", NULL, 0, 0),
            OPT_INTEGER('v', "VLANID", &(option->vlanId),
                        "specifies VLAN Id if enabled, range: 1~4094", NULL, 0, 0),
            OPT_END()
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ValidateSubcommandOptions(option, result);
    if (result->interrupt) {
        goto done;
    }

    // build payload
    payload = BuildPayload(option, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    getEthernetInterfacesRespJson = result->data;

    cJSON *linkNode = cJSONUtils_GetPointer(getEthernetInterfacesRespJson, "/Members/0/@odata.id");
    char *url = linkNode->valuestring;

    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(payload)
    FREE_CJSON(getEthernetInterfacesRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for setpwd command
*
* @param option
* @param result
* @return
*/
static void ValidateSubcommandOptions(UtoolSetVlanOption *option, UtoolResult *result)
{

    if (!UtoolStringIsEmpty(option->enabled)) {
        if (!UtoolStringInArray(option->enabled, UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto failure;
        }
    }

    if (option->vlanId != DEFAULT_INT_V) {
        if (option->vlanId < 1 || option->vlanId > 4094) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_VLAN_ID_ILLEGAL),
                                                  &(result->desc));
            goto failure;
        }
    }

    if (UtoolStringIsEmpty(option->enabled) && option->vlanId == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto failure;
    }

    return;

failure:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetVlanOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    cJSON *vlan = cJSON_AddObjectToObject(payload, "VLAN");
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    if (!UtoolStringIsEmpty(option->enabled)) {
        cJSON *node = cJSON_AddBoolToObject(vlan, "VLANEnable", UtoolStringEquals(option->enabled, ENABLED));
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }
    }

    if (option->vlanId != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(vlan, "VLANId", option->vlanId);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }
    }

    return payload;

failure:
    FREE_CJSON(payload)
    return NULL;
}