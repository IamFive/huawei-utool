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

static const char *RESET_TYPE_CHOICES[] = {"On", "ForceOff", "GracefulShutdown", "ForceReset", "Nmi",
                                           "ForcePowerCycle", NULL};

static const char *OPT_RESET_TYPE_REQUIRED = "Error: option `ResetType` is required.";
static const char *OPT_RESET_TYPE_ILLEGAL = "Error: option `ResetType` is illegal, available choices: "
                                            "On, ForceOff, GracefulShutdown, ForceReset, Nmi, ForcePowerCycle.";

static const char *const usage[] = {
        "utool powercontrol -t ResetType",
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
    cJSON *output = NULL, *payload = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('t', "ResetType", &resetType,
                        "specifies the system reset type, "
                        "choices: {On, ForceOff, GracefulShutdown, ForceReset, Nmi, ForcePowerCycle}",
                        NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ValidateSubcommandOptions(resetType, result);
    if (result->interrupt) {
        goto done;
    }

    // build payload
    payload = BuildPayload(resetType, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto done;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    UtoolRedfishPost(server, "/Systems/%s/Actions/ComputerSystem.Reset", payload, NULL, NULL, result);
    if (result->interrupt) {
        goto failure;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for setpwd command
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
        goto failure;
    }

    if (!UtoolStringInArray(resetType, RESET_TYPE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_RESET_TYPE_ILLEGAL),
                                              &(result->desc));
        goto failure;
    }

    return;

failure:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(char *resetType, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "ResetType", resetType);
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto failure;
    }

    return payload;

failure:
    FREE_CJSON(payload)
    return NULL;
}