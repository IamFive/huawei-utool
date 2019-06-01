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
        "utool getsensor",
        NULL,
};

static const UtoolOutputMapping getTemperatureMappings[] = {
        {.sourceXpath = "/SensorNumber", .targetKeyValue="SensorNumber"}, // ?
        {.sourceXpath = "/Name", .targetKeyValue="SensorName"},
        {.sourceXpath = "/ReadingValue", .targetKeyValue="Reading"},
        {.sourceXpath = "/Unit", .targetKeyValue="Unit"},
        {.sourceXpath = "/Status", .targetKeyValue="Status"},

        {.sourceXpath = "/UpperThresholdNonCritical", .targetKeyValue="unc"},
        {.sourceXpath = "/UpperThresholdCritical", .targetKeyValue="uc"},
        {.sourceXpath = "/UpperThresholdFatal", .targetKeyValue="unr"},
        {.sourceXpath = "/LowerThresholdNonCritical", .targetKeyValue="lnc"},
        {.sourceXpath = "/LowerThresholdCritical", .targetKeyValue="lc"},
        {.sourceXpath = "/LowerThresholdFatal", .targetKeyValue="lnr"},
        NULL
};


/**
 * command handler of `getfw`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetSensor(UtoolCommandOption *commandOption, char **result)
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
            *sensors = NULL, // output sensor array
            *sensor = NULL,    // output sensor
            *sensorsJson = NULL;    // curl response thermal as json

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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/ThresholdSensors", HTTP_GET, NULL, NULL, response);
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

    // initialize output sensors array
    sensors = cJSON_AddArrayToObject(output, "Sensor");
    ret = UtoolAssetCreatedJsonNotNull(sensors);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process response
    sensorsJson = cJSON_Parse(response->content);
    ret = UtoolAssetParseJsonNotNull(sensorsJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(sensorsJson, "Sensors");
    cJSON_ArrayForEach(member, members) {
        sensor = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(sensor);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create sensor item and add it to array
        ret = UtoolMappingCJSONItems(member, sensor, getTemperatureMappings);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON_AddItemToArray(sensors, sensor);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(sensor)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(sensorsJson)
    UtoolFreeCurlResponse(response);
    UtoolFreeRedfishServer(server);
    return ret;
}
