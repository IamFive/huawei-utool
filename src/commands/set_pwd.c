/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setpwd`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

typedef struct _SetPasswordOption
{
    char *username;
    char *password;
    UtoolCommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
} UtoolSetPasswordOption;

static const char *const OPTION_USERNAME_REQUIRED = "Error: option `username` is required.";
static const char *const OPTION_PASSWORD_REQUIRED = "Error: option `new-password` is required.";

static const char *const usage[] = {
        "setpwd -n username -p new-password",
        NULL,
};


static int ValidateSetPasswordOptions(UtoolSetPasswordOption *option, char **result);

static cJSON *BuildPayload(UtoolSetPasswordOption *option);


/**
* set user password, command handler for `setpwd`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetPassword(UtoolCommandOption *commandOption, char **result)
{
    int ret;
    cJSON *payload = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getUserResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *updateUserResponse = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *getUserMemberResponse = &(UtoolCurlResponse) {0};
    UtoolSetPasswordOption *setPasswordOption = &(UtoolSetPasswordOption) {.flag = EXECUTABLE};

    // initialize output objects
    cJSON *userMembersJson = NULL,        // curl response user member as json
            *userJson = NULL;              // curl response user as json

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('n', "username", &(setPasswordOption->username), "specifies the user to be modified", NULL, 0, 0),
            OPT_STRING ('p', "new-password", &(setPasswordOption->password), "new password of user", NULL, 0, 0),
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

    ret = ValidateSetPasswordOptions(setPasswordOption, result);
    if (setPasswordOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(setPasswordOption);
    if (payload == NULL) {
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
        char *username = userNameNode->valuestring;
        if (ret != UTOOLE_OK || username == NULL) {
            goto FAILURE;
        }

        if (UtoolStringEquals(setPasswordOption->username, username)) {
            foundUserWithName = true;
            ZF_LOGI("Current username is %s, matched.", username);
            break;
        }
        else {
            ZF_LOGI("Current username is %s, does not matched.", username);
        }

        FREE_CJSON(userJson)
        UtoolFreeCurlResponse(getUserResponse);
    }

    if (!foundUserWithName) {
        char buffer[MAX_FAILURE_MSG_LEN];
        snprintf(buffer, MAX_FAILURE_MSG_LEN, "Failure: No user with name `%s` exists", setPasswordOption->username);
        ret = UtoolBuildStringOutputResult(STATE_FAILURE, buffer, result);
    }
    else {
        // update user
        const UtoolCurlHeader ifMatchHeader[] = {
                {.name = HEADER_IF_MATCH, .value=getUserResponse->etag},
                NULL,
        };

        ret = UtoolMakeCurlRequest(server, userAccountUrl, HTTP_PATCH, payload, ifMatchHeader, updateUserResponse);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        if (updateUserResponse->httpStatusCode >= 400) {
            ret = UtoolResolveFailureResponse(updateUserResponse, result);
            goto FAILURE;
        }

        UtoolBuildDefaultSuccessResult(result);
    }

    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(userJson)
    FREE_CJSON(userMembersJson)
    UtoolFreeCurlResponse(getUserResponse);
    UtoolFreeCurlResponse(updateUserResponse);
    UtoolFreeCurlResponse(getUserMemberResponse);
    UtoolFreeRedfishServer(server);
    return ret;
}


/**
* validate user input options for the command
*
* @param option
* @param result
* @return
*/
static int ValidateSetPasswordOptions(UtoolSetPasswordOption *option, char **result)
{
    int ret = UTOOLE_OK;
    if (option->username == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_USERNAME_REQUIRED), result);
        goto FAILURE;
    }

    if (option->password == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_PASSWORD_REQUIRED), result);
        goto FAILURE;
    }

    return ret;

FAILURE:
    option->flag = ILLEGAL;
    return ret;
}

static cJSON *BuildPayload(UtoolSetPasswordOption *option)
{
    int ret;

    cJSON *payload = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *password = cJSON_AddStringToObject(payload, "Password", option->password);
    ret = UtoolAssetCreatedJsonNotNull(password);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}