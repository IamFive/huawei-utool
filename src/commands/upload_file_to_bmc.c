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


static const char *const usage[] = {
        "utool uploadfile",
        NULL,
};


int UtoolCmdUploadFileToBMC(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    UtoolRedfishServer *server = &(UtoolRedfishServer) {0};
    UtoolCurlResponse *response = &(UtoolCurlResponse) {0};

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    // validation
    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    ret = UtoolValidateConnectOptions(commandOption, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    // get redfish system id
    ret = UtoolGetRedfishServer(commandOption, server, result);
    if (ret != UTOOLE_OK || server->systemId == NULL) {
        goto failure;
    }

    char *filepath = "/data/nfs/2288H_V5_5288_V5-iBMC-V318-3.hpm";
    ret = UtoolUploadFileToBMC(server, filepath, response);
    if (ret != UTOOLE_OK) {
        goto failure;
    }

    if (response->httpStatusCode >= 400) {
        ret = UtoolResolveFailureResponse(response, result);
        goto done;
    }

    UtoolBuildDefaultSuccessResult(result);
    goto done;

failure:
    goto done;

done:
    UtoolFreeRedfishServer(server);
    UtoolFreeCurlResponse(response);
    return ret;
}
