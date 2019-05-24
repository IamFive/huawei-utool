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

//get BMC IP and VLAN information

static const char *const usage[] = {
        "utool getip",
        NULL,
};

static int VlanPropertyHandler(cJSON *target, const char *key, cJSON *node)
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
        {.sourceXpath = "/Oem/Huawei/IPVersion", .targetKeyValue="IPVersion"},
        {.sourceXpath = "/PermanentMACAddress", .targetKeyValue="PermanentMACAddress"},
        {.sourceXpath = "/IPv4Addresses", .targetKeyValue="IPv4"},
        {.sourceXpath = "/IPv6Addresses", .targetKeyValue="IPv6"},
        {.sourceXpath = "/VLAN", .targetKeyValue="VLANInfo", .handle=VlanPropertyHandler},
        NULL,
};


/**
 * command handler of `getip`
 *
 * Get BMC IP and VLAN information
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

    ret = UtoolMakeCurlRequest(server, "/Managers/%s/EthernetInterfaces/", HTTP_GET, NULL, NULL, getMembersResp);
    if (ret != UTOOLE_OK) {
        goto done;
    }
    if (getMembersResp->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getMembersResp, result);
        goto done;
    }

    // initialize output objects
    cJSON *memberJson = NULL, *membersJson = NULL;
    cJSON *output = NULL, *tasks = NULL, *task = NULL;

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    tasks = cJSON_AddArrayToObject(output, "task");
    ret = UtoolAssetCreatedJsonNotNull(tasks);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // process response
    membersJson = cJSON_Parse(getMembersResp->content);
    ret = UtoolAssetParseJsonNotNull(membersJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    cJSON *member = NULL;
    cJSON *members = cJSON_GetObjectItem(membersJson, "Members");
    cJSON_ArrayForEach(member, members) {
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;

        ret = UtoolMakeCurlRequest(server, url, HTTP_GET, NULL, NULL, getMemberResp);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        if (getMemberResp->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getMemberResp, result);
            goto failure;
        }

        memberJson = cJSON_Parse(getMemberResp->content);
        ret = UtoolAssetParseJsonNotNull(memberJson);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        task = cJSON_CreateObject();
        ret = UtoolAssetCreatedJsonNotNull(task);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        // create task item and add it to array
        ret = UtoolMappingCJSONItems(memberJson, task, getEthernetMappings);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
        cJSON_AddItemToArray(tasks, task);

        // free memory
        FREE_CJSON(memberJson)
        UtoolFreeCurlResponse(getMemberResp);
    }

    // output to result
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    goto done;

failure:
    FREE_CJSON(task)
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(membersJson)
    FREE_CJSON(memberJson)
    UtoolFreeCurlResponse(getMembersResp);
    UtoolFreeCurlResponse(getMemberResp);
    UtoolFreeRedfishServer(server);
    return ret;
}
