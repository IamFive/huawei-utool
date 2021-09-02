/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler of `getpcie`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string_utils.h>
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

#define CARD_TYPE_GPU "GPU"
#define CARD_TYPE_RAID "RAID"
#define CARD_TYPE_ACCELERATION "Acceleration"
#define CARD_TYPE_FPGA "FPGA"
#define CARD_TYPE_NET "NET"
#define CARD_TYPE_NIC "NIC"
#define CARD_TYPE_HBA "HBA"

static const char *const usage[] = {
        "getpcie",
        NULL,
};

/**
 *
 * @param server
 * @param target
 * @param key
 * @param root      root Node
 * @return
 */
static int TypeHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *root)
{
    char funcTypePath[MAX_XPATH_LEN] = {0};
    UtoolWrapSecFmt(funcTypePath, MAX_XPATH_LEN, MAX_XPATH_LEN - 1, "/Oem/%s/FunctionType", server->oemName);

    char cardTypePath[MAX_XPATH_LEN] = {0};
    UtoolWrapSecFmt(cardTypePath, MAX_XPATH_LEN, MAX_XPATH_LEN - 1, "/Oem/%s/PCIeCardType", server->oemName);

    cJSON *funcTypeNode = cJSONUtils_GetPointer(root, funcTypePath);
    cJSON *cardTypeNode = cJSONUtils_GetPointer(root, cardTypePath);

    if (cJSON_IsNull(funcTypeNode) || funcTypeNode->valuestring == NULL) {
        cJSON_AddItemToObjectCS(target, key, cJSON_CreateNull());
        FREE_CJSON(root)
        return UTOOLE_OK;
    }

    /**
        GPU Card -> GPU
        RAID Card -> RAID
        Accelerate Card -> Acceleration
        FPGA Card -> FPGA

        Net Card -> NIC/HBA
        Net Card 需要进一步根据 PcieCardType（该属性会在630 iBMC版本实现，届时可以联调） 区分：
        NIC -> NIC
        IB -> NIC
        FC -> HBA
        如果获取不到 PcieCardType 属性，则：
        Net Card -> NET
     */
    char *mapping = funcTypeNode->valuestring;
    if (UtoolStringEquals(funcTypeNode->valuestring, "GPU Card")) {
        mapping = CARD_TYPE_GPU;
    } else if (UtoolStringEquals(funcTypeNode->valuestring, "RAID Card")) {
        mapping = CARD_TYPE_RAID;
    } else if (UtoolStringEquals(funcTypeNode->valuestring, "Accelerate Card")) {
        mapping = CARD_TYPE_ACCELERATION;
    } else if (UtoolStringEquals(funcTypeNode->valuestring, "FPGA Card")) {
        mapping = CARD_TYPE_FPGA;
    } else if (UtoolStringEquals(funcTypeNode->valuestring, "Net Card")) {
        if (cJSON_IsNull(cardTypeNode) || cardTypeNode->valuestring == NULL) {
            mapping = CARD_TYPE_NET;
        } else if (UtoolStringEquals(cardTypeNode->valuestring, "NIC") ||
                   UtoolStringEquals(cardTypeNode->valuestring, "IB")) {
            mapping = CARD_TYPE_NIC;
        } else if (UtoolStringEquals(cardTypeNode->valuestring, "FC")) {
            mapping = CARD_TYPE_HBA;
        }
    }

    cJSON *newNode = cJSON_AddStringToObject(target, key, mapping);
    FREE_CJSON(root)
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static const UtoolOutputMapping getPCIeDeviceMappings[] = {
        {.sourceXpath = "/Id", .targetKeyValue="Id"},
        {.sourceXpath = "/Name", .targetKeyValue="CommonName"},
        {.sourceXpath = "/Oem/${Oem}/Position", .targetKeyValue="Location"},
        {.sourceXpath = "/Oem/${Oem}/FunctionType", .targetKeyValue="Type", .handle=TypeHandler, .useRootNode=true},
        {.sourceXpath = "/Oem/${Oem}/PCIeCardType", .targetKeyValue="CardType"},
        {.sourceXpath = "/Links/PCIeFunctions/0/@odata.id", .targetKeyValue="PCIeFunction"},
        {.sourceXpath = "/Status/State", .targetKeyValue="State"},
        {.sourceXpath = "/Status/Health", .targetKeyValue="Health"},
        NULL
};

static const UtoolOutputMapping getPCIeFunctionMappings[] = {
        {.sourceXpath = "/Oem/${Oem}/BusNumber", .targetKeyValue="SlotBus"},
        {.sourceXpath = "/Oem/${Oem}/DeviceNumber", .targetKeyValue="SlotDevice"},
        {.sourceXpath = "/Oem/${Oem}/FunctionNumber", .targetKeyValue="SlotFunction"},
        NULL
};


/**
 * get all pcie device information, command handler of `getpcie`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetPCIe(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // initialize output objects
    cJSON *systemRespJson = NULL,
            *output = NULL,
            *pcieDeviceArray = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};

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

    pcieDeviceArray = cJSON_CreateArray();
    result->code = UtoolAssetCreatedJsonNotNull(pcieDeviceArray);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    systemRespJson = result->data;

    cJSON *members = cJSONUtils_GetPointer(systemRespJson, "/PCIeDevices");
    result->code = UtoolAssetJsonNodeNotNull(members, "/PCIeDevices");
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGetMemberResources(server, members, pcieDeviceArray, getPCIeDeviceMappings, result);
    if (result->broken) {
        goto FAILURE;
    }

    cJSON *pcie = NULL;
    cJSON_ArrayForEach(pcie, pcieDeviceArray) {
        cJSON *pcieFuncLinkNode = cJSON_DetachItemFromObject(pcie, "PCIeFunction");
        if (cJSON_IsString(pcieFuncLinkNode)) {
            UtoolRedfishGet(server, pcieFuncLinkNode->valuestring, pcie, getPCIeFunctionMappings, result);
            if (result->broken) {
                FREE_CJSON(pcieFuncLinkNode)
                goto FAILURE;
            }
        }

        FREE_CJSON(pcieFuncLinkNode)
    }

    // add output properties
    // calculate maximum count
    cJSON *maximum = cJSON_AddNumberToObject(output, "Maximum", cJSON_GetArraySize(pcieDeviceArray));
    result->code = UtoolAssetCreatedJsonNotNull(maximum);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *overallHealth = cJSON_AddStringToObject(output, "OverallHealth",
                                                   UtoolGetOverallHealth(pcieDeviceArray, "/Health"));
    result->code = UtoolAssetCreatedJsonNotNull(overallHealth);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON_AddItemToObject(output, "PCIeDevice", pcieDeviceArray);

    // output to result
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(pcieDeviceArray)
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(systemRespJson)
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
