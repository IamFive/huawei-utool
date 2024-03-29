/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command handler for `collect`
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
static const char *OPT_FILE_URL_REQUIRED = "Error: option `file-uri` is required.";
static const char *OPT_FILE_URL_ILLEGAL = "Error: option `file-uri` is illegal, Please make sure the path exists.";

static const char *const usage[] = {
        "collect -u file-uri",
        NULL,
};


typedef struct _CollectBoardInfoOption {
    char *exportToFileUrl;
    char *bmcTempFileUrl;
    char *localExportToFileUrl;
    int isLocalFile;

} UtoolCollectBoardInfoOption;

static cJSON *BuildPayload(UtoolCollectBoardInfoOption *opt, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolCollectBoardInfoOption *opt, UtoolRedfishServer *server,
                                      UtoolResult *result);

/**
* collect diagnostic information with one click, command handler for `collect`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdCollectAllBoardInfo(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL;

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

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    ValidateSubcommandOptions(opt, server, result);
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(opt, result);
    if (result->broken) {
        goto FAILURE;
    }


    char *url = "/Managers/%s/Actions/Oem/${Oem}/Manager.Dump";
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

    ZF_LOGI("collect diagnostic information task finished successfully");
    // download file to local if necessary
    if (opt->isLocalFile) {
        ZF_LOGI("Try to download collect file from BMC now.");
        UtoolDownloadFileFromBMC(server, opt->bmcTempFileUrl, opt->localExportToFileUrl, result);
        if (result->broken) {
            goto FAILURE;
        }
    }

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_OBJ(opt->localExportToFileUrl)
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
static void ValidateSubcommandOptions(UtoolCollectBoardInfoOption *opt, UtoolRedfishServer *server, UtoolResult *result)
{
    ZF_LOGD("Export to file URI is %s.", opt->exportToFileUrl);
    cJSON *getSystemRespJson = NULL;
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
    } else {
        ZF_LOGI("Could not detect schema from export to file URI. Try to treat it as local file.");

        opt->localExportToFileUrl = (char *) malloc(PATH_MAX);
        if (opt->localExportToFileUrl == NULL) {
            result->code = UTOOLE_INTERNAL;
            goto FAILURE;
        }
        char end = opt->exportToFileUrl[strnlen(opt->exportToFileUrl, PATH_MAX) - 1];
        if (end == FILEPATH_SEP) { /* Folder path */
            char nowStr[100] = {0};
            time_t now = time(NULL);
            struct tm *tm_now = localtime(&now);
            if (tm_now == NULL) {
                result->code = UTOOLE_INTERNAL;
                goto FAILURE;
            }
            strftime(nowStr, sizeof(nowStr), "%Y%m%d-%H%M", tm_now);

            char *psn = "";
            UtoolRedfishGet(server, "/Systems/%s", NULL, NULL, result);
            if (result->broken) {
                ZF_LOGE("Failed to load system resource.");
                goto FAILURE;
            }
            getSystemRespJson = result->data;
            cJSON *sn = cJSONUtils_GetPointer(getSystemRespJson, "/SerialNumber");
            if (sn != NULL && sn->valuestring != NULL) {
                ZF_LOGI("product serial number is: %s.", sn->valuestring);
                psn = sn->valuestring;
            } else {
                ZF_LOGI("product serial number is null.");
            }

            UtoolWrapSecFmt(opt->localExportToFileUrl, PATH_MAX, PATH_MAX - 1, "%sdump_%s_%s.tar.gz",
                            opt->exportToFileUrl, psn, nowStr);
        } else {
            UtoolWrapSecFmt(opt->localExportToFileUrl, PATH_MAX, PATH_MAX - 1, "%s", opt->exportToFileUrl);
        }

        int pathOk = UtoolIsParentPathExists(opt->localExportToFileUrl);
        if (!pathOk) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        char realFilepath[PATH_MAX] = {0};
        UtoolFileRealpath(opt->localExportToFileUrl, realFilepath, PATH_MAX);
        int fd = open(realFilepath, O_RDWR | O_CREAT, 0664);
        if (fd == -1) {
            ZF_LOGI("%s is not a valid local file path.", opt->localExportToFileUrl);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        } else {
            opt->isLocalFile = 1;
            ZF_LOGI("%s is a valid local file.", opt->localExportToFileUrl);
            if (close(fd) < 0) {
                ZF_LOGE("Failed to close fd of %s.", opt->localExportToFileUrl);
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
    FREE_CJSON(getSystemRespJson)
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
    } else {
        ZF_LOGI("Could not detect schema from export to file URI. Try to treat it as local file.");
        char realFilepath[PATH_MAX] = {0};
        int pathOk = UtoolIsParentPathExists(opt->localExportToFileUrl);
        if (!pathOk) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        UtoolFileRealpath(opt->localExportToFileUrl, realFilepath, PATH_MAX);
        int fd = open(realFilepath, O_RDWR | O_CREAT, 0644);
        if (fd == -1) {
            ZF_LOGI("%s is not a valid local file path.", opt->exportToFileUrl);
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_FILE_URL_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        } else {
            ZF_LOGI("%s is a valid local file.", opt->localExportToFileUrl);
            if (close(fd) < 0) {
                ZF_LOGE("Failed to close fd of %s.", opt->localExportToFileUrl);
                result->code = UTOOLE_INTERNAL;
                goto FAILURE;
            }

            char *filename = basename(opt->localExportToFileUrl);
            opt->bmcTempFileUrl = (char *) malloc(PATH_MAX);
            if (opt->bmcTempFileUrl != NULL) {
                UtoolWrapSecFmt(opt->bmcTempFileUrl, PATH_MAX, PATH_MAX - 1, "/tmp/web/%s", filename);
                node = cJSON_AddStringToObject(payload, "Content", opt->bmcTempFileUrl);
                result->code = UtoolAssetCreatedJsonNotNull(node);
                if (result->code != UTOOLE_OK) {
                    goto FAILURE;
                }
            } else {
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