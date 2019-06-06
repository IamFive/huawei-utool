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


static const char *const usage[] = {
        "utool setadaptiveport -p PortList",
        NULL,
};

typedef struct _SetAdaptivePortOption
{
    char *adaptivePortListStr;
} UtoolSetAdaptivePortOption;

static cJSON *BuildPayload(UtoolSetAdaptivePortOption *option, UtoolResult *result);

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
    cJSON *payload = NULL, *getEthernetInterfacesRespJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetAdaptivePortOption *option = &(UtoolSetAdaptivePortOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('p', "PortList", &(option->adaptivePortListStr),
                        "specifies adaptive port list in JSON format, "
                        "example: [{NIC=Dedicated,Port=0},{NIC=MEZZ,Port=1}]", NULL, 0, 0),
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
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
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


    // TODO build payload
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
static void ValidateSubcommandOptions(UtoolSetAdaptivePortOption *option, UtoolResult *result)
{
    if (UtoolStringIsEmpty(option->adaptivePortListStr)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED(PortList)),
                                              &(result->desc));
        goto FAILURE;
    }

    cJSON *adaptivePortArray = cJSON_Parse(option->adaptivePortListStr);
    if (adaptivePortArray != NULL && cJSON_IsArray(adaptivePortArray)) {
        if (cJSON_GetArraySize(adaptivePortArray) > 0) {
            cJSON *adaptivePort = NULL;
            cJSON_ArrayForEach(adaptivePort, adaptivePortArray) {
                cJSON *nicNode = cJSON_GetObjectItem(adaptivePort, "NIC");
                nicNode->string = "Type";
                //cJSON_HasObjectItem(adaptivePort, "Port");
            }
        }
    }

    //fprintf(stdout, "%s", cJSON_Print(adaptivePortArray));
    //result->code = UtoolAssetParseJsonNotNull(node);

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetAdaptivePortOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *vlan = cJSON_AddObjectToObject(payload, "VLAN");
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    //if (!UtoolStringIsEmpty(option->state)) {
    //    cJSON *node = cJSON_AddBoolToObject(vlan, "VLANEnable", UtoolStringEquals(option->state, ENABLED));
    //    result->code = UtoolAssetCreatedJsonNotNull(node);
    //    if (result->code != UTOOLE_OK) {
    //        goto FAILURE;
    //    }
    //}
    //
    //if (option->frequency != DEFAULT_INT_V) {
    //    cJSON *node = cJSON_AddNumberToObject(vlan, "VLANId", option->frequency);
    //    result->code = UtoolAssetCreatedJsonNotNull(node);
    //    if (result->code != UTOOLE_OK) {
    //        goto FAILURE;
    //    }
    //}

    return payload;

FAILURE:
    result->interrupt = 1;
    FREE_CJSON(payload)
    return NULL;
}