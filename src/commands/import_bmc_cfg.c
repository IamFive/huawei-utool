/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `importbmccfg`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
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
#include "url_parser.h"

static const char *PROTOCOL_CHOICES[] = {"HTTPS", "SCP", "SFTP", "CIFS", "NFS", NULL};
static const char *OPT_FILE_URL_ILLEGAL = "Error: option `file-uri` is illegal. Protocol is not supported.";

static const char *const usage[] = {
        "importbmccfg -u file-uri",
        NULL,
};


typedef struct _ImportBMCCfgOption
{
    char *importFileUrl;
    char *bmcTempFileUrl;
    int isLocalFile;

} UtoolImportBMCCfgOption;

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolImportBMCCfgOption *opt, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolImportBMCCfgOption *opt, UtoolResult *result);

/**
* import BMC configuration, command handler for `importbmccfg`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdImportBMCCfg(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL, *payload = NULL, *lastSuccessTaskJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolImportBMCCfgOption *opt = &(UtoolImportBMCCfgOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('u', "file-uri", &(opt->importFileUrl), "specifies path to BMC config file", NULL, 0, 0),
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

    ValidateSubcommandOptions(opt, result);
    if (result->interrupt) {
        goto DONE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(server, opt, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    char *url = "/redfish/v1/Managers/%s/Actions/Oem/Huawei/Manager.ImportConfiguration";
    UtoolRedfishPost(server, url, payload, NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // waiting util task complete or exception
    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    // lastSuccessTaskJson = result->data;
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    /* When task success */
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

    /**
    cJSON *taskState = cJSON_GetObjectItem(result->data, "TaskState");
    // if task is successfully complete
    if (taskState != NULL && UtoolStringInArray(taskState->valuestring, g_UtoolRedfishTaskSuccessStatus)) {
        ZF_LOGI("import bmc config task finished successfully");
        // output to outputStr
        UtoolBuildDefaultSuccessResult(&(result->desc));
        goto DONE;
    }
    else {
        // create output container
        output = cJSON_CreateObject();
        result->code = UtoolAssetCreatedJsonNotNull(output);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        result->code = UtoolMappingCJSONItems(lastSuccessTaskJson, output, g_UtoolGetTaskMappings);
        FREE_CJSON(result->data)
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        result->code = UtoolBuildOutputResult(STATE_FAILURE, output, &(result->desc));
        goto DONE;
    }
    */


FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_OBJ(opt->bmcTempFileUrl)
    FREE_CJSON(payload)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}


/**
* validate user input options for the command
*
* @param opt
* @param result
* @return
*/
static void ValidateSubcommandOptions(UtoolImportBMCCfgOption *opt, UtoolResult *result)
{
    UtoolParsedUrl *parsedUrl = NULL;
    FILE *uploadFileFp = NULL;
    ZF_LOGI("Import file URI is %s.", opt->importFileUrl);

    if (UtoolStringIsEmpty(opt->importFileUrl)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("file-uri")),
                                              &(result->desc));
        goto FAILURE;
    }

    /** parse url */
    parsedUrl = UtoolParseURL(opt->importFileUrl);
    if (parsedUrl) {
        opt->isLocalFile = 0;
        ZF_LOGI("It seems the import file URI is a network file. protocol is: %s.", parsedUrl->scheme);
        if (!UtoolStringCaseInArray(parsedUrl->scheme, PROTOCOL_CHOICES)) {
            ZF_LOGI("Network file protocol %s is not supported.", parsedUrl->scheme);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }
    else {
        ZF_LOGI("Could not detect schema from import file URI. Try to treat it as local file.");
        struct stat fileInfo;
        uploadFileFp = fopen(opt->importFileUrl, "rb"); /* open file to upload */
        if (!uploadFileFp) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_PATH;
            goto FAILURE;
        } /* can't continue */

        ///* get the file size */
        if (fstat(fileno(uploadFileFp), &fileInfo) != 0) {
            result->code = UTOOLE_ILLEGAL_LOCAL_FILE_SIZE;
            goto FAILURE;
        } /* can't continue */

        opt->isLocalFile = 1;
    }

    goto DONE;

FAILURE:
    result->interrupt = 1;
    goto DONE;

DONE:
    if (uploadFileFp) {                  /* close FP */
        fclose(uploadFileFp);
    }
    UtoolFreeParsedURL(parsedUrl);
}

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolImportBMCCfgOption *opt, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *node = cJSON_AddStringToObject(payload, "Type", "URI");
    result->code = UtoolAssetCreatedJsonNotNull(node);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    if (!opt->isLocalFile) {
        node = cJSON_AddStringToObject(payload, "Content", opt->importFileUrl);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }
    else {
        UtoolUploadFileToBMC(server, opt->importFileUrl, result);
        if (result->interrupt) {
            goto FAILURE;
        }

        ZF_LOGI("%s is a valid local file.", opt->importFileUrl);
        char *filename = basename(opt->importFileUrl);
        opt->bmcTempFileUrl = (char *) malloc(PATH_MAX);
        snprintf(opt->bmcTempFileUrl, PATH_MAX, "/tmp/web/%s", filename);
        node = cJSON_AddStringToObject(payload, "Content", opt->bmcTempFileUrl);
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