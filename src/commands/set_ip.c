/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `setip`
* Author:
* Create: 2019-06-14
* Notes:
*/
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

#define IP_VER_4 "4"
#define IP_VER_6 "6"
#define IP_MODE_DHCP "DHCP"
#define IP_MODE_DHCPV6 "DHCPv6"
#define IP_MODE_STATIC "Static"

static const char *IP_VERSION_CHOICES[] = {IP_VER_4, IP_VER_6, NULL};
static const char *IP_MODE_CHOICES[] = {IP_MODE_STATIC, IP_MODE_DHCP, NULL};

static const char *OPT_PREFIX_LENGTH_ILLEGAL = "Error: prefix-length must be an integer between 1 and 128.";

static const char *OPT_NO_EFFECT_WHEN_DHCP = "Error: when IP mode is DHCP, "
                                             "options `address`, `gateway`, `subnet-mask/prefix-length` has no effect.";

static const char *OPT_REQUIRED_WHEN_STATIC = "Error: when IP mode is Static, options `address`, `gateway`, "
                                              "`subnet-mask/prefix-length` should not all be empty.";

static const char *const usage[] = {
        "setip -v ip-version -m ip-mode [-a address] [-g gateway] [-s subnet-mask/prefix-length]",
        NULL,
};

typedef struct _SetIPOption
{
    char *version;
    char *address;
    char *mode;
    char *gataway;
    char *subneMask;
    int prefixLength;
} UtoolSetIPOption;

static cJSON *BuildPayload(UtoolSetIPOption *option, UtoolResult *result);

static void ValidateSubcommandOptions(UtoolSetIPOption *option, UtoolResult *result);

/**
* set BMC IPv4 or IPv6, command handler for `setip`
*
* @param commandOption
* @param result
* @return
* */
int UtoolCmdSetIP(UtoolCommandOption *commandOption, char **outputStr)
{
    cJSON *payload = NULL, *getEthernetInterfacesRespJson = NULL;

    UtoolResult *result = &(UtoolResult) {0};
    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolSetIPOption *option = &(UtoolSetIPOption) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_STRING ('v', "ip-version", &(option->version),
                        "specifies enabled IP version, available choices: {4, 6}", NULL, 0, 0),
            OPT_STRING ('m', "ip-mode", &(option->mode),
                        "specifies how IP address is allocated, available choices: {Static, DHCP}", NULL, 0, 0),
            OPT_STRING ('a', "address", &(option->address),
                        "specifies IP address when IP mode is Static", NULL, 0, 0),
            OPT_STRING ('g', "gateway", &(option->gataway),
                        "specifies gateway IP address when IP mode is Static", NULL, 0, 0),
            OPT_STRING ('s', "subnet-mask", &(option->subneMask),
                        "specifies subnet mask of IPv4 address or prefix length of IPv6 address.", NULL, 0, 0),
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

    ValidateSubcommandOptions(option, result);
    if (result->broken) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    if (result->broken) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    getEthernetInterfacesRespJson = result->data;


    /** patch system ethernet */
    cJSON *linkNode = cJSONUtils_GetPointer(getEthernetInterfacesRespJson, "/Members/0/@odata.id");
    char *url = linkNode->valuestring;
    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // output to outputStr
    UtoolBuildDefaultSuccessResult(&(result->desc));
    goto DONE;

FAILURE:
    goto DONE;

DONE:
    FREE_CJSON(payload)
    FREE_CJSON(getEthernetInterfacesRespJson)
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
static void ValidateSubcommandOptions(UtoolSetIPOption *option, UtoolResult *result)
{

    if (UtoolStringIsEmpty(option->version)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED("ip-version")),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->version)) {
        if (!UtoolStringInArray(option->version, IP_VERSION_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_CHOICE("ip-version", "4, 6")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (!UtoolStringIsEmpty(option->mode)) {
        if (!UtoolStringInArray(option->mode, IP_MODE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_CHOICE("ip-mode", "Static, DHCP")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    bool isIPV4 = UtoolStringEquals(option->version, IP_VER_4);
    if (!isIPV4) { /** IPV6 */
        if (!UtoolStringIsEmpty(option->subneMask)) {
            if (!UtoolStringIsNumeric(option->subneMask)) {
                result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PREFIX_LENGTH_ILLEGAL),
                                                      &(result->desc));
                goto FAILURE;
            }

            long len = strtol(option->subneMask, NULL, 0);
            if (len < 1 || len > 128) {
                result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_PREFIX_LENGTH_ILLEGAL),
                                                      &(result->desc));
                goto FAILURE;
            }

            option->prefixLength = len;
        }
    }

    /**
    if (UtoolStringEquals(IP_MODE_DHCP, option->mode)) {
        if (!UtoolStringIsEmpty(option->address) || !UtoolStringIsEmpty(option->gataway) ||
            !UtoolStringIsEmpty(option->subneMask)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_NO_EFFECT_WHEN_DHCP),
                                                  &(result->desc));
            goto FAILURE;
        }
    }
    else {
        if (UtoolStringIsEmpty(option->address) || UtoolStringIsEmpty(option->gataway) ||
            UtoolStringIsEmpty(option->subneMask)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED_WHEN_STATIC),
                                                  &(result->desc));
            goto FAILURE;
        }
    } */

    if (UtoolStringIsEmpty(option->mode) && UtoolStringIsEmpty(option->address) &&
        UtoolStringIsEmpty(option->gataway) && UtoolStringIsEmpty(option->subneMask)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(NOTHING_TO_PATCH), &(result->desc));
        goto FAILURE;
    }

    return;

FAILURE:
    result->broken = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetIPOption *option, UtoolResult *result)
{
    cJSON *wrapped = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(wrapped);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    bool isIPV4 = UtoolStringEquals(option->version, IP_VER_4);
    cJSON *array = cJSON_AddArrayToObject(wrapped, isIPV4 ? "IPv4Addresses" : "IPv6Addresses");
    result->code = UtoolAssetCreatedJsonNotNull(array);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }
    cJSON_AddItemToArray(array, payload);


    if (isIPV4) {
        if (!UtoolStringIsEmpty(option->mode)) {
            cJSON *addressOrigin = cJSON_AddStringToObject(payload, "AddressOrigin", option->mode);
            result->code = UtoolAssetCreatedJsonNotNull(addressOrigin);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->address)) {
            cJSON *address = cJSON_AddStringToObject(payload, "Address", option->address);
            result->code = UtoolAssetCreatedJsonNotNull(address);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->subneMask)) {
            cJSON *subneMask = cJSON_AddStringToObject(payload, "SubnetMask", option->subneMask);
            result->code = UtoolAssetCreatedJsonNotNull(subneMask);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->gataway)) {
            cJSON *subneMask = cJSON_AddStringToObject(payload, "Gateway", option->gataway);
            result->code = UtoolAssetCreatedJsonNotNull(subneMask);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

    }
    else {

        if (!UtoolStringIsEmpty(option->mode)) {
            char *mode = UtoolStringEquals(option->mode, IP_MODE_STATIC) ? IP_MODE_STATIC : IP_MODE_DHCPV6;
            cJSON *addressOrigin = cJSON_AddStringToObject(payload, "AddressOrigin", mode);
            result->code = UtoolAssetCreatedJsonNotNull(addressOrigin);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->address)) {
            cJSON *address = cJSON_AddStringToObject(payload, "Address", option->address);
            result->code = UtoolAssetCreatedJsonNotNull(address);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->subneMask)) {
            cJSON *prefixLength = cJSON_AddNumberToObject(payload, "PrefixLength", option->prefixLength);
            result->code = UtoolAssetCreatedJsonNotNull(prefixLength);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }

        if (!UtoolStringIsEmpty(option->gataway)) {
            cJSON *subneMask = cJSON_AddStringToObject(wrapped, "IPv6DefaultGateway", option->gataway);
            result->code = UtoolAssetCreatedJsonNotNull(subneMask);
            if (result->code != UTOOLE_OK) {
                goto FAILURE;
            }
        }


    }

    return wrapped;

FAILURE:
    result->broken = 1;
    FREE_CJSON(wrapped)
    return NULL;
}