/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler of `getcpu`
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
        "getcpu",
        NULL,
};

static const UtoolOutputMapping getPowerWattsMapping[] = {
        {.sourceXpath = "/PowerControl/0/Oem/Huawei/PowerMetricsExtended/CurrentCPUPowerWatts",
                .targetKeyValue="TotalPowerWatts"},
        NULL
};


static const UtoolOutputMapping getProcessorSummaryMapping[] = {
        {.sourceXpath = "/ProcessorSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/ProcessorSummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static const UtoolOutputMapping getProcessorMappings[] = {
        {.sourceXpath = "/Oem/Huawei/DeviceLocator", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Oem/Huawei/L1CacheKiB", .targetKeyValue="L1CacheKiB"},
        {.sourceXpath = "/Oem/Huawei/L2CacheKiB", .targetKeyValue="L2CacheKiB"},
        {.sourceXpath = "/Oem/Huawei/L3CacheKiB", .targetKeyValue="L3CacheKiB"},
        {.sourceXpath = "/Oem/Huawei/Temperature", .targetKeyValue="Temperature"},
        {.sourceXpath = "/Oem/Huawei/EnabledSetting", .targetKeyValue="EnabledSetting"},
        {.sourceXpath = "/ProcessorType", .targetKeyValue="ProcessorType"},
        {.sourceXpath = "/ProcessorArchitecture", .targetKeyValue="ProcessorArchitecture"},
        {.sourceXpath = "/InstructionSet", .targetKeyValue="InstructionSet"},
        {.sourceXpath = "/MaxSpeedMHz", .targetKeyValue="MaxSpeedMHz"},
        {.sourceXpath = "/TotalCores", .targetKeyValue="TotalCores"},
        {.sourceXpath = "/TotalThreads", .targetKeyValue="TotalThreads"},
        {.sourceXpath = "/Socket", .targetKeyValue="Socket"},
        {.sourceXpath = "/Oem/Huawei/SerialNumber", .targetKeyValue="PPIN"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};


/**
 * command handler of `getcpu`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetProcessor(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    // initialize output objects
    cJSON *output = NULL,                   // output outputStr json
            *processorMembers = NULL,
            *processors = NULL;             // output processor array


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
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s", output, getProcessorSummaryMapping, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolRedfishGet(server, "/Chassis/%s/Power", output, getPowerWattsMapping, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)


    // initialize output processor array
    processors = cJSON_AddArrayToObject(output, "CPU");
    ret = UtoolAssetCreatedJsonNotNull(processors);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s/Processors", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    processorMembers = result->data;

    /** load all network ports */
    UtoolRedfishGetMemberResources(server, processorMembers, processors, getProcessorMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(processorMembers)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
