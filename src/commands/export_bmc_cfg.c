/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command hander for `exportbmccfg`
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
#include <securec.h>
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
static const char *OPT_FILE_URL_REQUIRED = "Error: opt `file-uri` is required.";
static const char *OPT_FILE_URL_ILLEGAL = "Error: option `file-uri` is illegal.";

static const char *const usage[] = {
        "exportbmccfg -u file-uri",
        NULL,
};


typedef struct _ExportBMCCfg {
    char *exportToFileUrl;
    char *bmcTempFileUrl;
    int isLocalFile;

} UtoolExportBMCCfg;

static cJSON *BuildPayload(UtoolExportBMCCfg *opt, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolExportBMCCfg *opt, UtoolResult *result);

/**
* export BMC configuration, command handler for `exportbmccfg`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdExportBMCCfg(UtoolCommandOption *commandOption, char **outputStr) {
    cJSON *output = NULL, *payload = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolExportBMCCfg *opt = &(UtoolExportBMCCfg) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('u', "file-uri", &(opt->exportToFileUrl), "specifies path of export file", NULL, 0, 0),
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
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(opt, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    char *url = "/redfish/v1/Managers/%s/Actions/Oem/${Oem}/Manager.ExportConfiguration";
    UtoolRedfishPost(server, url, payload, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }

    // waiting util task complete or exception
    UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    ZF_LOGI("export BMC config xml task finished successfully");
    if (opt->isLocalFile) { /* download file to local if necessary */
        ZF_LOGI("Try to download BMC config xml file from BMC now.");
        UtoolDownloadFileFromBMC(server, opt->bmcTempFileUrl, opt->exportToFileUrl, result);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_OBJ(opt->bmcTempFileUrl)
    UtoolFreeRedfishServer(server);

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
static void ValidateSubcommandOptions(UtoolExportBMCCfg *opt, UtoolResult *result) {
    UtoolParsedUrl *parsedUrl = NULL;
    ZF_LOGD("Export to file URI is %s.", opt->exportToFileUrl);

    if (UtoolStringIsEmpty(opt->exportToFileUrl)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("file-uri")),
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
        char realFilePath[PATH_MAX] = {0};
        UtoolFileRealpath(opt->exportToFileUrl, realFilePath);
        if (realFilePath == NULL) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
        int fd = open(realFilePath, O_RDWR | O_CREAT, 0664);
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
    result->broken = 1;
    goto DONE;

DONE:
    UtoolFreeParsedURL(parsedUrl);
}

static cJSON *BuildPayload(UtoolExportBMCCfg *opt, UtoolResult *result) {
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
        char realFilepath[PATH_MAX] = {0};
        UtoolFileRealpath(opt->exportToFileUrl, realFilepath);
        if (realFilepath == NULL) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        int fd = open(realFilepath, O_RDWR | O_CREAT, 0644);
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
            if (opt->bmcTempFileUrl != NULL) {
                snprintf_s(opt->bmcTempFileUrl, PATH_MAX, PATH_MAX, "/tmp/web/%s", filename);
                node = cJSON_AddStringToObject(payload, "Content", opt->bmcTempFileUrl);
                result->code = UtoolAssetCreatedJsonNotNull(node);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            }
            else {
                ZF_LOGI("Could malloc memory for bmc temp file url");
                result->code = UTOOLE_INTERNAL;
                goto FAILURE;
            }
        }
    }

    return payload;

FAILURE:
    result->broken = 1;
    FREE_CJSON(payload)
    return NULL;
}