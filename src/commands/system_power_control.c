/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `powercontrol`
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

static const char *RESET_TYPE_CHOICES[] = {"On", "ForceOff", "GracefulShutdown", "ForceRestart", "Nmi",
                                           "ForcePowerCycle", NULL};

static const char *OPT_RESET_TYPE_REQUIRED = "Error: option `reset-type` is required.";
static const char *OPT_RESET_TYPE_ILLEGAL = "Error: option `reset-type` is illegal, available choices: "
                                            "On, ForceOff, GracefulShutdown, ForceRestart, Nmi, ForcePowerCycle.";

static const char *const usage[] = {
        "powercontrol -t reset-type",
        NULL,
};

static cJSON *BuildPayload(char *resetType, UtoolResult *result);

static void ValidateSubcommandOptions(char *resetType, UtoolResult *result);

/**
* OS system power control, command handler for `powercontrol`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSystemPowerControl(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    char *resetType = NULL;
    // initialize output objects
    cJSON *payload = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('t', "reset-type", &resetType,
                        "specifies the system reset type, "
                        "choices: {On, ForceOff, GracefulShutdown, ForceRestart, Nmi, ForcePowerCycle}",
                        NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubcommandOptions(resetType, result);
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(resetType, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPost(server, "/Systems/%s/Actions/ComputerSystem.Reset", payload, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
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
static void ValidateSubcommandOptions(char *resetType, UtoolResult *result)
{
    if (UtoolStringIsEmpty(resetType)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_RESET_TYPE_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(resetType, RESET_TYPE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_RESET_TYPE_ILLEGAL),
                                              &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(char *resetType, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "ResetType", resetType);
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}