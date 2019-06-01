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

static const char *OPT_ENABLED_ILLEGAL = "Error: option `SSLEnabled` is illegal, available choices: Enabled, Disabled";
static const char *OPT_TIMEOUT_ILLEGAL = "Error: option `Timeout` is illegal, available value range: 0~480.";

static const char *const usage[] = {
        "utool setvnc [-e SSLEnabled] [-t Timeout] [-p Password]",
        NULL,
};

typedef struct _UpdateVNCSettings
{
    int timeout;
    char *sslEnabled;
    char *password;
} UtoolUpdateVNCSettings;

static cJSON *BuildPayload(UtoolUpdateVNCSettings *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolUpdateVNCSettings *option, UtoolResult *result);

/**
* set VNC setting, command handler for `setvnc`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetVNCSettings(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolUpdateVNCSettings *option = &(UtoolUpdateVNCSettings) {.timeout=DEFAULT_INT_V};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('e', "SSLEnabled", &(option->sslEnabled),
                        "specifies Whether SSL encryption is enabled, available choices: {Enabled, Disabled}",
                        NULL, 0, 0),
            OPT_INTEGER('t', "Timeout", &(option->timeout),
                        "specifies VNC session timeout period in minute, value range: 0~480.", NULL, 0, 0),
            OPT_STRING ('p', "Password", &(option->password),
                        "specifies VNC password.",
                        NULL, 0, 0),
            OPT_END()
    };

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateSubcommandOptions(option, result);
    if (result->interrupt) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPatch(server, "/Managers/%s/VNCService", payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
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
static void ValidateSubcommandOptions(UtoolUpdateVNCSettings *option, UtoolResult *result)
{
    if (!UtoolStringIsEmpty(option->sslEnabled)) {
        if (!UtoolStringInArray(option->sslEnabled, UTOOL_ENABLED_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ENABLED_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (option->timeout != DEFAULT_INT_V) {
        if (option->timeout < 0 || option->timeout > 480) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_TIMEOUT_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->sslEnabled) && option->timeout == DEFAULT_INT_V &&
        UtoolStringIsEmpty(option->password)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolUpdateVNCSettings *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->sslEnabled)) {
        cJSON *node = cJSON_AddBoolToObject(payload, "SSLEncryptionEnabled", UtoolStringEquals(option->sslEnabled, ENABLED));
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->password)) {
        cJSON *node = cJSON_AddStringToObject(payload, "Password", option->password);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (option->timeout != DEFAULT_INT_V) {
        cJSON *node = cJSON_AddNumberToObject(payload, "SessionTimeoutMinutes", option->timeout);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}