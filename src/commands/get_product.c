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
        "utool getproduct",
        NULL,
};

static const UtoolOutputMapping getPowerMappings[] = {
        {.sourceXpath = "/PowerControl/0/PowerConsumedWatts", .targetKeyValue="TotalPowerWatts"},
        NULL
};

static const UtoolOutputMapping getProductMappings[] = {
        {.sourceXpath = "/Model", .targetKeyValue="ProductName"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/UUID", .targetKeyValue="UUID"},
        {.sourceXpath = "/Oem/Huawei/DeviceOwnerID", .targetKeyValue="DeviceOwnerID"},
        {.sourceXpath = "/Oem/Huawei/DeviceSlotID", .targetKeyValue="DeviceSlotID"},
        {.sourceXpath = "/PowerState", .targetKeyValue="PowerState"},
        {.sourceXpath = "/Status/Health", .targetKeyValue = "Health"},
        NULL
};

/**
 * argparse get version action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolCmdGetProduct(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *output = NULL;
    cJSON *getSystemJson = NULL, *getChassisPowerJson = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getSystemResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getChassisPowerResponse = &(UtoolCurlResponse) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

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

    // process get system response
    ret = UtoolMakeCurlRequest(server, "/Systems/%s", HTTP_GET, NULL, NULL, getSystemResponse);
    if (ret != UTOOLE_OK) {
        goto done;
    }

    if (getSystemResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getSystemResponse, result);
        goto failure;
    }

    getSystemJson = cJSON_Parse(getSystemResponse->content);
    ret = UtoolAssetParseJsonNotNull(getSystemJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    UtoolMappingCJSONItems(getSystemJson, output, getProductMappings);


    // process get chassis power response
    ret = UtoolMakeCurlRequest(server, "/Chassis/%s/Power", HTTP_GET, NULL, NULL, getChassisPowerResponse);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (getChassisPowerResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getChassisPowerResponse, result);
        goto failure;
    }

    getChassisPowerJson = cJSON_Parse(getChassisPowerResponse->content);
    ret = UtoolAssetParseJsonNotNull(getChassisPowerJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }
    UtoolMappingCJSONItems(getChassisPowerJson, output, getPowerMappings);

    // mapping result to output json
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(getSystemJson)
    FREE_CJSON(getChassisPowerJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(getSystemResponse);
    UtoolFreeCurlResponse(getChassisPowerResponse);
    return ret;
}
