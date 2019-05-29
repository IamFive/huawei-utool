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
        "utool getcpu",
        NULL,
};

static const UtoolOutputMapping getProcessorSummaryMapping[] = {
        {.sourceXpath = "/ProcessorSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/ProcessorSummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static const UtoolOutputMapping getProcessorMappings[] = {
        {.sourceXpath = "/DeviceLocator", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/L1CacheKiB", .targetKeyValue="L1CacheKiB"},
        {.sourceXpath = "/L2CacheKiB", .targetKeyValue="L2CacheKiB"},
        {.sourceXpath = "/L3CacheKiB", .targetKeyValue="L3CacheKiB"},
        {.sourceXpath = "/Temperature", .targetKeyValue="Temperature"},
        {.sourceXpath = "/EnabledSetting", .targetKeyValue="EnabledSetting"},
        {.sourceXpath = "/ProcessorType", .targetKeyValue="ProcessorType"},
        {.sourceXpath = "/ProcessorArchitecture", .targetKeyValue="ProcessorArchitecture"},
        {.sourceXpath = "/InstructionSet", .targetKeyValue="InstructionSet"},
        {.sourceXpath = "/MaxSpeedMHz", .targetKeyValue="MaxSpeedMHz"},
        {.sourceXpath = "/TotalCores", .targetKeyValue="TotalCores"},
        {.sourceXpath = "/TotalThreads", .targetKeyValue="TotalThreads"},
        {.sourceXpath = "/Socket", .targetKeyValue="Socket"},
        {.sourceXpath = "/IdentificationRegisters", .targetKeyValue="PPIN"},
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
int UtoolCmdGetProcessor(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getSystemResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getProcessorViewResp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,                   // output result json
            *processors = NULL,             // output processor array
            *processor = NULL,              // output processor item
            *systemJson = NULL,             // curl get system response as json
            *processorViewJson = NULL;      // curl get processor view response as json


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
    ret = UtoolMappingCJSONItems(systemJson, output, getProcessorSummaryMapping);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // initialize output processor array
    processors = cJSON_AddArrayToObject(output, "Information");
    ret = UtoolAssetCreatedJsonNotNull(processors);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // curl request get processor view
    ret = UtoolMakeCurlRequest(server, "/Systems/%s/ProcessorView", HTTP_GET, NULL, NULL, getProcessorViewResp);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    if (getProcessorViewResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getProcessorViewResp, result);
        goto failure;
    }

    // process get processor view response
    processorViewJson = cJSON_Parse(getProcessorViewResp->content);
    ret = UtoolAssetParseJsonNotNull(processorViewJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // mapping response json to output
    cJSON *member;
    cJSON *members = cJSON_GetObjectItem(processorViewJson, "Information");
    cJSON_ArrayForEach(member, members) {
        processor = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(processor);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create processor item and add it to array
        ret = UtoolMappingCJSONItems(member, processor, getProcessorMappings);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
        cJSON_AddItemToArray(processors, processor);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(processor)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(systemJson)
    FREE_CJSON(processorViewJson)
    UtoolFreeCurlResponse(getSystemResp);
    UtoolFreeCurlResponse(getProcessorViewResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
