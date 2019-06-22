/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler of `getpcie`
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

static const char *const usage[] = {
        "getpcie",
        NULL,
};

static const UtoolOutputMapping getPCIeDeviceMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Oem/Huawei/AssociatedResource", .targetKeyValue="Type"},
        {.sourceXpath = "/Oem/Huawei/BusNumber", .targetKeyValue="SlotBus"},
        {.sourceXpath = "/Oem/Huawei/DeviceNumber", .targetKeyValue="SlotDevice"},
        {.sourceXpath = "/Oem/Huawei/FunctionNumber", .targetKeyValue="SlotFunction"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};


/**
 * get all pcie device information, command handler of `getpcie`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetPCIe(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *systemRespJson = NULL,
            *output = NULL,
            *pcieDeviceArray = NULL;

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

    pcieDeviceArray = cJSON_CreateArray();
    result->code = UtoolAssetCreatedJsonNotNull(pcieDeviceArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    systemRespJson = result->data;

    cJSON *members = cJSONUtils_GetPointer(systemRespJson, "/PCIeDevices");
    result->code = UtoolAssetJsonNodeNotNull(members, "/PCIeDevices");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGetMemberResources(server, members, pcieDeviceArray, getPCIeDeviceMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // add output properties
    // calculate maximum count
    cJSON *maximum = cJSON_AddNumberToObject(output, "Maximum", cJSON_GetArraySize(pcieDeviceArray));
    result->code = UtoolAssetCreatedJsonNotNull(maximum);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *overallHealth = cJSON_AddStringToObject(output, "OverallHealth", UtoolGetOverallHealth(pcieDeviceArray, "/Health"));
    result->code = UtoolAssetCreatedJsonNotNull(overallHealth);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON_AddItemToObject(output, "PCIeDevice", pcieDeviceArray);

    // output to result
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(pcieDeviceArray)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(systemRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
