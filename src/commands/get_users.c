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
        "utool getuser",
        NULL,
};

static const UtoolOutputMapping getUserMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="UserId"},
        {.sourceXpath = "/UserName", .targetKeyValue="UserName"},
        {.sourceXpath = "/RoleId", .targetKeyValue="RoleId"},
        {.sourceXpath = "/Locked", .targetKeyValue="Locked"},
        {.sourceXpath = "/Enabled", .targetKeyValue="Enabled"},
        NULL
};

/**
 * command handler of `getuser`
 * get BMC user information
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

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, outputStr);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    userArray = cJSON_AddArrayToObject(output, "User");
    result->code = UtoolAssetCreatedJsonNotNull(userArray);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishGetResource(server, "/AccountService/Accounts", result);
    if (result->interrupt) {
        goto failure;
    }

    userMemberJson = result->data;
    UtoolRedfishGetMemberResources(server, userMemberJson, userArray, getUserMappings, result);
    if (result->interrupt) {
        goto failure;
    }

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(userMemberJson)
    UtoolFreeRedfishServer(server);
    *outputStr = result->desc;
    return result->code;
}
