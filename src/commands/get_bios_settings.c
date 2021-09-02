/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getbios`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string_utils.h>
#include <fcntl.h>
#include <unistd.h>
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"

static const char *OPT_FILE_URL_ILLEGAL = "Error: option `file-uri` is illegal, Please make sure the path exists.";

static const char *const usage[] = {
        "getbios [-f file-url]",
        NULL,
};

typedef struct _GetBiosSettingsOption {
    char *fileURI;
} UtoolGetBiosSettingsOption;


static cJSON *BuildPayload(UtoolGetBiosSettingsOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolGetBiosSettingsOption *option, UtoolResult *result);

/**
* get pending bios settings, command handler for `getbios`
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetBiosSettings(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *getBiosJson = NULL;
    FILE *outputFileFP = NULL;
    char *prettyJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *getBiosSettingResp = &(UtoolCurlResponse) {0};
    UtoolGetBiosSettingsOption *option = &(UtoolGetBiosSettingsOption) {0};
    cJSON *attributes = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('f', "file-uri", &(option->fileURI),
                        "specifies the JSON file path to store the BIOS settings.", NULL, 0, 0),
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

    ValidateSubcommandOptions(option, result);
    if (result->broken) {
        goto DONE;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    // process get bios response
    result->code = UtoolMakeCurlRequest(server, "/Systems/%s/Bios", HTTP_GET, NULL, NULL, getBiosSettingResp);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (getBiosSettingResp->httpStatusCode >= 400) {
        result->code = UtoolResolveFailureResponse(getBiosSettingResp, &(result->desc));
        goto FAILURE;
    }


    getBiosJson = cJSON_Parse(getBiosSettingResp->content);
    result->code = UtoolAssetParseJsonNotNull(getBiosJson);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    attributes = cJSON_DetachItemFromObject(getBiosJson, "Attributes");
    result->code = UtoolAssetJsonNodeNotNull(attributes, "/Attributes");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // if file-uri is specified, output to local file.
    if (!UtoolStringIsEmpty(option->fileURI)) {
        prettyJson = cJSON_Print(attributes);
        result->code = UtoolAssetPrintJsonNotNull(prettyJson);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        FREE_CJSON(attributes)

        char realFilePath[PATH_MAX] = {0};
        UtoolFileRealpath(option->fileURI, realFilePath, PATH_MAX);
        outputFileFP = fopen(realFilePath, "wb");
        if (!outputFileFP) {
            result->broken = 1;
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        }

        if (fputs(prettyJson, outputFileFP) == EOF) {
            result->broken = 1;
            result->code = UTOOLE_FAILED_TO_WRITE_FILE;
            goto FAILURE;
        }

        // mapping result to output json
        UtoolBuildDefaultSuccessResult(&(result->desc));
        goto DONE;

    } else {
        // mapping result to output json
        result->code = UtoolBuildOutputResult(STATE_SUCCESS, attributes, &(result->desc));
        goto DONE;
    }


FAILURE:
    goto DONE;

DONE:
    if (outputFileFP) {
        fclose(outputFileFP);
    }

    if (prettyJson != NULL) {
        free(prettyJson);
    }

    FREE_CJSON(getBiosJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(getBiosSettingResp);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for the command
*
* @param opt
* @param resultR
* @return
*/
static void ValidateSubcommandOptions(UtoolGetBiosSettingsOption *opt, UtoolResult *result)
{
    if (!UtoolStringIsEmpty(opt->fileURI)) {
        int pathOk = UtoolIsParentPathExists(opt->fileURI);
        if (!pathOk) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        char realFilePath[PATH_MAX] = {0};
        UtoolFileRealpath(opt->fileURI, realFilePath, PATH_MAX);
        int fd = open(realFilePath, O_RDWR | O_CREAT, 0664);
        if (fd == -1) {
            ZF_LOGI("%s is not a valid local file path.", opt->fileURI);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        } else {
            ZF_LOGI("%s is a valid local file.", opt->fileURI);
            if (close(fd) < 0) {
                ZF_LOGE("Failed to close fd of %s.", opt->fileURI);
                result->code = UTOOLE_INTERNAL;
                goto FAILURE;
            }
        }
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}
