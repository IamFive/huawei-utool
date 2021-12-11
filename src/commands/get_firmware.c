/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getfw`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <securec.h>
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

#define LOCATION_REAR "chassisRear"

static const char *const usage[] = {
        "getfw",
        NULL,
};

static int FirmwareTypeHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    if (cJSON_IsNull(node)) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    // it seems firmware name is not solid enough to parse Type
    // we will try to parse Type from Software-Id
    char *nextp = NULL;
    char *type = UtoolStringTokens(node->valuestring, "-", &nextp);
    cJSON *newNode = cJSON_AddStringToObject(target, key, type);
    FREE_CJSON(node)
    int ret = UtoolAssetCreatedJsonNotNull(newNode);
    if (ret != UTOOLE_OK) {
        return ret;
    }

    /**
        BIOS：dcpowercycle
        CPLD：dcpowercycle
        iBMC：automatic
        PSU：automatic
        FAN：automatic
        FW：none
        Driver：none
    */
    char *activateType = NULL;
    if (UtoolStringEquals(newNode->valuestring, "BMC")) {
        activateType = "[\"automatic\"]";
    } else if (UtoolStringEquals(newNode->valuestring, "CPLD")) {
        activateType = "[\"dcpowercycle\"]";
    } else if (UtoolStringEquals(newNode->valuestring, "BIOS")) {
        activateType = "[\"dcpowercycle\"]";
    }
    cJSON *supportActivateTypes = cJSON_AddRawToObject(target, "SupportActivateType", activateType);
    ret = UtoolAssetCreatedJsonNotNull(supportActivateTypes);
    if (ret != UTOOLE_OK) {
        return ret;
    }

    return ret;
}

static int VersionHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    char *version = node->valuestring;
    if (cJSON_IsNull(node) || version == NULL) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }

    char *first = NULL, *second = NULL, *third = NULL;
    char **segments = UtoolStringSplit(version, '.');
    if (segments != NULL) {
        first = *segments == NULL ? "00" : *segments;
        second = *(segments + 1) == NULL ? "00" : *(segments + 1);
        third = *(segments + 2) == NULL ? "00" : *(segments + 2);
    } else {
        first = "00";
        second = "00";
        third = "00";
    }

    char output[32] = {0};
    UtoolWrapSecFmt(output, 32, 32 - 1, "%s.%s.%s", first, second, third);

    if (segments) {
        for (int idx = 0; *(segments + idx); idx++) {
            free(*(segments + idx));
        }
        free(segments);
    }

    cJSON *newNode = cJSON_AddStringToObject(target, key, output);
    FREE_CJSON(node)
    return UtoolAssetCreatedJsonNotNull(newNode);
}

static int NameHandler(UtoolRedfishServer *server, cJSON *target, const char *key, cJSON *node)
{
    char *name = node->valuestring;
    if (cJSON_IsNull(node) || name == NULL) {
        cJSON_AddItemToObjectCS(target, key, node);
        return UTOOLE_OK;
    }
    /**
     *  ActiveBMC -> ActiveBMC
        BackupBMC -> BackupBMC
        Bios -> BIOS
        MainBoardCPLD -> MainBoard_CPLD
        chassisPSU1 -> PSU1
        chassisPSU2 -> PSU2

        硬盘背板，
            需要在升级资源查询/redfish/v1/Chassis/1/Boards/chassisDiskBP5链接，查看Location属性，
            来明确是前置（chassisFront）还是后置（chassisRear），然后做重命名
        chassisDiskBP5CPLD -> DiskBP5_CPLD
        chassisDiskBP1CPLD -> RearDiskBP1_CPLD
     */

    char *mapping = node->valuestring;
    if (UtoolStringEquals(node->valuestring, "Bios")) {
        mapping = "BIOS";
    } else if (UtoolStringEquals(node->valuestring, "MainBoardCPLD")) {
        mapping = "MainBoard_CPLD";
    } else if (UtoolStringEquals(node->valuestring, "chassisPSU1")) {
        mapping = "PSU1";
    } else if (UtoolStringEquals(node->valuestring, "chassisPSU2")) {
        mapping = "PSU2";
    }

    cJSON *newNode = cJSON_AddStringToObject(target, key, mapping);
    FREE_CJSON(node)
    return UtoolAssetCreatedJsonNotNull(newNode);
}


static const UtoolOutputMapping getFwMappings[] = {
        {.sourceXpath = "/Name", .targetKeyValue="Name", .handle=NameHandler},
        {.sourceXpath = "/SoftwareId", .targetKeyValue="Type", .handle=FirmwareTypeHandler},
        {.sourceXpath = "/Version", .targetKeyValue="Version", .handle=VersionHandler},
        {.sourceXpath = "/Updateable", .targetKeyValue="Updateable"},
        {.sourceXpath = "/RelatedItem/0/@odata.id", .targetKeyValue="RelatedItem"},
        {0}
};

static const UtoolOutputMapping getLocationMapping[] = {
        {.sourceXpath = "/Location", .targetKeyValue="Location"},
        {.sourceXpath = "/DeviceLocator", .targetKeyValue="DeviceLocator"},
        {0}
};


/**
 * Get outband firmware information, command handler of `getfw`
 *
 * @param commandOption
 * @param outputStr
 * @return
 */
int UtoolCmdGetFirmware(UtoolCommandOption *commandOption, char **outputStr)
{
    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *memberResp = &(UtoolCurlResponse) {0};
    UtoolCurlResponse *firmwareResp = &(UtoolCurlResponse) {0};

    // initialize output objects
    cJSON *firmwareJson = NULL, *firmwareMembersJson = NULL;
    cJSON *output = NULL, *firmwares = NULL, *firmware = NULL;

    output = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(output);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    firmwares = cJSON_AddArrayToObject(output, "Firmware");
    result->code = UtoolAssetCreatedJsonNotNull(firmwares);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    result->code = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto FAILURE;
    }

    result->code = UtoolValidateConnectOptions(commandOption, &(result->desc));
    if (commandOption->flag != EXECUTABLE) {
        goto FAILURE;
    }

    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    result->code = UtoolMakeCurlRequest(server, "/UpdateService/FirmwareInventory", HTTP_GET, NULL, NULL, memberResp);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }
    if (memberResp->httpStatusCode >= 400) {
        result->code = UtoolResolveFailureResponse(memberResp, &(result->desc));
        goto FAILURE;
    }

    // process response
    firmwareMembersJson = cJSON_Parse(memberResp->content);
    result->code = UtoolAssetParseJsonNotNull(firmwareMembersJson);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *members = cJSON_GetObjectItem(firmwareMembersJson, "Members");
    int memberCount = cJSON_GetArraySize(members);
    for (int idx = 0; idx < memberCount; idx++) {
        cJSON *member = cJSON_GetArrayItem(members, idx);
        cJSON *odataIdNode = cJSON_GetObjectItem(member, "@odata.id");
        char *url = odataIdNode->valuestring;
        if (UtoolStringEndsWith(url, "BMC") || UtoolStringEndsWith(url, "Bios") || UtoolStringEndsWith(url, "CPLD")) {
            firmware = cJSON_CreateObject();
            result->code = UtoolAssetCreatedJsonNotNull(firmware);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
            cJSON_AddItemToArray(firmwares, firmware);

            UtoolRedfishGet(server, url, firmware, getFwMappings, result);
            if (result->broken) {
                goto FAILURE;
            }
            FREE_CJSON(result->data)

            /**
                硬盘背板，
                    需要在升级资源查询/redfish/v1/Chassis/1/Boards/chassisDiskBP5链接，查看Location属性，
                    来明确是前置（chassisFront）还是后置（chassisRear），然后做重命名
                chassisDiskBP5CPLD -> DiskBP5_CPLD
                chassisDiskBP1CPLD -> RearDiskBP1_CPLD
             */
            cJSON *name = cJSONUtils_GetPointer(firmware, "/Name");
            cJSON *relatedItem = cJSONUtils_GetPointer(firmware, "/RelatedItem");
            if (cJSON_IsString(name) && cJSON_IsString(relatedItem)
                && name->valuestring != NULL && relatedItem->valuestring != NULL) {
                if (UtoolStringStartsWith(name->valuestring, "chassisDiskBP")
                    && UtoolStringEndsWith(name->valuestring, "CPLD")) {
                    UtoolRedfishGet(server, relatedItem->valuestring, firmware, getLocationMapping, result);
                    if (result->broken) {
                        goto FAILURE;
                    }
                    FREE_CJSON(result->data)

                    // update Name
                    cJSON *location = cJSONUtils_GetPointer(firmware, "/Location");
                    cJSON *deviceLocator = cJSONUtils_GetPointer(firmware, "/DeviceLocator");
                    if (cJSON_IsString(location) && cJSON_IsString(deviceLocator) && location->valuestring != NULL &&
                        deviceLocator->valuestring != NULL) {
                        char mapping[256] = {0};
                        UtoolWrapSecFmt(mapping, sizeof(mapping), sizeof(mapping) - 1, "%s%s_CPLD",
                                        location->valuestring == LOCATION_REAR ? "Rear" : "",
                                        deviceLocator->valuestring);
                        cJSON_SetValuestring(name, mapping);
                    }
                }
            }
        }
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, firmwares) {
        cJSON_DeleteItemFromObject(item, "RelatedItem");
        cJSON_DeleteItemFromObject(item, "Location");
        cJSON_DeleteItemFromObject(item, "DeviceLocator");
    }


    // output to outputStr
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    FREE_CJSON(firmwareMembersJson)
    FREE_CJSON(firmwareJson)
    UtoolFreeCurlResponse(memberResp);
    UtoolFreeCurlResponse(firmwareResp);
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
