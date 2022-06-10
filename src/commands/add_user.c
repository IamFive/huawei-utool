/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: Command Handler for `getuser`
* Author:
* Create: 2019-06-16
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

typedef struct _AddUserOption
{
    char *username;
    char *password;
    char *roleId;
    char *privilege;
    UtoolCommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
} UtoolAddUserOption;

static const char *const OPTION_USERNAME_REQUIRED = "Error: option `username` is required.";
static const char *const OPTION_PASSWORD_REQUIRED = "Error: option `password` is required.";
static const char *const OPTION_ROlEID_REQUIRED = "Error: option `role-id` is required.";
static const char *const OPTION_ROlEID_ILLEGAL = "Error: option `role-id` is illegal, available choices: "
                                                 "Administrator, Operator, Commonuser, OEM.";
static const char *const OPTION_PRIVILEGE_ILLEGAL = "Error: option `privilege` is illegal, "
                                                    "only `None` is support for now.";

static const char *ROLES[] = {"Administrator", "Operator", "Commonuser", "OEM", NULL};

static const char *const usage[] = {
        "adduser -n USERNAME -p PASSWORD -r {Administrator,Operator,Commonuser,OEM} "
        "-l PRIVILEGE1,PRIVILEGE2",
        NULL,
};


static int ValidateAddUserOptions(UtoolAddUserOption *option, char **result);

static cJSON *BuildPayload(UtoolAddUserOption *addUserOption);

/**
*
* add user command
*
* @param commandOption
* @param result
* @return
*/
int UtoolCmdAddUser(UtoolCommandOption *commandOption, char **result)
{
    int ret = UTOOLE_OK;
    cJSON *payload = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UtoolAddUserOption *addUserOption = &(UtoolAddUserOption) {.flag = EXECUTABLE};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('n', "username", &(addUserOption->username), "new user name.", NULL, 0, 0),
            OPT_STRING ('p', "password", &(addUserOption->password), "new user password.", NULL, 0, 0),
            OPT_STRING ('r', "role-id", &(addUserOption->roleId),
                        "new user role, choices: {Administrator, Operator, Commonuser, OEM}.",
                        NULL, 0, 0),
            OPT_STRING ('l', "privilege", &(addUserOption->privilege),
                        "new user privilege, choices: {None, KVM, VMM, SOL}. "
                        "Multiple choices is supported by use comma.", NULL, 0, 0),
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

    ret = ValidateAddUserOptions(addUserOption, result);
    if (addUserOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(addUserOption);
    if (payload == NULL) {
        goto FAILURE;
    }

    // get redfish system id
    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    ret = UtoolMakeCurlRequest(server, "/AccountService/Accounts", HTTP_POST, payload, NULL, response);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto DONE;
    }

    UtoolBuildDefaultSuccessResult(result);
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    return ret;
}


/**
* validate user input options for adduser command
*
* @param option
* @param result
* @return
*/
static int ValidateAddUserOptions(UtoolAddUserOption *option, char **result)
{
    int ret = UTOOLE_OK;
    if (UtoolStringIsEmpty(option->username)) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_USERNAME_REQUIRED), result);
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->password)) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_PASSWORD_REQUIRED), result);
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->roleId)) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_ROlEID_REQUIRED), result);
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->roleId, ROLES)) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_ROlEID_ILLEGAL), result);
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->privilege)) {
        if (!UtoolStringEquals(option->privilege, "None")) {
            ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_PRIVILEGE_ILLEGAL), result);
            goto FAILURE;
        }
    }

    goto DONE;

FAILURE:
    option->flag = ILLEGAL;
    goto DONE;

DONE:
    return ret;
}

static cJSON *BuildPayload(UtoolAddUserOption *addUserOption)
{
    int ret;

    cJSON *payload = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *username = cJSON_AddStringToObject(payload, "UserName", addUserOption->username);
    ret = UtoolAssetCreatedJsonNotNull(username);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *password = cJSON_AddStringToObject(payload, "Password", addUserOption->password);
    ret = UtoolAssetCreatedJsonNotNull(password);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    char *roleIdName = addUserOption->roleId;
    if (UtoolStringEquals(addUserOption->roleId, ROLE_OEM)) {
        roleIdName = ROLE_OEM_MAP;
    }
    cJSON *roleId = cJSON_AddStringToObject(payload, "RoleId", roleIdName);
    ret = UtoolAssetCreatedJsonNotNull(roleId);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}