//
// Created by qianbiao on 5/8/19.
//
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

typedef struct _SetUserOption
{
    char *username;
    char *roleId;
} UtoolSetUserOption;

static const char *const OPTION_USERNAME_REQUIRED = "Error: option `Username` is required.";
static const char *const OPTION_PASSWORD_REQUIRED = "Error: option `Password` is required.";

static const char *ROLES[] = {"Administrator", "Operator", "Commonuser", "Noaccess", NULL};

static const char *const usage[] = {
        "utool setpriv -n Username -r RoleId",
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
            OPT_STRING ('n', "Username", &(setPasswordOption->username), "specifies the user to be modified", NULL, 0,
                        0),
            OPT_STRING ('r', "RoleId", &(setPasswordOption->roleId),
                        "new role, choices: {Administrator, Operator, Commonuser, Noaccess}.", NULL, 0, 0),
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
    if (result->interrupt) {
        goto FAILURE;
    }

    // build payload
    payload = BuildPayload(setPasswordOption, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/AccountService/Accounts", NULL, NULL, result);
    if (result->interrupt) {
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
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, buffer, &(result->desc));
    }
    else {
        // update user
        const UtoolCurlHeader ifMatchHeader[] = {
                {.name = HEADER_IF_MATCH, .value=getUserResponse->etag},
                NULL,
        };

        UtoolRedfishPatch(server, userAccountUrl, payload, ifMatchHeader, NULL, NULL, result);
        if (result->interrupt) {
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
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED(Username)), &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->roleId)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED(RoleId)), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->roleId, ROLES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(
                OPT_NOT_IN_CHOICE(RoleId, "Administrator, Operator, Commonuser, Noaccess")), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetUserOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *roleId = cJSON_AddStringToObject(payload, "RoleId", option->roleId);
    result->code = UtoolAssetCreatedJsonNotNull(roleId);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    result->interrupt = 1;
    FREE_CJSON(payload)
    return NULL;
}