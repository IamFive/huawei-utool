/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getip`
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
        "getip",
        NULL,
};

static int VlanPropertyHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    cJSON *vlanEnableNode = cJSON_GetObjectItem(node, "VLANEnable");
    int ret = UtoolAssetJsonNodeNotNull(vlanEnableNode, "/VLANInfo/VLANEnable");
    if (ret != UTOOLE_OK) {
        FREE_CJSON(node)
        return ret;
    }

    cJSON *stateNode = cJSON_AddStringToObject(node, "State", cJSON_IsTrue(vlanEnableNode) ? ENABLED : DISABLED);
    ret = UtoolAssetCreatedJsonNotNull(stateNode);
    if (ret != UTOOLE_OK) {
        FREE_CJSON(node)
        return ret;
    }

    cJSON_DeleteItemFromObject(node, "VLANEnable");
    cJSON_AddItemToObjectCS(target, key, node);
    return UTOOLE_OK;
}

static const UtoolOutputMapping getEthernetMappings[] = {
        {.sourceXpath = "/Oem/${Oem}/IPVersion", .targetKeyValue="IPVersion"},
        {.sourceXpath = "/PermanentMACAddress", .targetKeyValue="PermanentMACAddress"},
        {.sourceXpath = "/IPv4Addresses", .targetKeyValue="IPv4"},
        {.sourceXpath = "/IPv6DefaultGateway", .targetKeyValue="IPv6DefaultGateway"},
        {.sourceXpath = "/IPv6Addresses", .targetKeyValue="IPv6"},
        {.sourceXpath = "/VLAN", .targetKeyValue="VLANInfo", .handle=VlanPropertyHandler},
        NULL,
};


/**
 * Get BMC IP and VLAN information, command handler of `getip`
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolCmdGetBmcIP(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getMembersResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getMemberResp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *memberJson = NULL, *membersJson = NULL;
    cJSON *output = NULL;

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

    ret = UtoolMakeCurlRequest(server, "/Managers/%s/EthernetInterfaces/", HTTP_GET, NULL, NULL, getMembersResp);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }
    if (getMembersResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getMembersResp, result);
        goto DONE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process response
    membersJson = cJSON_Parse(getMembersResp->content);
    ret = UtoolAssetParseJsonNotNull(membersJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }


    cJSON *members = cJSON_GetObjectItem(membersJson, "Members");
    cJSON *member = cJSON_GetArrayItem(members, 0);
    cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
    char *url = odataIdNode->valuestring;

    ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getMemberResp);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (getMemberResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getMemberResp, result);
        goto FAILURE;
    }

    memberJson = cJSON_Parse(getMemberResp->content);
    ret = UtoolAssetParseJsonNotNull(memberJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // create task item and add it to array
    ret = UtoolMappingCJSONItems(server, memberJson, output, getEthernetMappings);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(memberJson)
    FREE_CJSON(membersJson)
    UtoolFreeCurlResponse(getMemberResp);
    UtoolFreeCurlResponse(getMembersResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
