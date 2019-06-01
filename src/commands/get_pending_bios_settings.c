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
int UtoolCmdGetPendingBiosSettings(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *getBiosJson = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getSystemResponse = &(UtoolCurlResponse) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

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

    // process get system response
    ret = UtoolMakeCurlRequest(server, "/Systems/%s/Bios/Settings", HTTP_GET, NULL, NULL, getSystemResponse);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (getSystemResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getSystemResponse, result);
        goto FAILURE;
    }

    getBiosJson = cJSON_Parse(getSystemResponse->content);
    ret = UtoolAssetParseJsonNotNull(getBiosJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *attributes = cJSON_DetachItemFromObject(getBiosJson, "Attributes");
    ret = UtoolAssetJsonNodeNotNull(attributes, "/Attributes");
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // mapping result to output json
    ret = UtoolBuildOutputResult(STATE_SUCCESS, attributes, result);
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(getBiosJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(getSystemResponse);
    return ret;
}
