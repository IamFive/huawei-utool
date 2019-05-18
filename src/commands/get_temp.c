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
        "utool gettemp",
        NULL,
};

static const UtoolOutputMapping getTemperatureMappings[] = {
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
 * command handler of `getfw`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetTemperature(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,       // output result json
            *temperatures = NULL, // output temperature array
            *temperature = NULL,    // output temperature
            *thermalJson = NULL;    // curl response thermal as json

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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Thermal", HTTP_GET, NULL, NULL, response);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto done;
    }


    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // initialize output temperatures array
    temperatures = cJSON_AddArrayToObject(output, "Temperature");
    ret = UtoolAssetCreatedJsonNotNull(temperatures);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process response
    thermalJson = cJSON_Parse(response->content);
    ret = UtoolAssetParseJsonNotNull(thermalJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    cJSON *members = cJSON_GetObjectItem(thermalJson, "Temperatures");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);

        temperature = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(temperature);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create temperature item and add it to array
        UtoolMappingCJSONItems(member, temperature, getTemperatureMappings);
        cJSON_AddItemToArray(temperatures, temperature);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(thermalJson)
    UtoolFreeCurlResponse(response);
    UtoolFreeRedfishServer(server);
    return ret;
}
