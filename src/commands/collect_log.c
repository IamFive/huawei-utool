/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command hander for `collect`
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
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
static const char *OPT_FILE_URL_REQUIRED = "Error: option `file-uri` is required.";
static const char *OPT_FILE_URL_ILLEGAL = "Error: option `file-uri` is illegal.";

static const char *const usage[] = {
        "collect -u file-uri",
        NULL,
};


typedef struct _CollectBoardInfoOption
{
    char *exportToFileUrl;
    char *bmcTempFileUrl;
    int isLocalFile;

} UtoolCollectBoardInfoOption;

static cJSON *BuildPayload(UtoolCollectBoardInfoOption *opt, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolCollectBoardInfoOption *opt, UtoolResult *result);

/**
* collect diagnostic information with one click, command handler for `collect`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdCollectAllBoardInfo(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL, *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCollectBoardInfoOption *opt = &(UtoolCollectBoardInfoOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('u', "file-uri", &(opt->exportToFileUrl), "specifies path to the collect file", NULL, 0, 0),
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

    // build payload
    payload = BuildPayload(opt, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    char *url = "/Managers/%s/Actions/Oem/Huawei/Manager.Dump";
    UtoolRedfishPost(server, url, payload, NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // waiting util task complete or exception
    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    ZF_LOGI("collect diagnostic information task finished successfully");
    // download file to local if necessary
    if (opt->isLocalFile) {
        ZF_LOGI("Try to download collect file from BMC now.");
        UtoolDownloadFileFromBMC(server, opt->bmcTempFileUrl, opt->exportToFileUrl, result);
        if (result->interrupt) {
            goto FAILURE;
        }
    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

    /**
    cJSON *taskState = cJSON_GetObjectItem(result->data, "TaskState");
    // if task is successfully complete
    if (taskState != NULL && UtoolStringInArray(taskState->valuestring, g_UtoolRedfishTaskSuccessStatus)) {
        ZF_LOGI("collect diagnostic information task finished successfully");
        // download file to local if necessary
        if (opt->isLocalFile) {
            ZF_LOGI("Try to download collect file from BMC now.");
            UtoolDownloadFileFromBMC(server, opt->bmcTempFileUrl, opt->exportToFileUrl, result);
            if (result->interrupt) {
                goto FAILURE;
            }
        }

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
    } */


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
static void ValidateSubcommandOptions(UtoolCollectBoardInfoOption *opt, UtoolResult *result)
{
    ZF_LOGI("Export to file URI is %s.", opt->exportToFileUrl);
    UtoolParsedUrl *parsedUrl = NULL;

    if (UtoolStringIsEmpty(opt->exportToFileUrl)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    /** parse url */
    parsedUrl = UtoolParseURL(opt->exportToFileUrl);
    if (parsedUrl) {
        opt->isLocalFile = 0;
        ZF_LOGI("It seems the export to file URI is a network file. protocol is: %s.", parsedUrl->scheme);
        if (!UtoolStringCaseInArray(parsedUrl->scheme, PROTOCOL_CHOICES)) {
            ZF_LOGI("Network file protocol %s is not supported.", parsedUrl->scheme);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }
    else {
        ZF_LOGI("Could not detect schema from export to file URI. Try to treat it as local file.");
        int fd = open(opt->exportToFileUrl, O_RDWR | O_CREAT, 0664);
        if (fd == -1) {
            ZF_LOGI("%s is not a valid local file path.", opt->exportToFileUrl);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
        else {
            opt->isLocalFile = 1;
            ZF_LOGI("%s is a valid local file.", opt->exportToFileUrl);
            if (close(fd) < 0) {
                ZF_LOGE("Failed to close fd of %s.", opt->exportToFileUrl);
                result->code = UTOOLE_INTERNAL;
                goto FAILURE;
            }
        }
    }

    goto DONE;

FAILURE:
    result->interrupt = 1;
    goto DONE;

DONE:
    UtoolFreeParsedURL(parsedUrl);
}

static cJSON *BuildPayload(UtoolCollectBoardInfoOption *opt, UtoolResult *result)
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
        node = cJSON_AddStringToObject(payload, "Content", opt->exportToFileUrl);
        result->code = UtoolAssetCreatedJsonNotNull(node);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }
    else {
        ZF_LOGI("Could not detect schema from export to file URI. Try to treat it as local file.");
        int fd = open(opt->exportToFileUrl, O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            ZF_LOGI("%s is not a valid local file path.", opt->exportToFileUrl);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
        else {
            ZF_LOGI("%s is a valid local file.", opt->exportToFileUrl);
            char *filename = basename(opt->exportToFileUrl);
            opt->bmcTempFileUrl = (char *) malloc(PATH_MAX);
            snprintf(opt->bmcTempFileUrl, PATH_MAX, "/tmp/web/%s", filename);
            node = cJSON_AddStringToObject(payload, "Content", opt->bmcTempFileUrl);
            result->code = UtoolAssetCreatedJsonNotNull(node);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }
    }

    return payload;

FAILURE:
    result->interrupt = 1;
    FREE_CJSON(payload)
    return NULL;
}