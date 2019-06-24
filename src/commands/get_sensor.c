/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getsensor`
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
        "getsensor",
        NULL,
};

static const UtoolOutputMapping getThresholdSensorNestMappings[] = {
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


static const UtoolOutputMapping getThresholdSensorMappings[] = {
        {.sourceXpath = "/Sensors", .targetKeyValue="Sensors", .nestMapping=getThresholdSensorNestMappings}, // ?
        NULL
};


static int StatusToReadingHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    cJSON *newNode = NULL;
    if (node->valuestring != NULL) {
        int reading = strtod(node->valuestring, NULL);
        newNode = cJSON_AddNumberToObject(target, key, reading);
    }
    else {
        newNode = cJSON_AddNullToObject(target, key);
    }

    FREE_CJSON(node)
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getDiscreteSensorsNestMappings[] = {
        {.sourceXpath = "/Null", .targetKeyValue="SensorNumber"},
        {.sourceXpath = "/Name", .targetKeyValue="SensorName"},
        {.sourceXpath = "/Status", .targetKeyValue="Reading", .handle=StatusToReadingHandler},
        {.sourceXpath = "/Null", .targetKeyValue="Unit"},
        {.sourceXpath = "/Null", .targetKeyValue="Status"},
        {.sourceXpath = "/Null", .targetKeyValue="unc"},
        {.sourceXpath = "/Null", .targetKeyValue="uc"},
        {.sourceXpath = "/Null", .targetKeyValue="unr"},
        {.sourceXpath = "/Null", .targetKeyValue="lnc"},
        {.sourceXpath = "/Null", .targetKeyValue="lc"},
        {.sourceXpath = "/Null", .targetKeyValue="lnr"},
        NULL
};


static const UtoolOutputMapping getDiscreteSensorsMappings[] = {
        {.sourceXpath = "/Sensors", .targetKeyValue="Sensors", .nestMapping=getDiscreteSensorsNestMappings}, // ?
        NULL
};


/**
 * command handler of `getsensor`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetSensor(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    // initialize output objects
    cJSON *output = NULL;

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

    cJSON *maximum = cJSON_AddNumberToObject(output, "Maximum", 0);
    result->code = UtoolAssetCreatedJsonNotNull(maximum);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    /** get threshold sensors */
    UtoolRedfishGet(server, "/Chassis/%s/ThresholdSensors", output, getThresholdSensorMappings, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    /** get threshold sensors */
    UtoolRedfishGet(server, "/Chassis/%s/DiscreteSensors", output, getDiscreteSensorsMappings, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    cJSON *sensors = cJSON_GetObjectItem(output, "Sensors");
    maximum->valuedouble = cJSON_GetArraySize(sensors);

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
