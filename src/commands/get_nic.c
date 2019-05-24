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
        "utool getnic",
        NULL,
};

static int FirmwareTypeHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    // it seems firmware name is not solid enough to parse Type
    // UtoolStringEndsWith(node->valuestring, "BMC");
    // we will try to parse Type from Software-Id
    char *type = strtok(node->valuestring, "-");
    cJSON *newNode = cJSON_AddStringToObject(target, key, type);
    FREE_CJSON(node)
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getNetworkAdapterSummaryMapping[] = {
        {.sourceXpath = "/Oem/Huawei/NetworkAdaptersSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/Huawei/NetworkAdaptersSummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static const UtoolOutputMapping getNetworkAdapterMappings[] = {
        {.sourceXpath = "/Oem/Huawei/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Null", .targetKeyValue="Serialnumber"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};

static const UtoolOutputMapping getControllerMapping[] = {
        {.sourceXpath = "/Controllers/0/FirmwarePackageVersion", .targetKeyValue="FirmwareVersion"},
        {.sourceXpath = "/Controllers/0/ControllerCapabilities/NetworkPortCount", .targetKeyValue="PortCount"},
        NULL
};

static int LoadNetworkController(cJSON *output, cJSON *networkAdapterJson, cJSON *networkAdapter);


/**
 * command handler of `getnic`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetNIC(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getChassisResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getNetworkAdaptersResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getNetworkAdapterResp = &(UtoolCurlResponse) {0};

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    // initialize output objects
    cJSON *networkAdaptersJson, *networkAdapterJson = NULL, *chassisJson = NULL;
    cJSON *output = NULL, *networkAdapterList = NULL, *networkAdapter = NULL;

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    networkAdapterList = cJSON_AddArrayToObject(output, "NIC");
    ret = UtoolAssetCreatedJsonNotNull(networkAdapterList);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // get chassis to mapping summary info
    ret = UtoolMakeCurlRequest(server, "/Chassis/%s", HTTP_GET, NULL, NULL, getChassisResp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (getChassisResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisResp, result);
        goto failure;
    }

    // process get chassis response
    chassisJson = cJSON_Parse(getChassisResp->content);
    ret = UtoolAssetParseJsonNotNull(chassisJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    ret = UtoolMappingCJSONItems(chassisJson, output, getNetworkAdapterSummaryMapping);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // get network adapters
    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/NetworkAdapters", HTTP_GET, NULL, NULL, getNetworkAdaptersResp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (getNetworkAdaptersResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getNetworkAdaptersResp, result);
        goto failure;
    }

    // process get network adapters response
    networkAdaptersJson = cJSON_Parse(getNetworkAdaptersResp->content);
    ret = UtoolAssetParseJsonNotNull(networkAdaptersJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }


    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(networkAdaptersJson, "Members");
    cJSON_ArrayForEach(member, members) {
        cJSON *linkNode = cJSON_GetObjectItem(member, "@odata.id");
        ret = UtoolAssetJsonNodeNotNull(linkNode, "/Members/*/@odata.id");
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        char *url = linkNode->valuestring;
        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getNetworkAdapterResp);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        if (getNetworkAdapterResp->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getNetworkAdapterResp, result);
            goto failure;
        }

        networkAdapterJson = cJSON_Parse(getNetworkAdapterResp->content);
        ret = UtoolAssetParseJsonNotNull(networkAdapterJson);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        networkAdapter = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(networkAdapter);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create networkAdapter item and add it to array
        ret = UtoolMappingCJSONItems(networkAdapterJson, networkAdapter, getNetworkAdapterMappings);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // load controller information
        ret = LoadNetworkController(networkAdapterJson, networkAdapter, NULL);

        // add network adapter to array
        cJSON_AddItemToArray(networkAdapterList, networkAdapter);



        // free memory
        FREE_CJSON(networkAdapterJson)
        UtoolFreeCurlResponse(getNetworkAdapterResp);
    }



    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(networkAdapter)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(chassisJson)
    FREE_CJSON(networkAdapterJson)
    UtoolFreeCurlResponse(getChassisResp);
    UtoolFreeCurlResponse(getNetworkAdapterResp);
    UtoolFreeRedfishServer(server);
    return ret;
}

static int LoadNetworkController(cJSON *output, cJSON *networkAdapterJson, cJSON *networkAdapter)
{


    return 0;
}
