/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `deluser`
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

static const char *const OPTION_USERNAME_REQUIRED = "Error: option `username` is required.";

static const char *const usage[] = {
        "deluser -n username",
        NULL,
};


/**
* Delete user, command handler for `deluser`
*
* @param commandOption
* @param result
* @return
*/
int UtoolCmdDeleteUser(UtoolCommandOption *commandOption, char **result)
{
    int ret = UTOOLE_OK;
    char *username = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getUserResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *deleteUserResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getUserMemberResponse = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *userMembersJson = NULL,        // curl response user member as json
            *userJson = NULL;              // curl response user as json

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('n', "username", &username, "specifies the user to delete", NULL, 0, 0),
            OPT_END(),
    };

    // validation
    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    if (username == NULL || strlen(username) == 0) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_USERNAME_REQUIRED), result);
        goto FAILURE;
    }

    // get redfish system id
    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    ret = UtoolMakeCurlRequest(server, "/AccountService/Accounts", HTTP_GET, NULL, NULL, getUserMemberResponse);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    // process get chassis response
    if (getUserMemberResponse->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(getUserMemberResponse, result);
        goto DONE;
    }

    userMembersJson = cJSON_Parse(getUserMemberResponse->content);
    ret = UtoolAssetParseJsonNotNull(userMembersJson);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    bool foundUserWithName = false;
    char *userAccountUrl = NULL;
    cJSON *members = cJSON_GetObjectItem(userMembersJson, "Members");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount && !foundUserWithName; idx++) {
        cJSON *userLinkObject = cJSON_GetArrayItem(members, idx);
        cJSON *link = cJSON_GetObjectItem(userLinkObject, "@odata.id");
        ret = UtoolAssetJsonNodeNotNull(link, "/Members/*/@odata.id");
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        userAccountUrl = link->valuestring;
        ret = UtoolMakeCurlRequest(server, userAccountUrl, HTTP_GET, NULL, NULL, getUserResponse);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        if (getUserResponse->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(getUserResponse, result);
            goto FAILURE;
        }

        userJson = cJSON_Parse(getUserResponse->content);
        ret = UtoolAssetParseJsonNotNull(userJson);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *userNameNode = cJSON_GetObjectItem(userJson, "UserName");
        ret = UtoolAssetJsonNodeNotNull(userNameNode, "UserName");
        char *currentUsername = userNameNode->valuestring;
        if (ret != UTOOLE_OK || username == NULL) {
            goto FAILURE;
        }

        if (UtoolStringEquals(username, currentUsername)) {
            foundUserWithName = true;
            ZF_LOGI("Current username is %s, matched.", username);
            break;
        } else {
            ZF_LOGI("Current username is %s, does not matched.", currentUsername);
        }

        FREE_CJSON(userJson)
        UtoolFreeCurlResponse(getUserResponse);
    }

    if (!foundUserWithName) {
        char buffer[MAX_FAILURE_MSG_LEN];
        UtoolWrapSecFmt(buffer, MAX_FAILURE_MSG_LEN, MAX_FAILURE_MSG_LEN - 1,
                        "Failure: No user with name `%s` exists", username);
        ret = UtoolBuildStringOutputResult(STATE_FAILURE, buffer, result);
    } else {
        // delete user
        const UtoolCurlHeader ifMatchHeader[] = {
                {.name = HEADER_IF_MATCH, .value=getUserResponse->etag},
                NULL,
        };

        ret = UtoolMakeCurlRequest(server, userAccountUrl, HTTP_DELETE, NULL, NULL, deleteUserResponse);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        if (deleteUserResponse->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(deleteUserResponse, result);
            goto FAILURE;
        }

        UtoolBuildDefaultSuccessResult(result);
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(userJson)
    FREE_CJSON(userMembersJson)
    UtoolFreeCurlResponse(getUserResponse);
    UtoolFreeCurlResponse(deleteUserResponse);
    UtoolFreeCurlResponse(getUserMemberResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}
