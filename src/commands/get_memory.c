/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getmemory`
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
#include "string_utils.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"

static const char *const usage[] = {
        "getmemory",
        NULL,
};

static const UtoolOutputMapping getPowerWattsMapping[] = {
        {.sourceXpath = "/PowerControl/0/Oem/Huawei/PowerMetricsExtended/CurrentMemoryPowerWatts",
                .targetKeyValue="TotalPowerWatts"},
        NULL
};

static const UtoolOutputMapping getMemorySummaryMapping[] = {
        {.sourceXpath = "/MemorySummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/MemorySummary/TotalSystemMemoryGiB", .targetKeyValue="TotalSystemMemoryGiB"},
        NULL
};

static const UtoolOutputMapping getMemoryMappings[] = {
        {.sourceXpath = "/DeviceLocator", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/CapacityMiB", .targetKeyValue="CapacityMiB"},
        {.sourceXpath = "/OperatingSpeedMhz", .targetKeyValue="OperatingSpeedMhz"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/MemoryDeviceType", .targetKeyValue="MemoryDeviceType"},
        {.sourceXpath = "/DataWidthBits", .targetKeyValue="DataWidthBits"},
        {.sourceXpath = "/RankCount", .targetKeyValue="RankCount"},
        {.sourceXpath = "/PartNumber", .targetKeyValue="PartNumber"},
        {.sourceXpath = "/Technology", .targetKeyValue="Technology"},
        {.sourceXpath = "/MinVoltageMillivolt", .targetKeyValue="MinVoltageMillivolt"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};

static const UtoolOutputMapping getAbsentMemoryMappings[] = {
        {.sourceXpath = "/DeviceLocator", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        NULL
};


/**
 * command handler of `getmemory`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetMemory(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    // initialize output objects
    cJSON *output = NULL,                   // output result json
            *memories = NULL,               // output memory array
            *memory = NULL,                 // output memory item
            *memoryViewJson = NULL;         // curl get memory view response as json


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

    UtoolRedfishGet(server, "/Systems/%s", output, getMemorySummaryMapping, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)


    UtoolRedfishGet(server, "/Systems/%s/MemoryView", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    memoryViewJson = result->data;

    // mapping response json to output
    cJSON *member;
    cJSON *members = cJSON_GetObjectItem(memoryViewJson, "Information");
    cJSON *maximumNode = cJSON_AddNumberToObject(output, "Maximum", cJSON_GetArraySize(members));
    result->code = UtoolAssetCreatedJsonNotNull(maximumNode);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Chassis/%s/Power", output, getPowerWattsMapping, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // initialize output memory array
    memories = cJSON_AddArrayToObject(output, "Information");
    result->code = UtoolAssetCreatedJsonNotNull(memories);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON_ArrayForEach(member, members) {
        memory = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(memory);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *stateNode = cJSONUtils_GetPointer(member, "/Status/State");
        if (stateNode != NULL && UtoolStringEquals(MEMORY_STATE_ENABLED, stateNode->valuestring)) {
            result->code = UtoolMappingCJSONItems(member, memory, getMemoryMappings);
        }
        else {
            result->code = UtoolMappingCJSONItems(member, memory, getAbsentMemoryMappings);
        }

        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(memories, memory);
    }

    // output to result
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(memory)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(memoryViewJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
