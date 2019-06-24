/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getservice`
* Author:
* Create: 2019-06-14
* Notes:
*/
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
#include "string_utils.h"

static const char *const usage[] = {
        "getservice",
        NULL,
};

static const UtoolOutputMapping getProtocolMappings[] = {
        {.sourceXpath = "/ProtocolEnabled", .targetKeyValue="Enabled", .handle=UtoolBoolToEnabledPropertyHandler},
        {.sourceXpath = "/Port", .targetKeyValue="Port"},
        NULL
};

static const UtoolOutputMapping getKVMIPSslEnabledMappings[] = {
        {.sourceXpath = "/EncryptionEnabled", .targetKeyValue="SSLEnabled", .handle=UtoolBoolToEnabledPropertyHandler},
        NULL
};

static const UtoolOutputMapping getVMSslEnabledMappings[] = {
        {.sourceXpath = "/Oem/Huawei/EncryptionEnabled", .targetKeyValue="SSLEnabled", .handle=UtoolBoolToEnabledPropertyHandler},
        NULL
};

int walkThoughNodeToFindService(cJSON *serviceArray, cJSON *rootNode);

/**
 * get BMC network protocol services, command handler of `getservice`
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
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    serviceArray = cJSON_AddArrayToObject(output, "Service");
    result->code = UtoolAssetCreatedJsonNotNull(serviceArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Managers/%s/NetworkProtocol", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    serviceRespJson = result->data;

    /** walk though ROOT node */
    result->code = walkThoughNodeToFindService(serviceArray, serviceRespJson->child);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    /** get oem node */
    cJSON *oem = cJSONUtils_GetPointer(serviceRespJson, "/Oem/Huawei");
    result->code = UtoolAssetJsonNodeNotNull(oem, "/Oem/Huawei");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    /** walk though oem node */
    result->code = walkThoughNodeToFindService(serviceArray, oem->child);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    /** process special properties */
    cJSON *service = NULL;
    cJSON_ArrayForEach(service, serviceArray) {
        cJSON *name = cJSON_GetObjectItem(service, "Name");

        /** IPMI has port2 property */
        if (UtoolStringEquals(name->valuestring, "IPMI")) {
            cJSON *ipmiPort2Node = cJSONUtils_GetPointer(oem, "/IPMI/Port2");
            cJSON_AddItemReferenceToObject(service, "Port2", ipmiPort2Node);
            continue;
        }

        /** KVMIP should Load ssl-enabled through another API */
        if (UtoolStringEquals(name->valuestring, "KVMIP")) {
            UtoolRedfishGet(server, "/Managers/%s/KvmService", service, getKVMIPSslEnabledMappings, result);
            if (result->broken) {
                goto FAILURE;
            }
            FREE_CJSON(result->data)
        }

        /** VirtualMedia should Load ssl-enabled through another API */
        if (UtoolStringEquals(name->valuestring, "VirtualMedia")) {
            UtoolRedfishGet(server, "/Managers/%s/VirtualMedia/CD", service, getVMSslEnabledMappings, result);
            if (result->broken) {
                goto FAILURE;
            }
            FREE_CJSON(result->data)
        }
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
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
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
