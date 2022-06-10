/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getpsu`
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
        "getpsu",
        NULL,
};

static const UtoolOutputMapping getPowerSummaryMapping[] = {
        {.sourceXpath = "/Oem/${Oem}/PowerSupplySummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        {.sourceXpath = "/Oem/${Oem}/PowerSupplySummary/Count", .targetKeyValue="Maximum"},
        NULL
};

static const UtoolOutputMapping getPowerSupplyMapping[] = {
        {.sourceXpath = "/MemberId", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/${Oem}/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/Oem/${Oem}/Protocol", .targetKeyValue="Protocol"},
        {.sourceXpath = "/Oem/${Oem}/PowerOutputWatts", .targetKeyValue="PowerOutputWatts"},
        {.sourceXpath = "/Oem/${Oem}/InputAmperage", .targetKeyValue="InputAmperage"},
        {.sourceXpath = "/Oem/${Oem}/ActiveStandby", .targetKeyValue="ActiveStandby"},
        {.sourceXpath = "/Oem/${Oem}/OutputVoltage", .targetKeyValue="OutputVoltage"},
        {.sourceXpath = "/Oem/${Oem}/PowerInputWatts", .targetKeyValue="PowerInputWatts"},
        {.sourceXpath = "/Oem/${Oem}/OutputAmperage", .targetKeyValue="OutputAmperage"},

        {.sourceXpath = "/PartNumber", .targetKeyValue="PartNumber"},
        {.sourceXpath = "/PowerSupplyType", .targetKeyValue="PowerSupplyType"},
        {.sourceXpath = "/LineInputVoltage", .targetKeyValue="LineInputVoltage"},
        {.sourceXpath = "/PowerCapacityWatts", .targetKeyValue="PowerCapacityWatts"},
        {.sourceXpath = "/FirmwareVersion", .targetKeyValue="FirmwareVersion"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},

        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},


        NULL
};


/**
 * get power supply information, command handler of `getpsu`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetPowerSupply(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getPowerResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getChassisResponse = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *output = NULL,               // output result json
            *powerSupplies = NULL,      // output power supplies array
            *powerSupply = NULL,        // output power supply item
            *powerJson = NULL,          // curl response power content as json
            *chassisJson = NULL;        // curl response chassis content as json

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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Power", HTTP_GET, NULL, NULL, getPowerResponse);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getPowerResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getPowerResponse, result);
        goto DONE;
    }

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s", HTTP_GET, NULL, NULL, getChassisResponse);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getChassisResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisResponse, result);
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process chassis response
    chassisJson = cJSON_Parse(getChassisResponse->content);
    ret = UtoolAssetParseJsonNotNull(chassisJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }
    ret = UtoolMappingCJSONItems(server, chassisJson, output, getPowerSummaryMapping);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }
    // initialize output temperatures array
    powerSupplies = cJSON_AddArrayToObject(output, "PSU");
    ret = UtoolAssetCreatedJsonNotNull(powerSupplies);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process power response
    powerJson = cJSON_Parse(getPowerResponse->content);
    ret = UtoolAssetParseJsonNotNull(powerJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }


    cJSON *members = cJSON_GetObjectItem(powerJson, "PowerSupplies");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);

        powerSupply = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(powerSupply);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        // create voltage item and add it to array
        ret = UtoolMappingCJSONItems(server, member, powerSupply, getPowerSupplyMapping);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        cJSON_AddItemToArray(powerSupplies, powerSupply);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(powerSupply)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(powerJson)
    FREE_CJSON(chassisJson)
    UtoolFreeCurlResponse(getPowerResponse);
    UtoolFreeCurlResponse(getChassisResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}
