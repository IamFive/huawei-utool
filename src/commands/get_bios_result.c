/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command hander for `getbiosresult`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "string_utils.h"
#include "cJSON_Utils.h"
#include "commons.h"
#include "curl/curl.h"
#include "zf_log.h"
#include "constants.h"
#include "command-helps.h"
#include "command-interfaces.h"
#include "argparse.h"
#include "redfish.h"

static const char *const usage[] = {
        "getbiosresult",
        NULL,
};

static int ApplyResultPropertyHandler(cJSON *target, const char *key, cJSON *node) {

    cJSON *newNode;
    if (cJSON_IsNull(node) || (cJSON_IsArray(node) && cJSON_GetArraySize(node) == 0)) {
        newNode = cJSON_AddStringToObject(target, key, "Success");
        return UTOOLE_OK;
    }

    newNode = cJSON_AddStringToObject(target, key, "Failure");
    FREE_CJSON(node)

    return UtoolAssetCreatedJsonNotNull(newNode);
}

static int ApplyDetailPropertyHandler(cJSON *target, const char *key, cJSON *node) {
    if (cJSON_IsNull(node) || (cJSON_IsArray(node) && cJSON_GetArraySize(node) == 0)) {
        cJSON_AddItemToObject(target, key, node);
        return UTOOLE_OK;
    }

    int ret = UTOOLE_OK;
    cJSON *item = NULL;
    cJSON *convertedItem = NULL;
    cJSON *outputArray = cJSON_CreateArray();

    cJSON_ArrayForEach(item, node) {
        cJSON *messageIdNode = cJSON_GetObjectItem(item, "MessageId");
        cJSON *relatedPropertiesNode = cJSON_GetObjectItem(item, "RelatedProperties");
        ret = UtoolAssetJsonNodeNotNull(messageIdNode, "/@Redfish.Settings/Messages/*/MessageId");
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }
        ret = UtoolAssetJsonNodeNotNull(relatedPropertiesNode, "/@Redfish.Settings/Messages/*/RelatedProperties");
        if (ret != UTOOLE_OK) {
            goto FAILURE;
        }

        cJSON *propertyNode = NULL;
        cJSON_ArrayForEach(propertyNode, relatedPropertiesNode) {
            convertedItem = cJSON_CreateObject();
            ret = UtoolAssetCreatedJsonNotNull(convertedItem);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            char *propertyStr = UtoolStringLastSplit(propertyNode->valuestring, '/');
            cJSON *attrNode = cJSON_AddStringToObject(convertedItem, "Attribute", propertyStr);
            ret = UtoolAssetCreatedJsonNotNull(attrNode);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            cJSON *reasonNode = cJSON_AddStringToObject(convertedItem, "FailReason", messageIdNode->valuestring);
            ret = UtoolAssetCreatedJsonNotNull(reasonNode);
            if (ret != UTOOLE_OK) {
                goto FAILURE;
            }

            cJSON_AddItemToArray(outputArray, convertedItem);
        }
    }

    cJSON_AddItemToObject(target, key, outputArray);

FAILURE:
    FREE_CJSON(outputArray)
    FREE_CJSON(convertedItem)
    FREE_CJSON(node)
    return ret;
}

/**
   Bios Apply result messages:
   {"Messages":[
        {
        "MessageId":"PropertyUnknown",
        "RelatedProperties":["#/Attribute/BootTypexxx","#/Attribute/CutomPowerPolicyyyy"]
        },
        {
        "MessageId":"PropertyValueNotInList",
        "RelatedProperties":["#/Attribute/BootPState","#/Attribute/PowerSaving"]
        },
        {
        "MessageId":"PropertyValueTypeError",
        "RelatedProperties":["#/Attribute/TurboMode"]
        }
    ]}
*/
static const UtoolOutputMapping getBiosResultMapping[] = {
        {
                .sourceXpath = "/@Redfish.Settings/Time",
                .targetKeyValue="ApplyTime"
        },
        {
                .sourceXpath = "/@Redfish.Settings/Messages",
                .targetKeyValue="ApplyResult",
                .handle=ApplyResultPropertyHandler
        },
        {
                .sourceXpath = "/@Redfish.Settings/Messages",
                .targetKeyValue="ApplyDetail",
                .handle=ApplyDetailPropertyHandler
        },
        NULL
};


/**
* get bios config result, command handler for `getbiosresult`
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetBiosResult(UtoolCommandOption *commandOption, char **outputStr) {
    cJSON *output = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
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

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s/Bios", output, getBiosResultMapping, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));

    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
