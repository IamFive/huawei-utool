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
        {.sourceXpath = "/Controllers", .targetKeyValue="Controller"},
        NULL
};

static const UtoolOutputMapping getControllerMapping[] = {
        {.sourceXpath = "/Null", .targetKeyValue="Id"},
        {.sourceXpath = "/Oem/Huawei/CardManufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Oem/Huawei/CardModel", .targetKeyValue="Model"},
        {.sourceXpath = "/Null", .targetKeyValue="Serialnumber"},
        {.sourceXpath = "/Controllers/0/FirmwarePackageVersion", .targetKeyValue="FirmwareVersion"},
        {.sourceXpath = "/Controllers/0/ControllerCapabilities/NetworkPortCount", .targetKeyValue="PortCount"},
        NULL
};

static const UtoolOutputMapping getNetworkPortMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/AssociatedNetworkAddresses/0", .targetKeyValue="MACAddress"},
        {.sourceXpath = "/LinkStatus", .targetKeyValue="LinkStatus"},
        NULL
};


static int LoadNetworkController(cJSON *output, cJSON *networkAdapterJson, cJSON *networkAdapter);

/**
 * command handler of `getnic`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetNIC(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getChassisResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getNetworkAdaptersResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getNetworkAdapterResp = &(UtoolCurlResponse) {0};


    /** initialize output objects */
    cJSON *networkAdapterMembersJson, *networkAdapterJson = NULL, *chassisJson = NULL;
    cJSON *output = NULL, *networkAdapterArray = NULL, *networkAdapter = NULL;

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishGet(server, "/Chassis/%s", output, getNetworkAdapterSummaryMapping, result);
    if (result->interrupt) {
        goto failure;
    }
    chassisJson = result->data;

    UtoolRedfishGet(server, "/Chassis/%s/NetworkAdapters", NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    networkAdapterMembersJson = result->data;

    networkAdapterArray = cJSON_AddArrayToObject(output, "NIC");
    ret = UtoolAssetCreatedJsonNotNull(networkAdapterArray);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    /** processing all network adapters */
    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(networkAdapterMembersJson, "Members");
    cJSON_ArrayForEach(member, members) {
        networkAdapter = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(networkAdapter);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }

        cJSON *linkNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = linkNode->valuestring;
        UtoolRedfishGet(server, url, networkAdapter, getNetworkAdapterMappings, result);
        if (result->interrupt) {
            goto failure;
        }
        networkAdapterJson = result->data;

        cJSON *controllerArray = cJSONUtils_GetPointer(networkAdapter, "/Controller");
        cJSON_DeleteItemFromArray(controllerArray, 0);
        cJSON *outputController = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(outputController);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }
        cJSON_AddItemToArray(controllerArray, outputController);

        result->code = UtoolMappingCJSONItems(networkAdapterJson, outputController, getControllerMapping);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }

        /** add ports to controller */
        cJSON *ports = cJSON_AddArrayToObject(outputController, "Port");
        result->code = UtoolAssetCreatedJsonNotNull(ports);
        if (result->code != UTOOLE_OK) {
            goto failure;
        }

        /** load all network ports */
        cJSON *networkPortMembers = cJSONUtils_GetPointer(networkAdapterJson, "/Controllers/0/Link/NetworkPorts");
        UtoolRedfishGetMemberResources(server, networkPortMembers, ports, getNetworkPortMappings, result);
        if (result->interrupt) {
            goto failure;
        }

        cJSON_AddItemToArray(networkAdapterArray, networkAdapter);

        /** free memory */
        FREE_CJSON(networkAdapterJson)
    }

    // output to &(result->desc)
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto done;

failure:
    FREE_CJSON(networkAdapterJson)
    FREE_CJSON(networkAdapter)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(chassisJson)
    FREE_CJSON(networkAdapterMembersJson)
    UtoolFreeCurlResponse(getChassisResp);
    UtoolFreeCurlResponse(getNetworkAdapterResp);
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
