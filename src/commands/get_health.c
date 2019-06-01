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
#include "string_utils.h"

static const char *const usage[] = {
        "utool getraid",
        NULL,
};


static const UtoolOutputMapping getSystemSummaryMappings[] = {
        {.sourceXpath = "/Status/Health", .targetKeyValue="System"},
        {.sourceXpath = "/ProcessorSummary/Status/HealthRollup", .targetKeyValue="CPU"},
        {.sourceXpath = "/MemorySummary/Status/HealthRollup", .targetKeyValue="Memory"},
        {.sourceXpath = "/Oem/Huawei/StorageSummary/Status/HealthRollup", .targetKeyValue="Storage"},
        NULL
};

static const UtoolOutputMapping getChassisSummaryMappings[] = {
        {.sourceXpath = "/Oem/Huawei/NetworkAdaptersSummary/Status/HealthRollup", .targetKeyValue="Network"},
        {.sourceXpath = "/Oem/Huawei/PowerSupplySummary/Status/HealthRollup", .targetKeyValue="PSU"},
        NULL
};

static const UtoolOutputMapping getThermalSummaryMappings[] = {
        {.sourceXpath = "/Oem/Huawei/FanSummary/Status/HealthRollup", .targetKeyValue="Fan"},
        NULL
};

/**
 * get server overall health and component health, command handler of `gethealth`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetHealth(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *output = NULL;

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

    UtoolRedfishGet(server, "/Systems/%s", output, getSystemSummaryMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolRedfishGet(server, "/Chassis/%s", output, getChassisSummaryMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolRedfishGet(server, "/Chassis/%s/Thermal", output, getThermalSummaryMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
