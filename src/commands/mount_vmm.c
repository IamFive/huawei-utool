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

#define OP_MOUNT "Mount"
#define OP_UNMOUNT "Unmount"

#define VMM_CTRL_CONNECT "Connect"
#define VMM_CTRL_DISCONNECT "Disconnect"

typedef struct _MountVMMOption
{
    char *operatorType;
    char *image;
} UtoolMountVMMOption;

static const char *TYPE_CHOICES[] = {OP_MOUNT, OP_UNMOUNT, NULL};

static const char *OPT_OP_TYPE_REQUIRED = "Error: option `OperatorType` is required.";
static const char *OPT_OP_ILLEGAL = "Error: option `OperatorType` is illegal, available choices: Mount, Unmount.";
static const char *const OPT_IMAGE_REQUIRED = "Error: option `Image` is required when `OperatorType` is Mount.";

static const char *const usage[] = {
        "utool mountvmm -o OperatorType [-i Image]",
        NULL,
};


static void ValidateMountVMMOptions(UtoolMountVMMOption *option, UtoolResult *result);

static cJSON *BuildPayload(UtoolMountVMMOption *option);

/**
* mount or unmount virtual media, command handler for `mountvmm`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdMountVMM(UtoolCommandOption *commandOption, char **outputStr)
{
    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolMountVMMOption *mountVMMOptions = &(UtoolMountVMMOption) {0};

    // initialize output objects
    cJSON *output = NULL, *payload = NULL;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('o', "OperatorType", &(mountVMMOptions->operatorType),
                        "specifies the operation type, choices: {Mount, Unmount}", NULL, 0, 0),
            OPT_STRING ('i', "Image", &(mountVMMOptions->image),
                        "specifies the VRI of the virtual media image", NULL, 0, 0),
            OPT_END()
    };

    // validation
    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto DONE;
    }

    ValidateMountVMMOptions(mountVMMOptions, result);
    if (result->interrupt) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(mountVMMOptions);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }


    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    char *url = "/Managers/%s/VirtualMedia/CD/Oem/Huawei/Actions/VirtualMedia.VmmControl";
    UtoolRedfishPost(server, url, payload, output, utoolGetTaskMappings, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    //UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
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
static void ValidateMountVMMOptions(UtoolMountVMMOption *option, UtoolResult *result)
{
    if (option->operatorType == NULL) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_OP_TYPE_REQUIRED), &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringInArray(option->operatorType, TYPE_CHOICES)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_OP_ILLEGAL), &(result->desc));
        goto FAILURE;
    }

    if (UtoolStringEquals(option->operatorType, OP_MOUNT)) {
        if (option->image == NULL) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_IMAGE_REQUIRED),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolMountVMMOption *option)
{
    int ret;

    cJSON *payload = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(payload);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    if (UtoolStringEquals(option->operatorType, OP_MOUNT)) {
        cJSON *vmmControlType = cJSON_AddStringToObject(payload, "VmmControlType", VMM_CTRL_CONNECT);
        ret = UtoolAssetCreatedJsonNotNull(vmmControlType);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *vmmVRI = cJSON_AddStringToObject(payload, "Image", option->image);
        ret = UtoolAssetCreatedJsonNotNull(vmmVRI);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
    }
    else {
        cJSON *vmmControlType = cJSON_AddStringToObject(payload, "VmmControlType", VMM_CTRL_DISCONNECT);
        ret = UtoolAssetCreatedJsonNotNull(vmmControlType);
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
    }

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}