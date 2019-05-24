//
// Created by qianbiao on 5/8/19.
//
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
        "utool getcpu",
        NULL,
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

/**
 * command handler of `getfan`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetMemory(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getSystemResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getMemoryViewResp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,                   // output result json
            *memories = NULL,               // output memory array
            *memory = NULL,                 // output memory item
            *systemJson = NULL,             // curl get system response as json
            *memoryViewJson = NULL;         // curl get memory view response as json


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

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // initialize output memory array
    memories = cJSON_AddArrayToObject(output, "Information");
    ret = UtoolAssetCreatedJsonNotNull(memories);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // curl request get system
    ret = UtoolMakeCurlRequest(server, "/Systems/%s", HTTP_GET, NULL, NULL, getSystemResp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    if (getSystemResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getSystemResp, result);
        goto failure;
    }

    // process get system response
    systemJson = cJSON_Parse(getSystemResp->content);
    ret = UtoolAssetParseJsonNotNull(systemJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // mapping response json to output
    ret = UtoolMappingCJSONItems(systemJson, output, getMemorySummaryMapping);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // curl request get memory view
    ret = UtoolMakeCurlRequest(server, "/Systems/%s/MemoryView", HTTP_GET, NULL, NULL, getMemoryViewResp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    if (getMemoryViewResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getMemoryViewResp, result);
        goto failure;
    }

    // process get memory view response
    memoryViewJson = cJSON_Parse(getMemoryViewResp->content);
    ret = UtoolAssetParseJsonNotNull(memoryViewJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // mapping response json to output
    cJSON *member;
    cJSON *members = cJSON_GetObjectItem(memoryViewJson, "Information");
    cJSON_ArrayForEach(member, members) {
        cJSON *stateNode = cJSONUtils_GetPointer(member, "/Status/State");
        if (stateNode != NULL && UtoolStringEquals(MEMORY_STATE_ENABLED, stateNode->valuestring)) {
            memory = cJSON_CreateObject();
            ret = UtoolAssetCreatedJsonNotNull(memory);
            if (ret != UTOOLE_OK) {
                goto failure;
            }

            // create memory item and add it to array
            ret = UtoolMappingCJSONItems(member, memory, getMemoryMappings);
            if (ret != UTOOLE_OK) {
                goto failure;
            }
            cJSON_AddItemToArray(memories, memory);
        }
    }

    cJSON *maximumNode = cJSON_AddNumberToObject(output, "Maximum", cJSON_GetArraySize(memories));
    ret = UtoolAssetCreatedJsonNotNull(maximumNode);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(memory)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(systemJson)
    FREE_CJSON(memoryViewJson)
    UtoolFreeCurlResponse(getSystemResp);
    UtoolFreeCurlResponse(getMemoryViewResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
