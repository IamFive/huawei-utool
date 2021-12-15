/*
* Copyright © Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
* Description: command handler for `getoverallpowerstatus`
* Author:
* Create: 2019-06-14
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
        "getoverallpowerstatus",
        NULL,
};

/**
    OverallPowerStatus: 多系统机器整体的电源状态，
        与HOST协同上下电的各系统都为power off则本属性值为Off，都为power on则本属性值为On，
         若有读取失败情况则本属性值为null，其它情况本属性值为Mixed 。
        若机器未带SMARTNIC、JBOG、JBOD等，则本属性等同于HostPowerStatus。取值范围： Off/On/Mixed/null 。
    HostPowerStatus: Host（主板）的电源状态，取值范围： Off/On 。
    SmartNICPowerStatus: 智能网卡的主电源状态，没接智能网卡时为null，取值范围： Off/On/null 。
    JBOGPowerStatus: JBOG的主电源状态，没接JBOG时为null，取值范围： Off/On/null。
    JBODPowerStatus: JBOD的主电源状态，没接JBOD时为null，取值范围： Off/On/null 。
 */
static const UtoolOutputMapping getOverallPowerStatusMapping[] = {
        {.sourceXpath = "/PowerState", .targetKeyValue="OverallPowerStatus"},
        {.sourceXpath = "/PowerState", .targetKeyValue="HostPowerStatus"},
        {.sourceXpath = "/Null", .targetKeyValue="SmartNICPowerStatus"},
        {.sourceXpath = "/Null", .targetKeyValue="JBOGPowerStatus"},
        {.sourceXpath = "/Null", .targetKeyValue="JBODPowerStatus"},
        NULL
};


/**
* get overall power status, command handler for `getoverallpowerstatus`.
*
* @param commandOption
* @param outputStr
* @return
*/
int UtoolCmdGetOverallPowerStatus(UtoolCommandOption *commandOption, char **outputStr)
{
    int ret;
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

    UtoolGetRedfishServer2(commandOption, server, result);
    if (result->broken || result->code != UTOOLE_OK || server->systemId == NULL) {
        goto FAILURE;
    }

    output = cJSON_CreateObject();
    ret = UtoolAssetCreatedJsonNotNull(output);
    if (ret != UTOOLE_OK) {
        goto FAILURE;
    }

    UtoolRedfishGet(server, "/Systems/%s", output, getOverallPowerStatusMapping, result);
    if (result->broken) {
        goto FAILURE;
    }
    FREE_CJSON(result->data)

    // mapping result to output json
    result->code = UtoolBuildOutputResult(STATE_SUCCESS, output, &(result->desc));
    goto DONE;

FAILURE:
    FREE_CJSON(output)
    goto DONE;

DONE:
    UtoolFreeRedfishServer(server);

    *outputStr = result->desc;
    return result->code;
}
