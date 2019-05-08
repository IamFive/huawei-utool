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

static const utool_OutputMapping getProductMappings[] = {
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
int utool_get_product(utool_CommandOption *commandOption, char **result)
{
    int ret;

    utool_CurlResponse *response = &(utool_CurlResponse) {0};
    utool_RedfishServer *server = &(utool_RedfishServer) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, utool_get_help_option_cb, 0, 0),
            OPT_END(),
    };

    ret = utool_validate_sub_command_basic_options(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) goto done;

    ret = utool_validate_connect_options(commandOption, result);
    if (commandOption->flag != EXECUTABLE) goto done;


    ret = utool_get_redfish_server(commandOption, server, result);
    if (ret != OK || server->systemId == NULL) goto done;

    ret = utool_curl_make_request(server, "/Systems/%s", HTTP_GET, NULL, NULL, response);
    if (ret != OK) goto done;

    // process response
    cJSON *json;
    cJSON *output = cJSON_CreateObject();
    ret = utool_asset_json_not_null(output);
    if (ret != OK) goto failure;

    if (response->httpStatusCode < 300)
    {
        json = cJSON_Parse(response->content);
        ret = utool_asset_json_not_null(json);
        if (ret != OK) goto failure;

        // create output json object
        int count = sizeof(getProductMappings) / sizeof(utool_OutputMapping);
        utool_mapping_cJSON_items(json, output, getProductMappings, count);

        // output to result
        ret = utool_copy_result(STATE_SUCCESS, output, result);
        goto done;
    }
    else
    {
        ret = utool_resolve_failure_response(response, result);
        goto failure;
    }


failure:
    FREE_cJSON(output)
    goto done;

done:
    FREE_cJSON(json)
    utool_free_redfish_server(server);
    utool_free_curl_response(response);
    return ret;
}
