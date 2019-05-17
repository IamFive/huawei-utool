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
        "utool getfw",
        NULL,
};

static const UtoolOutputMapping getFwMappings[] = {
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
int UtoolCmdGetFirmware(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *memberResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *firmwareResp = &(UtoolCurlResponse) {0};

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

    ret = UtoolMakeCurlRequest(server, "/UpdateService/FirmwareInventory", HTTP_GET, NULL, NULL, memberResp);
    if (ret != OK) {
        goto done;
    }
    if (memberResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(memberResp, result);
        goto done;
    }

    // initialize output objects
    cJSON *firmwareJson = NULL, *firmwareMembersJson = NULL;
    cJSON *output = NULL, *firmwares = NULL, *firmware = NULL;

    output = cJSON_CreateObject();
    ret = UtoolAssetJsonNotNull(output);
    if (ret != OK) {
        goto failure;
    }

    firmwares = cJSON_AddArrayToObject(output, "Firmware");
    ret = UtoolAssetJsonNotNull(output);
    if (ret != OK) {
        goto failure;
    }

    // process response
    firmwareMembersJson = cJSON_Parse(memberResp->content);
    ret = UtoolAssetJsonNotNull(firmwareMembersJson);
    if (ret != OK) {
        goto failure;
    }

    int count = sizeof(getFwMappings) / sizeof(UtoolOutputMapping);
    cJSON *members = cJSON_GetObjectItem(firmwareMembersJson, "Members");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, firmwareResp);
        if (ret != OK) {
            goto failure;
        }

        firmwareJson = cJSON_Parse(firmwareResp->content);
        ret = UtoolAssetJsonNotNull(firmwareJson);
        if (ret != OK) {
            goto failure;
        }

        firmware = cJSON_CreateObject();
        ret = UtoolAssetJsonNotNull(firmware);
        if (ret != OK) {
            goto failure;
        }

        // create firmware item and add it to array
        UtoolMappingCJSONItems(firmwareJson, firmware, getFwMappings, count);
        cJSON_AddItemToArray(firmwares, firmware);

        // free memory
        FREE_CJSON(firmwareJson)
        UtoolFreeCurlResponse(firmwareResp);
    }


    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(firmwareMembersJson)
    FREE_CJSON(firmwareJson)
    UtoolFreeCurlResponse(memberResp);
    UtoolFreeCurlResponse(firmwareResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
