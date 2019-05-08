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

#define COMMAND_NAME "getfw"

static const char *const usage[] = {
        "utool getfw",
        NULL,
};

static const utool_OutputMapping getFwMappings[] = {
        {.sourceXpath = "/Name", .targetKeyValue="Name"},
        {.sourceXpath = "/SoftwareId", .targetKeyValue="Type"},
        {.sourceXpath = "/Version", .targetKeyValue="Version"},
        {.sourceXpath = "/Updateable", .targetKeyValue="Updateable"},
//        {.sourceXpath = "", .targetKeyValue="SupportActivateType"},
};


/**
 * command handler of `getfw`
 *
 * @param commandOption
 * @param result
 * @return
 */
int utool_get_fw(utool_CommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, utool_get_help_option_cb, 0, 0),
            OPT_END(),
    };


    utool_RedfishServer *server = &(utool_RedfishServer) {0};
    utool_CurlResponse *memberResp = &(utool_CurlResponse) {0};
    utool_CurlResponse *firmwareResp = &(utool_CurlResponse) {0};
    cJSON *firmwareJson = NULL;
    cJSON *firmwareMembersJson = NULL;

    // initialize output objects
    cJSON *firmware = NULL;
    cJSON *firmwares = cJSON_CreateArray();
    ret = utool_asset_json_not_null(firmwares);
    if (ret != OK) goto done;

    cJSON *output = cJSON_CreateObject();
    ret = utool_asset_json_not_null(output);
    if (ret != OK) goto done;

    cJSON_AddItemToObject(output, "Firmware", firmwares);


    ret = utool_validate_sub_command_basic_options(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) goto done;

    ret = utool_validate_connect_options(commandOption, result);
    if (commandOption->flag != EXECUTABLE) goto done;

    ret = utool_get_redfish_server(commandOption, server, result);
    if (ret != OK || server->systemId == NULL) goto done;

    ret = utool_curl_make_request(server, "/UpdateService/FirmwareInventory", HTTP_GET, NULL, NULL, memberResp);
    if (ret != OK) goto done;
    if (memberResp->httpStatusCode >= 400)
    {
        ret = utool_resolve_failure_response(memberResp, result);
        goto done;
    }

    // process response
    firmwareMembersJson = cJSON_Parse(memberResp->content);
    ret = utool_asset_json_not_null(firmwareMembersJson);
    if (ret != OK) goto done;

    int count = sizeof(getFwMappings) / sizeof(utool_OutputMapping);
    cJSON *members = cJSON_GetObjectItem(firmwareMembersJson, "Members");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++)
    {
        firmware = cJSON_CreateObject();
        ret = utool_asset_json_not_null(firmware);
        if (ret != OK) goto failure;

        cJSON *member = cJSON_GetArrayItem(members, idx);
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;

        ret = utool_curl_make_request(server, url, HTTP_GET, NULL, NULL, firmwareResp);
        if (ret != OK) goto failure;

        firmwareJson = cJSON_Parse(firmwareResp->content);
        ret = utool_asset_json_not_null(firmwareJson);
        if (ret != OK) goto failure;

        // create firmware item and add it to array
        utool_mapping_cJSON_items(firmwareJson, firmware, getFwMappings, count);
        cJSON_AddItemToArray(firmwares, firmware);

        // free memory
        FREE_cJSON(firmwareJson);
        utool_free_curl_response(firmwareResp);
    }


    // output to result
    ret = utool_copy_result(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_cJSON(output)
    goto done;

done:
    FREE_cJSON(firmwareMembersJson)
    FREE_cJSON(firmwareJson)
    utool_free_curl_response(memberResp);
    utool_free_curl_response(firmwareResp);
    utool_free_redfish_server(server);
    return ret;
}
