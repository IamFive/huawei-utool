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

#define COMMAND_NAME "getproduct"

static const char *const usage[] = {
        "utool getproduct",
        NULL,
};

static const UtoolOutputMapping getProductMappings[] = {
        {.sourceXpath = "/Model", .targetKeyValue="ProductName"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/UUID", .targetKeyValue="UUID"},
        {.sourceXpath = "/Oem/Huawei/DeviceOwnerID", .targetKeyValue="DeviceOwnerID"},
        {.sourceXpath = "/Oem/Huawei/DeviceSlotID", .targetKeyValue="DeviceSlotID"},
        {.sourceXpath = "/PowerState", .targetKeyValue="PowerState"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
};

/**
 * argparse get version action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetProduct(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *json = NULL;
    cJSON *output = NULL;

    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

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
    if (ret != OK || server->systemId == NULL) {
        goto done;
    }

    ret = UtoolMakeCurlRequest(server, "/Systems/%s", HTTP_GET, NULL, NULL, response);
    if (ret != OK) {
        goto done;
    }

    // process response
    output = cJSON_CreateObject();
    ret = UtoolAssetJsonNotNull(output);
    if (ret != OK) { goto failure; }

    if (response->httpStatusCode < 300) {
        json = cJSON_Parse(response->content);
        ret = UtoolAssetJsonNotNull(json);
        if (ret != OK) { goto failure; }

        // create output json object
        int count = sizeof(getProductMappings) / sizeof(UtoolOutputMapping);
        UtoolMappingCJSONItems(json, output, getProductMappings, count);

        // output to result
        ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
        goto done;
    }
    else {
        ret = UtoolResolveFailureResponse(response, result);
        goto failure;
    }


failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(json)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    return ret;
}
