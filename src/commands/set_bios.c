/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setbios`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
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

#define MB (1024 * 1024)

static const char *OPT_VALUE_REQUIRED = "Error: option `value` is required when `attribute` option present.";
static const char *OPT_ATTR_REQUIRED = "Error: option `attribute` is required when `value` option present.";
static const char *OPT_FILE_TOO_LARGE = "Error: input JSON file should not large than 1 MB.";
static const char *OPT_JSON_FILE_ILLEGAL = "Error: input JSON file is not well formed.";

static const char *const usage[] = {
        "setbios [-a attribute] [-v value] [-f file-uri]",
        NULL,
};

typedef struct _SetBiosAttrOption
{
    char *attr;
    char *value;
    char *fileURI;
} UtoolSetBiosAttrOption;

static cJSON *BuildPayload(UtoolSetBiosAttrOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetBiosAttrOption *option, UtoolResult *result);

/**
* set BIOS attributes, command handler for `setbios`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetBIOS(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetBiosAttrOption *option = &(UtoolSetBiosAttrOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('a', "attribute", &(option->attr),
                        "specifies the BIOS attribute to update", NULL, 0, 0),
            OPT_STRING ('v', "value", &(option->value),
                        "specifies the value of BIOS attribute to update", NULL, 0, 0),
            OPT_STRING ('f', "file-uri", &(option->fileURI),
                        "specifies JSON file URI which indicates BIOS attribute and value pairs to update.", NULL, 0,
                        0),
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
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishPatch(server, "/Systems/%s/Bios/Settings", payload, NULL, NULL, NULL, result);
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
static void ValidateSubcommandOptions(UtoolSetBiosAttrOption *option, UtoolResult *result)
{
    FILE *uploadFileFp = NULL;
    if (!UtoolStringIsEmpty(option->attr) && UtoolStringIsEmpty(option->value)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_VALUE_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringIsEmpty(option->attr) && !UtoolStringIsEmpty(option->value)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_ATTR_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    /** check whether file exists */
    if (!UtoolStringIsEmpty(option->fileURI)) {
        ZF_LOGI("Validate file %s now.", option->fileURI);
        struct stat fileInfo;

        char realFilePath[PATH_MAX] = {0};
        char *ok = UtoolFileRealpath(option->fileURI, realFilePath);
        if (ok == NULL) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        }

        uploadFileFp = fopen(realFilePath, "rb"); /* open file to upload */
        if (!uploadFileFp) {
            ZF_LOGE("Could not open file %s.", option->fileURI);
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        } /* can't continue */

        ///* get the file size */
        if (fstat(fileno(uploadFileFp), &fileInfo) != 0) {
            ZF_LOGE("Could not stat file %s.", option->fileURI);
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
            goto FAILURE;
        } /* can't continue */

        if (fileInfo.st_size > MB) {
            ZF_LOGE("File %s is too larger(> 1 MB).", option->fileURI);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_TOO_LARGE),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->attr) && UtoolStringIsEmpty(option->value) && UtoolStringIsEmpty(option->fileURI)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    goto DONE;


FAILURE:
    result->broken = 1;
    goto DONE;

DONE:
    if (uploadFileFp != NULL) {
        fclose(uploadFileFp);
    }
}

static cJSON *BuildPayload(UtoolSetBiosAttrOption *option, UtoolResult *result)
{
    cJSON *attributes = NULL;
    cJSON *payload = NULL;
    FILE *infile = NULL;
    char *fileContent = NULL;
    size_t numbytes;

    attributes = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(attributes);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->fileURI)) {
        ZF_LOGI("Try to parse file %s now.", option->fileURI);
        char realFilePath[PATH_MAX] = {0};
        char *ok = UtoolFileRealpath(option->fileURI, realFilePath);
        if (ok == NULL) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        }
        infile = fopen(realFilePath, "rb");
        if (!infile) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        } /* can't continue */

        /* Get the number of bytes */
        fseek(infile, 0L, SEEK_END);
        numbytes = ftell(infile);
        /* reset the file position indicator to the beginning of the file */
        fseek(infile, 0L, SEEK_SET);

        /* grab sufficient memory for the buffer to hold the text */
        fileContent = (char *) calloc(numbytes + 1, sizeof(char));
        /* memory error */
        if (fileContent == NULL) {
            result->code = UTOOLE_INTERNAL;
            goto FAILURE;
        }

        /* copy all the text into the buffer */
        size_t size = fread(fileContent, sizeof(char), numbytes, infile);
        fileContent[numbytes] = '\0';
        fclose(infile);
        infile = NULL;
        ZF_LOGI("File content is %s", fileContent);

        /** parse file content */
        payload = cJSON_Parse(fileContent);
        if (size != numbytes || !payload) {
            ZF_LOGI("File format is illegal, not well json formed, position: %s.", cJSON_GetErrorPtr());
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_JSON_FILE_ILLEGAL), &(result->desc));
            goto FAILURE;
        }
    }

    if (payload == NULL) {
        payload = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(payload);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->attr)) {
        cJSON *newNode = cJSON_AddStringToObject(payload, option->attr, option->value);
        result->code = UtoolAssetCreatedJsonNotNull(newNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    cJSON_AddItemToObject(attributes, "Attributes", payload);
    goto DONE;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    FREE_CJSON(attributes)
    goto DONE;

DONE:
    FREE_OBJ(fileContent)
    if (infile != NULL) {
        fclose(infile);
    }
    return attributes;
}