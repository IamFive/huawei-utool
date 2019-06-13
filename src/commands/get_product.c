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

static int CalculateTotalPowerWattsHandler(cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) { // should not happen
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    if (cJSON_IsArray(node)) {
        int total = 0;
        cJSON *ps = NULL;
        cJSON_ArrayForEach(ps, node) {
            cJSON *input = cJSONUtils_GetPointer(ps, "/Oem/Huawei/PowerInputWatts");
            if (input != NULL && cJSON_IsNumber(input)) {
                total += input->valueint;
            }
        }

        FREE_CJSON(node)
        cJSON *newNode;
        if (total > 0) {
            newNode = cJSON_AddNumberToObject(target, key, total);
        }
        else {
            newNode = cJSON_AddNullToObject(target, key);
        }

        return UtoolAssetCreatedJsonNotNull(newNode);
    }
    else {
        FREE_CJSON(node)
        return UTOOLE_UNKNOWN_JSON_FORMAT;
    }

}

static const UtoolOutputMapping getPowerMappings[] = {
        {.sourceXpath = "/PowerSupplies", .targetKeyValue="TotalPowerWatts", .handle=CalculateTotalPowerWattsHandler},
        NULL
};

static const UtoolOutputMapping getProductMappings[] = {
        {.sourceXpath = "/Model", .targetKeyValue="ProductName"},
        {.sourceXpath = "/Manufacturer", .targetKeyValue="Manufacturer"},
        {.sourceXpath = "/SerialNumber", .targetKeyValue="SerialNumber"},
        {.sourceXpath = "/UUID", .targetKeyValue="UUID"},
        {.sourceXpath = "/HostingRole", .targetKeyValue="HostingRole"},
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
int UtoolCmdGetProduct(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

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

    UtoolRedfishGet(server, "/Systems/%s", output, getProductMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolRedfishGet(server, "/Chassis/%s/Power", output, getPowerMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

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
