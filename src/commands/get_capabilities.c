//
// Created by qianbiao on 5/8/19.
//
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
        "utool getcapabilities",
        NULL,
};

/**
 * argparse get capabilities action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetCapabilities(UtoolCommandOption *commandOption, char **result)
{
    int ret;

    struct argparse_option options[] = {
            OPT_BOOLEAN('h', "help", &(commandOption->flag), HELP_SUB_COMMAND_DESC, UtoolGetHelpOptionCallback, 0, 0),
            OPT_END(),
    };

    ret = UtoolValidateSubCommandBasicOptions(commandOption, options, usage, result);
    if (commandOption->flag != EXECUTABLE) {
        goto done;
    }

    cJSON *output, *getCommandList, *setCommandList;
    output = cJSON_CreateObject();
    ret = UtoolAssetJsonNotNull(output);
    if (ret != OK) {
        goto failure;
    }

    getCommandList = cJSON_AddArrayToObject(output, "GetCommandList");
    setCommandList = cJSON_AddArrayToObject(output, "SetCommandList");

    ret = UtoolAssetJsonNotNull(setCommandList);
    if (ret != OK) {
        goto failure;
    }

    ret = UtoolAssetJsonNotNull(output);
    if (ret != OK) {
        goto failure;
    }


    for (int idx = 0;; idx++) {
        UtoolCommand *command = commands + idx;
        if (command->name != NULL) {
            switch (command->type) {
                case GET:
                    cJSON_AddItemToArray(getCommandList, cJSON_CreateString(command->name));
                    break;
                case SET:
                    cJSON_AddItemToArray(setCommandList, cJSON_CreateString(command->name));
                    break;
                default:
                    break;
            }
        }
        else {
            break;
        }
    }

    UtoolBuildOutputResult(STATE_SUCCESS, output, result);
    ret = OK;
    goto done;


failure:
    FREE_CJSON(output)
    goto done;

done:
    return ret;
}
