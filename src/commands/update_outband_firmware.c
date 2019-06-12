//
// Created by qianbiao on 5/8/19.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
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


typedef struct _UpdateFirmwareOption
{
    char *imageURI;
    char *activateMode;
    char *firmwareType;
} UtoolUpdateFirmwareOption;

static const char *MODE_CHOICES[] = {UPGRADE_ACTIVATE_MODE_AUTO, UPGRADE_ACTIVATE_MODE_MANUAL, NULL};
static const char *TYPE_CHOICES[] = {"BMC", "BIOS", "CPLD", "PSUFW", NULL};
static const char *IMAGE_PROTOCOL_CHOICES[] = {"HTTPS", "SCP", "SFTP", "CIFS", "TFTP", "NFS", NULL};

static const char *OPTION_IMAGE_URI_REQUIRED = "Error: option `ImageURI` is required.";
static const char *OPTION_IMAGE_URI_ILLEGAL = "Error: option `ImageURI` is illegal.";
static const char *OPTION_MODE_REQUIRED = "Error: option `ActivateMode` is required.";
static const char *OPTION_MODE_ILLEGAL = "Error: option `ActivateMode` is illegal, available choices: Auto, Manual.";
static const char *OPTION_TYPE_ILLEGAL = "Error: option `FirmwareType` is illegal, "
                                         "available choices: BMC, BIOS, CPLD, PSUFW.";

static const char *const usage[] = {
        "fwupdate -u ImageURI -e ActivateMode [-t FirmwareType]",
        NULL,
};

static void ValidateUpdateFirmwareOptions(UtoolUpdateFirmwareOption *updateFirmwareOption, UtoolResult *result);

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolUpdateFirmwareOption *updateFirmwareOption,
                           UtoolResult *result);

static int ResetBMCAndWaitingAlive(UtoolRedfishServer *server);

/**
 * 要求：升级失败要求能重试，最多3次。
 *
 * 如果升级BMC带了-e Auto参数，且出现BMC的HTTP不通或空间不足等情况时要求utool能直接先将BMC重启后再试。
 * 升级过程详细记录到升级日志文件，内容及格式详见sheet "升级日志文件"。
 *
 * @param self
 * @param option
 * @return
 */
int UtoolCmdUpdateOutbandFirmware(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *output = NULL,           /** output result json*/
            *payload = NULL,        /** payload json */
            *updateFirmwareJson = NULL;    /** curl response thermal as json */

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UtoolUpdateFirmwareOption *updateFirmwareOption = &(UtoolUpdateFirmwareOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('u', "ImageURI", &(updateFirmwareOption->imageURI), "firmware image file URI", NULL, 0, 0),
            OPT_STRING ('e', "ActivateMode", &(updateFirmwareOption->activateMode),
                        "firmware active mode, choices: {Auto, Manual}", NULL, 0, 0),
            OPT_STRING ('t', "FirmwareType", &(updateFirmwareOption->firmwareType),
                        "firmware type, choices: {BMC, BIOS, CPLD, PSUFW}",
                        NULL, 0, 0),
            OPT_END(),
    };

    // validation sub command options
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // validation connection options
    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    // validation update firmware options
    ValidateUpdateFirmwareOptions(updateFirmwareOption, result);
    if (result->interrupt) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    // create output container
    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    int retryTimes = 0;
    cJSON *cJSONTask = NULL;
    while (retryTimes++ < UPGRADE_FIRMWARE_RETRY_TIMES) {
        ZF_LOGI("Start to upgrade outband firmware now, retry round: %d", retryTimes);

        if (result->desc != NULL) {
            FREE_OBJ(result->desc);
        }

        if (result->data != NULL) {
            FREE_CJSON(result->data)
        }

        payload = BuildPayload(server, updateFirmwareOption, result);
        if (payload == NULL || result->interrupt) {
            // TODO should we only retry when BMC return failure?
            if (result->retryable) {
                if (UtoolStringEquals(UPGRADE_ACTIVATE_MODE_AUTO, updateFirmwareOption->activateMode)) {
                    ZF_LOGI("Failed to upload file to BMC and activate mode is auto, will try reset BMC now.");
                    // we do not care about whether server is alive?
                    // ResetBMCAndWaitingAlive(server);
                }
            }
            continue;
        }

        char *url = "/UpdateService/Actions/UpdateService.SimpleUpdate";
        UtoolRedfishPost(server, url, payload, NULL, NULL, result);
        if (result->interrupt) {
            continue;
        }

        // waiting util task complete or exception
        UtoolRedfishWaitUtilTaskFinished(server, result->data, result);
        if (!result->interrupt) {
            break;
        }
        // TODO 是否需要判断task的状态？如果状态异常也是需要自动重试？
    }

    // if it still fails when reaching the maximum retries
    if (result->interrupt) {
        FREE_CJSON(result->data)
        goto FAILURE;
    }

    // output to result
    result->code = UtoolMappingCJSONItems(result->data, output, utoolGetTaskMappings);
    FREE_CJSON(result->data)
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(updateFirmwareJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);

    *outputStr = result->desc;
    return result->code;
}

static int ResetBMCAndWaitingAlive(UtoolRedfishServer *server)
{
    int ret;
    cJSON *payload;
    UtoolCurlResponse *resetManagerResp = &(UtoolCurlResponse) {0};

    ZF_LOGI("Restart BMC and waiting alive");

    // build payload
    char *payloadString = "{\"ResetType\" : \"ForceRestart\" }";
    payload = cJSON_CreateRaw(payloadString);
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto DONE;
    }

    // we do not care about whether reset manager is successful
    char *url = "/Managers/%s/Actions/Manager.Reset";
    UtoolMakeCurlRequest(server, url, HTTP_POST, payload, NULL, resetManagerResp);
    sleep(5);  // wait 5 seconds to let manager resetting takes effect

    ZF_LOGI("Waiting BMC alive now...");
    // waiting BMC up, total wait time is (30 + 1) * 30/2
    int interval = 30;
    while (interval > 0) {
        UtoolCurlResponse *getRedfishResp = &(UtoolCurlResponse) {0};
        ret = UtoolMakeCurlRequest(server, "/", HTTP_GET, NULL, NULL, getRedfishResp);
        if (ret == UTOOLE_OK && getRedfishResp->httpStatusCode < 300) {
            ZF_LOGI("BMC is alive now.");
            UtoolFreeCurlResponse(getRedfishResp);
            break;
        }

        ZF_LOGI("BMC is down now. Next check will be %d seconds later.", interval);
        sleep(interval);
        interval--;
        UtoolFreeCurlResponse(getRedfishResp);
    }

DONE:
    FREE_CJSON(payload)
    UtoolFreeCurlResponse(resetManagerResp);
    return ret;
}


/**
* validate user input options for update firmware command
*
* @param updateFirmwareOption
* @param result
* @return
*/
static void ValidateUpdateFirmwareOptions(UtoolUpdateFirmwareOption *updateFirmwareOption, UtoolResult *result)
{
    if (updateFirmwareOption->imageURI == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_REQUIRED),
                                              &(result->desc));
        goto FAILURE;
    }

    if (updateFirmwareOption->activateMode == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_REQUIRED), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(updateFirmwareOption->activateMode, MODE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_ILLEGAL), &(result->desc));
        goto FAILURE;
    }

    if (updateFirmwareOption->firmwareType != NULL) {
        if (!UtoolStringInArray(updateFirmwareOption->firmwareType, TYPE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_TYPE_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolRedfishServer *server, UtoolUpdateFirmwareOption *updateFirmwareOption,
                           UtoolResult *result)
{

    // build payload
    cJSON *payload = cJSON_CreateObject();

    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    char *imageUri = updateFirmwareOption->imageURI;

    // only output if not command help action is requested.
    ZF_LOGI("Update firmware options:");
    ZF_LOGI(" ");
    ZF_LOGI("\t\tImageURI\t\t : %s", imageUri);
    ZF_LOGI("\t\tActivateMode\t : %s", updateFirmwareOption->activateMode);
    ZF_LOGI("\t\tFirmwareType\t : %s", updateFirmwareOption->firmwareType);
    ZF_LOGI(" ");

    struct stat fileInfo;
    UtoolParsedUrl *parsedUrl = NULL;
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    bool isLocalFile = false;
    /** try to treat imageURI as a local file */
    FILE *imageFileFP = fopen(imageUri, "rb"); /* open file to upload */
    if (imageFileFP) {
        if (fstat(fileno(imageFileFP), &fileInfo) == 0) {
            isLocalFile = true;
        }
    }

    if (isLocalFile) {  /** if file exists in local machine, try to upload it to BMC */
        ZF_LOGI("Firmware image uri `%s` is a local file.", imageUri);

        result->code = UtoolUploadFileToBMC(server, imageUri, response);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        if (response->httpStatusCode >= 400) {
            ZF_LOGE("Failed to upload local file `%s` to BMC.", imageUri);
            result->code = UtoolResolveFailureResponse(response, &(result->desc));
            result->retryable = 1; // should we only retry when BMC return failure?
            goto FAILURE;
        }

        char *filename = basename(imageUri);
        char uploadFilePath[MAX_FILE_PATH_LEN];
        snprintf(uploadFilePath, MAX_FILE_PATH_LEN, "/tmp/web/%s", filename);

        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", uploadFilePath);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        goto DONE;
    }
    else if (UtoolStringStartsWith(imageUri, "/tmp/")) { /** handle BMC tmp file */
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
        goto DONE;
    }
    else { /** handle remote file */
        ZF_LOGI("Firmware image uri `%s` is not local file.", imageUri);

        /** parse url */
        parsedUrl = UtoolParseURL(imageUri);

        if (!parsedUrl || !UtoolStringCaseInArray(parsedUrl->scheme, IMAGE_PROTOCOL_CHOICES)) {
            ZF_LOGI("It seems the image uri is illegal.");
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_ILLEGAL),
                                                  &(result->desc));
            goto FAILURE;
        }

        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        result->code = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }

        UtoolStringToUpper(parsedUrl->scheme);
        cJSON *protocolNode = cJSON_AddStringToObject(payload, "TransferProtocol", parsedUrl->scheme);
        result->code = UtoolAssetCreatedJsonNotNull(protocolNode);
        if (result->code != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    goto DONE;

FAILURE:
    result->interrupt = 1;
    FREE_CJSON(payload)
    payload = NULL;
    goto DONE;

DONE:
    if (imageFileFP) {                  /* close FP */
        fclose(imageFileFP);
    }
    UtoolFreeParsedURL(parsedUrl);
    UtoolFreeCurlResponse(response);    /* free response */
    return payload;
}