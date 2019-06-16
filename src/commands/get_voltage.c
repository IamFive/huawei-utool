/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getvolt`
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
        "getvolt",
        NULL,
};

static const UtoolOutputMapping getVoltageMappings[] = {
        {.sourceXpath = "/Name", .targetKeyValue="Name"},
        {.sourceXpath = "/SensorNumber", .targetKeyValue="SensorNumber"},
        {.sourceXpath = "/UpperThresholdFatal", .targetKeyValue="UpperThresholdFatal"},
        {.sourceXpath = "/UpperThresholdCritical", .targetKeyValue="UpperThresholdCritical"},
        {.sourceXpath = "/UpperThresholdNonCritical", .targetKeyValue="UpperThresholdNonCritical"},
        {.sourceXpath = "/ReadingCelsius", .targetKeyValue="ReadingCelsius"},
        {.sourceXpath = "/LowerThresholdNonCritical", .targetKeyValue="LowerThresholdNonCritical"},
        {.sourceXpath = "/LowerThresholdCritical", .targetKeyValue="LowerThresholdCritical"},
        {.sourceXpath = "/LowerThresholdFatal", .targetKeyValue="LowerThresholdFatal"},
        NULL
};


/**
 * get voltage information, command handler of `getvolt`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetVoltage(UtoolCommandOption *commandOption, char **result)
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
            *voltages = NULL,       // output voltages array
            *voltage = NULL,        // output voltage
            *powerJson = NULL;      // curl response power content as json

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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Power", HTTP_GET, NULL, NULL, response);
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

    // initialize output temperatures array
    voltages = cJSON_AddArrayToObject(output, "Voltages");
    ret = UtoolAssetCreatedJsonNotNull(voltages);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process response
    powerJson = cJSON_Parse(response->content);
    ret = UtoolAssetParseJsonNotNull(powerJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *members = cJSON_GetObjectItem(powerJson, "Voltages");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);

        voltage = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(voltage);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create voltage item and add it to array
        ret = UtoolMappingCJSONItems(member, voltage, getVoltageMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(voltages, voltage);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(voltage)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(powerJson)
    UtoolFreeCurlResponse(response);
    UtoolFreeRedfishServer(server);
    return ret;
}
