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
        "utool getfan",
        NULL,
};

static const UtoolOutputMapping getFanSummaryMapping[] = {
        {.sourceXpath = "/Oem/Huawei/FanSummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/Huawei/FanSummary/Count", .targetKeyValue="Maximum"},
        //{.sourceXpath = "/", .targetKeyValue="Model"},
        {.sourceXpath = "/Oem/Huawei/FanSpeedLevelPercents", .targetKeyValue="FanSpeedLevelPercents"},
        {.sourceXpath = "/Oem/Huawei/FanSpeedAdjustmentMode", .targetKeyValue="FanSpeedAdjustmentMode"},
        {.sourceXpath = "/Oem/Huawei/FanTotalPowerWatts", .targetKeyValue="FanTotalPowerWatts"},
        {.sourceXpath = "/Oem/Huawei/FanManualModeTimeoutSeconds", .targetKeyValue="FanManualModeTimeoutSeconds"},
        NULL
};

static const UtoolOutputMapping getFanMappings[] = {
        {.sourceXpath = "/MemberId", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/MaxReadingRange", .targetKeyValue="RatedSpeedRPM"},
        {.sourceXpath = "/Reading", .targetKeyValue="SpeedRPM"},
        {.sourceXpath = "/MinReadingRange", .targetKeyValue="LowerThresholdRPM"},
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
int UtoolCmdGetFan(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,           // output result json
            *fans = NULL,           // output fan array
            *fan = NULL,            // output fan item
            *thermalJson = NULL;    // curl response thermal as json

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Thermal", HTTP_GET, NULL, NULL, response);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto DONE;
    }


    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process response
    thermalJson = cJSON_Parse(response->content);
    ret = UtoolAssetParseJsonNotNull(thermalJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    ret = UtoolMappingCJSONItems(thermalJson, output, getFanSummaryMapping);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // initialize output fan array
    fans = cJSON_AddArrayToObject(output, "Fan");
    ret = UtoolAssetCreatedJsonNotNull(fans);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *members = cJSON_GetObjectItem(thermalJson, "Fans");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);

        fan = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(fan);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create fan item and add it to array
        ret = UtoolMappingCJSONItems(member, fan, getFanMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(fans, fan);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(fan)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(thermalJson)
    UtoolFreeCurlResponse(response);
    UtoolFreeRedfishServer(server);
    return ret;
}
