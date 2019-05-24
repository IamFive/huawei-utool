//
// Created by qianbiao on 5/8/19.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
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
    int ret;
    UtoolCommandOptionFlag flag;  /** whether the command should be executed, default yes(0) otherwise no */
} UtoolUpdateFirmwareOption;

static const char *MODE_CHOICES[] = {"Auto", "Manual", NULL};
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

static int ValidateUpdateFirmwareOptions(UtoolUpdateFirmwareOption *updateFirmwareOption, char **result);

static int BuildPayload(UtoolRedfishServer *server, cJSON *payload, UtoolUpdateFirmwareOption *updateFirmwareOption,
                        char **result);

/**
 *
 * @param self
 * @param option
 * @return
 */
int UtoolCmdUpdateOutbandFirmware(UtoolCommandOption *commandOption, char **result)
{
    int ret = UTOOLE_OK;

    cJSON *output = NULL,           /** output result json*/
            *payload = NULL,        /** payload json */
            *updateFirmwareJson = NULL;    /** curl response thermal as json */

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};
    UtoolUpdateFirmwareOption *updateFirmwareOption = &(UtoolUpdateFirmwareOption) {.flag = EXECUTABLE};

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
    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    // validation connection options
    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    // validation update firmware options
    ret = ValidateUpdateFirmwareOptions(updateFirmwareOption, result);
    if (updateFirmwareOption->flag != EXECUTABLE) {
        goto done;
    }

    // get redfish system id
    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto failure;
    }

    // create output container
    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // build payload
    payload = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    ret = BuildPayload(server, payload, updateFirmwareOption, result);
    if (ret != UTOOLE_OK || updateFirmwareOption->flag != EXECUTABLE) {
        goto failure;
    }

    char *url = "/UpdateService/Actions/UpdateService.SimpleUpdate";
    ret = UtoolMakeCurlRequest(server, url, HTTP_POST, payload, NULL, response);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto failure;
    }

    // process response
    updateFirmwareJson = cJSON_Parse(response->content);
    ret = UtoolAssetParseJsonNotNull(updateFirmwareJson);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    // output to result
    UtoolMappingCJSONItems(updateFirmwareJson, output, getTaskMappings);
    ret = UtoolBuildOutputResult(STATE_SUCCESS, output, result);

    goto done;

failure:
    FREE_CJSON(output)
    goto done;

done:
    FREE_CJSON(payload)
    FREE_CJSON(updateFirmwareJson)
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    return ret;
}


/**
* validate user input options for update firmware command
*
* @param updateFirmwareOption
* @param result
* @return
*/
static int ValidateUpdateFirmwareOptions(UtoolUpdateFirmwareOption *updateFirmwareOption, char **result)
{
    int ret = UTOOLE_OK;
    if (updateFirmwareOption->imageURI == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_REQUIRED), result);
        goto failure;
    }

    if (updateFirmwareOption->activateMode == NULL) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_REQUIRED), result);
        goto failure;
    }

    if (!UtoolStringInArray(updateFirmwareOption->activateMode, MODE_CHOICES)) {
        ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_MODE_ILLEGAL), result);
        goto failure;
    }

    if (updateFirmwareOption->firmwareType != NULL) {
        if (!UtoolStringInArray(updateFirmwareOption->firmwareType, TYPE_CHOICES)) {
            ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_TYPE_ILLEGAL), result);
            goto failure;
        }
    }

    goto done;

failure:
    updateFirmwareOption->flag = ILLEGAL;
    goto done;

done:
    return ret;
}

static int
BuildPayload(UtoolRedfishServer *server, cJSON *payload, UtoolUpdateFirmwareOption *updateFirmwareOption, char **result)
{
    char *imageUri = updateFirmwareOption->imageURI;

    // only output if not command help action is requested.
    ZF_LOGI("Update firmware options:");
    ZF_LOGI(" ");
    ZF_LOGI("\t\tImageURI\t\t : %s", imageUri);
    ZF_LOGI("\t\tActivateMode\t : %s", updateFirmwareOption->activateMode);
    ZF_LOGI("\t\tFirmwareType\t : %s", updateFirmwareOption->firmwareType);
    ZF_LOGI(" ");

    int ret;
    struct stat fileInfo;
    UtoolParsedUrl *parsedUrl;
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
        ret = UtoolUploadFileToBMC(server, imageUri, response);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        if (response->httpStatusCode >= 400) {
            ZF_LOGE("Failed to upload local file `%s` to BMC.", imageUri);
            ret = UtoolResolveFailureResponse(response, result);
            goto failure;
        }

        char *filename = basename(imageUri);
        char uploadFilePath[MAX_FILE_PATH_LEN];
        snprintf(uploadFilePath, MAX_FILE_PATH_LEN, "/tmp/web/%s", filename);

        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", uploadFilePath);
        ret = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        goto done;
    }
    else if (UtoolStringStartsWith(imageUri, "/tmp/")) { /** handle BMC tmp file */
        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        ret = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
        goto done;
    }
    else { /** handle remote file */
        ZF_LOGI("Firmware image uri `%s` is not local file.", imageUri);

        /** parse url */
        parsedUrl = UtoolParseURL(imageUri);

        if (!parsedUrl && !UtoolStringCaseInArray(parsedUrl->scheme, IMAGE_PROTOCOL_CHOICES)) {
            ZF_LOGI("It seems the image uri is illegal.");
            ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPTION_IMAGE_URI_ILLEGAL), result);
            goto failure;
        }

        cJSON *imageUriNode = cJSON_AddStringToObject(payload, "ImageURI", imageUri);
        ret = UtoolAssetCreatedJsonNotNull(imageUriNode);
        if (ret != UTOOLE_OK) {
            goto failure;
        }

        UtoolStringToUpper(parsedUrl->scheme);
        cJSON *protocolNode = cJSON_AddStringToObject(payload, "TransferProtocol", parsedUrl->scheme);
        ret = UtoolAssetCreatedJsonNotNull(protocolNode);
        if (ret != UTOOLE_OK) {
            goto failure;
        }
    }

    goto done;

failure:
    updateFirmwareOption->flag = ILLEGAL;
    goto done;

done:
    if (imageFileFP) {                  /* close FP */
        fclose(imageFileFP);
    }
    UtoolFreeParsedURL(parsedUrl);
    UtoolFreeCurlResponse(response);    /* free response */
    return ret;
}