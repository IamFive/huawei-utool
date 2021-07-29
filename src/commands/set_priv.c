/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setpriv`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <securec.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"
#include "string_utils.h"

typedef struct _SetUserOption {
    char *username;
    char *roleId;
} UtoolSetUserOption;


static const char *ROLES[] = {"Administrator", "Operator", "Commonuser", ROLE_OEM, NULL};

static const char *const usage[] = {
        "setpriv -n username -r role-id",
        NULL,
};


static void ValidateSetUserOptions(UtoolSetUserOption *option, UtoolResult *result);

static cJSON *BuildPayload(UtoolSetUserOption *option, UtoolResult *result);


/**
* set user privileges, command handler for `setpriv`
*
* @param commandOption
* @param outputStr
* @return
* */
int UtoolCmdSetUserPriv(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getUserResponse = &(UtoolCurlResponse) {0};
    UtoolSetUserOption *setPasswordOption = &(UtoolSetUserOption) {0};

    // initialize output objects
    cJSON *userMembersJson = NULL,        // curl response user member as json
            *userJson = NULL;              // curl response user as json

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('n', "username", &(setPasswordOption->username), "specifies the user to be modified", NULL, 0,
                        0),
            OPT_STRING ('r', "role-id", &(setPasswordOption->roleId),
                        "new role, choices: {Administrator, Operator, Commonuser, OEM}.", NULL, 0, 0),
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

    ValidateSetUserOptions(setPasswordOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    // build payload
    payload = BuildPayload(setPasswordOption, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/AccountService/Accounts", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    userMembersJson = result->data;

    bool foundUserWithName = false;
    char *userAccountUrl = NULL;
    cJSON *members = cJSON_GetObjectItem(userMembersJson, "Members");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount && !foundUserWithName; idx++) {
        cJSON *userLinkObject = cJSON_GetArrayItem(members, idx);
        cJSON *link = cJSON_GetObjectItem(userLinkObject, "@odata.id");
        result->code = UtoolAssetJsonNodeNotNull(link, "/Members/*/@odata.id");
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        userAccountUrl = link->valuestring;
        result->code = UtoolMakeCurlRequest(server, userAccountUrl, HTTP_GET, NULL, NULL, getUserResponse);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        if (getUserResponse->httpStatusCode >= 400) {
            result->code = UtoolResolveFailureResponse(getUserResponse, outputStr);
            goto FAILURE;
        }

        userJson = cJSON_Parse(getUserResponse->content);
        result->code = UtoolAssetParseJsonNotNull(userJson);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *userNameNode = cJSON_GetObjectItem(userJson, "UserName");
        result->code = UtoolAssetJsonNodeNotNull(userNameNode, "UserName");
        char *username = userNameNode->valuestring;
        if (result->code != UTOOLE_OK || username == NULL) {
            goto FAILURE;
        }

        if (UtoolStringEquals(setPasswordOption->username, username)) {
            foundUserWithName = true;
            ZF_LOGI("Current username is %s, matched.", username);
            break;
        } else {
            ZF_LOGI("Current username is %s, does not matched.", username);
        }

        FREE_CJSON(userJson)
        UtoolFreeCurlResponse(getUserResponse);
    }

    if (!foundUserWithName) {
        char buffer[MAX_FAILURE_MSG_LEN];
        UtoolWrapSnprintf(buffer, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN, "Failure: No user with name `%s` exists",
                   setPasswordOption->username);
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, buffer, &(result->desc));
    } else {
        // update user
        const UtoolCurlHeader ifMatchHeader[] = {
                {.name = HEADER_IF_MATCH, .value=getUserResponse->etag},
                NULL,
        };

        UtoolRedfishPatch(server, userAccountUrl, payload, ifMatchHeader, NULL, NULL, result);
        if (result->broken) {
            goto FAILURE;
        }
        FREE_CJSON(result->data)

        UtoolBuildDefaultSuccessResult(&(result->desc));
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(userJson)
    FREE_CJSON(userMembersJson)
    UtoolFreeCurlResponse(getUserResponse);
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static void ValidateSetUserOptions(UtoolSetUserOption *option, UtoolResult *result)
{
    if (UtoolStringIsEmpty(option->username)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("username")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->roleId)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("role-id")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->roleId, ROLES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(
                OPT_NOT_IN_CHOICE("role-id", "Administrator, Operator, Commonuser, OEM")), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetUserOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    char *roleIdName = option->roleId;
    if (UtoolStringEquals(option->roleId, ROLE_OEM)) {
        roleIdName = ROLE_OEM_MAP;
    }
    cJSON *roleId = cJSON_AddStringToObject(payload, "RoleId", roleIdName);
    result->code = UtoolAssetCreatedJsonNotNull(roleId);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}