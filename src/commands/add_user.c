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

typedef struct _AddUserOption
{
    char *username;
    char *password;
    char *roleId;
    char *privilege;
    UtoolCommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
} UtoolAddUserOption;

static const char *const OPTION_USERNAME_REQUIRED = "Error: option `Username` is required.";
static const char *const OPTION_PASSWORD_REQUIRED = "Error: option `Password` is required.";
static const char *const OPTION_ROlEID_REQUIRED = "Error: option `RoleId` is required.";
static const char *const OPTION_ROlEID_ILLEGAL = "Error: option `RoleId` is illegal, available choices: "
                                                 "Administrator, Operator, Commonuser, Noaccess.";
static const char *const OPTION_PRIVILEGE_REQUIRED = "Error: option `Privilege` is required.";

static const char *const ROLES[] = {"Administrator", "Operator", "Commonuser", "Noaccess"};

static const char *const usage[] = {
        "utool adduser -n USERNAME -p PASSWORD -r {Administrator,Operator,Commonuser,NoAccess} "
        "-l PRIVILEGE1,PRIVILEGE2",
        NULL,
};


static int ValidateAddUserOptions(UtoolAddUserOption *addUserOption, char **result);

static cJSON *BuildPayload(UtoolAddUserOption *addUserOption);

/**
 * argparse get capabilities action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolCmdAddUser(UtoolCommandOption *commandOption, char **result)
{
    int ret = OK;
    cJSON *payload = NULL;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UtoolAddUserOption *addUserOption = &(UtoolAddUserOption) {.flag = EXECUTABLE};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('n', "Username", &(addUserOption->username), "new user name", NULL, 0, 0),
            OPT_STRING ('p', "Password", &(addUserOption->password), "new user password", NULL, 0, 0),
            OPT_STRING ('r', "RoleId", &(addUserOption->roleId),
                        "new user role, choices: {Administrator,Operator,Commonuser,Noaccess}",
                        NULL, 0, 0),
            OPT_STRING ('l', "Privilege", &(addUserOption->privilege),
                        "new user privilege, choices: {None,KVM,VMM,SOL}. Multiple choices is supported by use comma.",
                        NULL, 0, 0),
            OPT_END(),
    };

    // validation
    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = ValidateAddUserOptions(addUserOption, result);
    if (addUserOption->flag != EXECUTABLE) {
        goto done;
    }

    // build payload
    payload = BuildPayload(addUserOption);
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != OK) {
        goto failure;
    }

    // get redfish system id
    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != OK || server->systemId == NULL) {
        goto failure;
    }

    ret = UtoolMakeCurlRequest(server, "/AccountService/Accounts", HTTP_POST, payload, NULL, response);
    if (ret != OK) {
        goto failure;
    }

    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto done;
    }

    UtoolBuildDefaultSuccessResult(result);
    goto done;

failure:
    goto done;

done:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    return ret;
}


/**
* validate user input options for adduser command
*
* @param addUserOption
* @param result
* @return
*/
static int ValidateAddUserOptions(UtoolAddUserOption *addUserOption, char **result)
{
    int ret = OK;
    if (addUserOption->username == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_USERNAME_REQUIRED), result);
        goto failure;
    }

    if (addUserOption->password == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_PASSWORD_REQUIRED), result);
        goto failure;
    }

    if (addUserOption->roleId == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_ROlEID_REQUIRED), result);
        goto failure;
    }

    bool found = false;
    int roleChoiceCount = sizeof(ROLES) / sizeof(ROLES[0]);
    for (int idx = 0; idx < roleChoiceCount; idx++) {
        const char *role = ROLES[idx];
        if (strncmp(role, addUserOption->roleId, strlen(role)) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_ROlEID_ILLEGAL), result);
        goto failure;
    }

//if (ret == OK && privileges == NULL) {
//    ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_PRIVILEGE_REQUIRED), result);
//}
    goto done;

failure:
    addUserOption->flag = ILLEGAL;
    goto done;

done:
    return ret;
}

static cJSON *BuildPayload(UtoolAddUserOption *addUserOption)
{
    int ret;

    cJSON *payload = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != OK) {
        goto failure;
    }

    cJSON *username = cJSON_AddStringToObject(payload, "UserName", addUserOption->username);
    ret = UtoolAssetCreatedJsonNotNull(username);
    if (ret != OK) {
        goto failure;
    }

    cJSON *password = cJSON_AddStringToObject(payload, "Password", addUserOption->password);
    ret = UtoolAssetCreatedJsonNotNull(password);
    if (ret != OK) {
        goto failure;
    }

    cJSON *roleId = cJSON_AddStringToObject(payload, "RoleId", addUserOption->roleId);
    ret = UtoolAssetCreatedJsonNotNull(roleId);
    if (ret != OK) {
        goto failure;
    }

    return payload;

failure:
    FREE_CJSON(payload)
    return NULL;
}