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

#define IP_MODE_DHCP "DHCP"
#define IP_MODE_STATIC "Static"

static const char *IP_VERSION_CHOICES[] = {"4", "6", "4+6", NULL};
static const char *IP_MODE_CHOICES[] = {IP_MODE_STATIC, IP_MODE_DHCP, NULL};

static const char *OPT_NO_EFFECT_WHEN_DHCP = "Error: when IP mode is DHCP, "
                                             "options `Address`, `Gateway`, `SubnetMask` has no effect.";

//static const char *OPT_IP_VERSION_REQUIRED = "Error: option `IpVersion` is required.";
//static const char *OPT_IP_VERSION_ILLEGAL = "Error: option `IpVersion` is illegal, available choices: 4, 6, 4+6.";
//static const char *OPT_IP_MODE_REQUIRED = "Error: option `IpMode` is required.";
//static const char *OPT_IP_MODE_ILLEGAL = "Error: option `IpMode` is illegal, available choices: Static, DHCP.";

static const char *const usage[] = {
        "utool setip -v IpVersion -m IpMode [-a Address] [-g Gateway] [-s SubnetMask/PrefixLength]",
        NULL,
};

typedef struct _SetIPOption
{
    char *version;
    char *address;
    char *mode;
    char *gataway;
    char *subneMask;
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
            OPT_STRING ('v', "IPVersion", &(option->version),
                        "specifies enabled IP version, available choices: {4, 6, 4+6}", NULL, 0, 0),
            OPT_STRING ('m', "IPMode", &(option->mode),
                        "specifies how IP address is allocated, available choices: {Static, DHCP}", NULL, 0, 0),
            OPT_STRING ('a', "Address", &(option->address),
                        "specifies IP address when IP mode is Static", NULL, 0, 0),
            OPT_STRING ('g', "Gateway", &(option->gataway),
                        "specifies gateway IP address when IP mode is Static", NULL, 0, 0),
            OPT_STRING ('s', "SubnetMask", &(option->subneMask),
                        "specifies subnet mask of IPv4 address", NULL, 0, 0),
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
    if (result->interrupt) {
        goto DONE;
    }

    // build payload
    payload = BuildPayload(option, result);
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    // get redfish system id
    result->code = UtoolGetRedfishServer(commandOption, server, &(result->desc));
    if (result->code != UTOOLE_OK || server->systemId == NULL) {
        goto DONE;
    }

    UtoolRedfishGet(server, "/Managers/%s/EthernetInterfaces", NULL, NULL, result);
    if (result->interrupt) {
        goto FAILURE;
    }
    getEthernetInterfacesRespJson = result->data;


    /** patch system ethernet */
    cJSON *linkNode = cJSONUtils_GetPointer(getEthernetInterfacesRespJson, "/Members/0/@odata.id");
    char *url = linkNode->valuestring;
    UtoolRedfishPatch(server, url, payload, NULL, NULL, NULL, result);
    if (result->interrupt) {
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
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED(IPVersion)),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->version)) {
        if (!UtoolStringInArray(option->version, IP_VERSION_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_CHOICE(IPVersion, "4, 6, 4+6")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringIsEmpty(option->mode)) {
        result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_REQUIRED(IPMode)),
                                              &(result->desc));
        goto FAILURE;
    }

    if (!UtoolStringIsEmpty(option->mode)) {
        if (!UtoolStringInArray(option->mode, IP_MODE_CHOICES)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE,
                                                  cJSON_CreateString(OPT_NOT_IN_CHOICE(IPVersion, "Static, DHCP")),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    if (UtoolStringEquals(IP_MODE_DHCP, option->mode)) {
        if (!UtoolStringIsEmpty(option->address) || !UtoolStringIsEmpty(option->gataway) ||
            !UtoolStringIsEmpty(option->subneMask)) {
            result->code = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(OPT_NO_EFFECT_WHEN_DHCP),
                                                  &(result->desc));
            goto FAILURE;
        }
    }

    return;

FAILURE:
    result->interrupt = 1;
    return;
}

static cJSON *BuildPayload(UtoolSetIPOption *option, UtoolResult *result)
{
    cJSON *payload = cJSON_CreateObject();
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    cJSON *vlan = cJSON_AddObjectToObject(payload, "VLAN");
    result->code = UtoolAssetCreatedJsonNotNull(payload);
    if (result->code != UTOOLE_OK) {
        goto FAILURE;
    }

    //if (!UtoolStringIsEmpty(option->version)) {
    //    cJSON *node = cJSON_AddBoolToObject(vlan, "VLANEnable", UtoolStringEquals(option->state, ENABLED));
    //    result->code = UtoolAssetCreatedJsonNotNull(node);
    //    if (result->code != UTOOLE_OK) {
    //        goto FAILURE;
    //    }
    //}
    //
    //if (option->version != DEFAULT_INT_V) {
    //    cJSON *node = cJSON_AddNumberToObject(vlan, "VLANId", option->version);
    //    result->code = UtoolAssetCreatedJsonNotNull(node);
    //    if (result->code != UTOOLE_OK) {
    //        goto FAILURE;
    //    }
    //}

    return payload;

FAILURE:
    FREE_CJSON(payload)
    return NULL;
}