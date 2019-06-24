/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getuser`
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
        "getuser",
        NULL,
};

static int EmptyPrivilegeHandler(cJSON *target, const char *key, cJSON *node)
{
    FREE_CJSON(node)
    cJSON *newNode = cJSON_AddArrayToObject(target, key);
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getUserMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="UserId"},
        {.sourceXpath = "/UserName", .targetKeyValue="UserName"},
        {.sourceXpath = "/RoleId", .targetKeyValue="RoleId"},
        {.sourceXpath = "/Null", .targetKeyValue="Privilege", .handle=EmptyPrivilegeHandler},
        {.sourceXpath = "/Locked", .targetKeyValue="Locked"},
        {.sourceXpath = "/Enabled", .targetKeyValue="Enabled"},
        NULL
};

/**
 * get BMC user information, command handler of `getuser`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetUsers(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *userMemberJson = NULL, *output = NULL, *userArray = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

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

    userArray = cJSON_AddArrayToObject(output, "User");
    result->code = UtoolAssetCreatedJsonNotNull(userArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/AccountService/Accounts", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }

    userMemberJson = result->data;
    UtoolRedfishGetMemberResources(server, userMemberJson, userArray, getUserMappings, result);
    if (result->broken) {
        goto FAILURE;
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(userMemberJson)
    UtoolFreeRedfishServer(server);
    *outputStr = result->desc;
    return result->code;
}
