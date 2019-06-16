/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setsysboot`
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

static const char *BOOT_DEVICE_CHOICES[] = {"none", "HDD", "PXE", "CD", "BIOSSETUP", NULL};
static const char *BOOT_MODE_CHOICES[] = {"Legacy", "UEFI", NULL};
static const char *BOOT_EFFECTIVE_CHOICES[] = {"Disabled", "Once", "Continuous", NULL};

static const char *IBMC_BOOT_DEVICE_CHOICES[] = {BOOT_DEVICE_NONE, BOOT_DEVICE_PXE, BOOT_DEVICE_FLOPPY,
                                                 BOOT_DEVICE_CD, BOOT_DEVICE_HDD, BOOT_DEVICE_BIOSSETUP, NULL};

static const char *OPT_BOOT_DEVICE_ILLEGAL = "Error: option `boot-device` is illegal, "
                                             "available choices: none, HDD, PXE, CD, BIOSSETUP.";
static const char *OPT_EFFECTIVE_ILLEGAL = "Error: option `effective` is illegal, "
                                           "available choices: Disabled, Once, Continuous.";
static const char *OPT_BOOT_MODE_ILLEGAL = "Error: option `boot-mode` is illegal, available choices: Legacy, UEFI.";


static const char *const usage[] = {
        "setsysboot [-d boot-device] [-e effective] [-m boot-mode]",
        NULL,
};

typedef struct _SetBootSourceOverrideOption
{
    char *bootDevice;
    char *effective;
    char *bootMode;
} UtoolSetBootSourceOverrideOption;


static cJSON *BuildPayload(UtoolSetBootSourceOverrideOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetBootSourceOverrideOption *option, UtoolResult *result);

/**
* set system boot up override settings, command handler for `setsysboot`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetBootSourceOverride(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetBootSourceOverrideOption *option = &(UtoolSetBootSourceOverrideOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('d', "boot-device", &(option->bootDevice),
                        "specifies boot source override target device,"
                        "available choices: {none, HDD, PXE, CD, BIOSSETUP}", NULL, 0, 0),
            OPT_STRING ('e', "effective", &(option->effective),
                        "specifies Whether the boot settings are effective,"
                        "available choices: {Disabled, Once,Continuous}", NULL, 0, 0),
            OPT_STRING ('m', "boot-mode", &(option->bootMode),
                        "specifies level of alarm to sent, available choices: {Legacy, UEFI}",
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

    UtoolRedfishPatch(server, "/Systems/%s", payload, NULL, NULL, NULL, result);
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
static void ValidateSubcommandOptions(UtoolSetBootSourceOverrideOption *option, UtoolResult *result)
{
    if (!UtoolStringIsEmpty(option->bootDevice)) {
        if (!UtoolStringInArray(option->bootDevice, BOOT_DEVICE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_BOOT_DEVICE_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->effective)) {
        if (!UtoolStringInArray(option->effective, BOOT_EFFECTIVE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_EFFECTIVE_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->bootMode)) {
        if (!UtoolStringInArray(option->bootMode, BOOT_MODE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_BOOT_MODE_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->effective) && UtoolStringIsEmpty(option->bootMode) &&
        UtoolStringIsEmpty(option->bootDevice)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetBootSourceOverrideOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *boot = cJSON_AddObjectToObject(payload, "Boot");
    result->code = UtoolAssetCreatedJsonNotNull(boot);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->bootDevice)) {
        char *device = UtoolStringCaseFindInArray(option->bootDevice, IBMC_BOOT_DEVICE_CHOICES);
        cJSON *node = cJSON_AddStringToObject(boot, "BootSourceOverrideTarget", device);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->effective)) {
        cJSON *node = cJSON_AddStringToObject(boot, "BootSourceOverrideEnabled", option->effective);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }


    if (!UtoolStringIsEmpty(option->bootMode)) {
        cJSON *node = cJSON_AddStringToObject(boot, "BootSourceOverrideMode", option->bootMode);
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