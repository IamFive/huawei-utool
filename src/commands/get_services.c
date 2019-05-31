//
// Created by qianbiao on 5/8/19.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"

static const char *const usage[] = {
        "utool getservice",
        NULL,
};

static const UtoolOutputMapping getProtocolMappings[] = {
        {.sourceXpath = "/ProtocolEnabled", .targetKeyValue="Enabled", .handle=UtoolBoolToEnabledPropertyHandler},
        {.sourceXpath = "/Port", .targetKeyValue="Port"},
        NULL
};

int walkThoughNodeToFindService(cJSON *serviceArray, cJSON *rootNode);

/**
 * command handler of `getservice`
 *
 * get BMC network protocol services
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetServices(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *output = NULL, *serviceRespJson = NULL, *serviceArray = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    serviceArray = cJSON_AddArrayToObject(output, "Service");
    result->code = UtoolAssetCreatedJsonNotNull(serviceArray);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishGet(server, "/Managers/%s/NetworkProtocol", NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    serviceRespJson = result->data;

    /** walk though ROOT node */
    result->code = walkThoughNodeToFindService(serviceArray, serviceRespJson->child);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    /** get oem node */
    cJSON *oem = cJSONUtils_GetPointer(serviceRespJson, "/Oem/Huawei");
    result->code = UtoolAssetJsonNodeNotNull(oem, "/Oem/Huawei");
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    /** walk though oem node */
    result->code = walkThoughNodeToFindService(serviceArray, oem->child);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    /** add SSDP node */
    cJSON *ssdp = cJSON_DetachItemFromObject(serviceRespJson, "SSDP");
    if (ssdp != NULL) {
        cJSON_AddItemToObject(output, "SSDP", ssdp);
        if (cJSON_IsObject(ssdp)) {
            cJSON_DeleteItemFromObject(ssdp, "ProtocolEnabled");
            cJSON_DeleteItemFromObject(ssdp, "Port");
        }
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(serviceRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}

int walkThoughNodeToFindService(cJSON *serviceArray, cJSON *rootNode)
{
    while (rootNode != NULL) {
        if (cJSON_IsObject(rootNode)) {
            if (cJSON_HasObjectItem(rootNode, "ProtocolEnabled") && cJSON_HasObjectItem(rootNode, "Port")) {
                cJSON *service = cJSON_CreateObject();
                int ret = UtoolAssetCreatedJsonNotNull(service);
                if (ret != UTOOLE_OK) {
                    return ret;
                }
                cJSON_AddItemToArray(serviceArray, service);
                cJSON *nameNode = cJSON_AddStringToObject(service, "Name", rootNode->string);
                ret = UtoolAssetCreatedJsonNotNull(nameNode);
                if (ret != UTOOLE_OK) {
                    return ret;
                }

                ret = UtoolMappingCJSONItems(rootNode, service, getProtocolMappings);
                if (ret != UTOOLE_OK) {
                    return ret;
                }
            }
        }
        rootNode = rootNode->next;
    }
}
