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
        "utool getvolt",
        NULL,
};

static const UtoolOutputMapping getPowerSummaryMapping[] = {
        {.sourceXpath = "/Oem/Huawei/PowerSupplySummary/Count", .targetKeyValue="Maximum"},
        {.sourceXpath = "/Oem/Huawei/PowerSupplySummary/Status/HealthRollup", .targetKeyValue="OverallHealth"},
        NULL
};

static const UtoolOutputMapping getPowerSupplyMapping[] = {
        {.sourceXpath = "/MemberId", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/Huawei/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Model", .targetKeyValue="Model"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/PartNumber", .targetKeyValue="PartNumber"},
        {.sourceXpath = "/PowerSupplyType", .targetKeyValue="PowerSupplyType"},
        {.sourceXpath = "/LineInputVoltage", .targetKeyValue="LineInputVoltage"},
        {.sourceXpath = "/PowerCapacityWatts", .targetKeyValue="PowerCapacityWatts"},
        {.sourceXpath = "/FirmwareVersion", .targetKeyValue="FirmwareVersion"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},

        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},

        {.sourceXpath = "/Oem/Huawei/Protocol", .targetKeyValue="Protocol"},
        {.sourceXpath = "/Oem/Huawei/PowerOutputWatts", .targetKeyValue="PowerOutputWatts"},
        {.sourceXpath = "/Oem/Huawei/InputAmperage", .targetKeyValue="InputAmperage"},
        {.sourceXpath = "/Oem/Huawei/ActiveStandby", .targetKeyValue="ActiveStandby"},
        {.sourceXpath = "/Oem/Huawei/OutputVoltage", .targetKeyValue="OutputVoltage"},
        {.sourceXpath = "/Oem/Huawei/PowerInputWatts", .targetKeyValue="PowerInputWatts"},
        {.sourceXpath = "/Oem/Huawei/OutputAmperage", .targetKeyValue="OutputAmperage"},
        NULL
};


/**
 * command handler of `getfw`
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

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Power", HTTP_GET, NULL, NULL, getPowerResponse);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getPowerResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getPowerResponse, result);
        goto done;
    }

    ret = UtoolMakeCurlRequest(server, "/Chassis/%s", HTTP_GET, NULL, NULL, getChassisResponse);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getChassisResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisResponse, result);
        goto done;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process chassis response
    chassisJson = cJSON_Parse(getChassisResponse->content);
    ret = UtoolAssetParseJsonNotNull(chassisJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    UtoolMappingCJSONItems(chassisJson, output, getPowerSummaryMapping);

    // initialize output temperatures array
    powerSupplies = cJSON_AddArrayToObject(output, "PSU");
    ret = UtoolAssetCreatedJsonNotNull(powerSupplies);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process power response
    powerJson = cJSON_Parse(getPowerResponse->content);
    ret = UtoolAssetParseJsonNotNull(powerJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }


    cJSON *members = cJSON_GetObjectItem(powerJson, "PowerSupplies");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);

        powerSupply = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(powerSupply);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create voltage item and add it to array
        UtoolMappingCJSONItems(member, powerSupply, getPowerSupplyMapping);
        cJSON_AddItemToArray(powerSupplies, powerSupply);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(powerJson)
    FREE_CJSON(chassisJson)
    UtoolFreeCurlResponse(getPowerResponse);
    UtoolFreeCurlResponse(getChassisResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}