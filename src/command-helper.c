/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command argument parsing helper
* Author:
* Create: 2019-06-16
* Notes:
*/

#include <command-helps.h>
#include <zf_log.h>
#include <commons.h>
#include <ipmi.h>


/**
 * get help action handler
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetHelpOptionCallback(struct argparse *self, const struct argparse_option *option)
{
    (void) option;
    argparse_usage(self);
    if (((int *) option->value)) {
        *((int *) option->value) = FEAT_HELP;
    }
    return 0;
}

/**
 * argparse get help action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetVersionOptionCallback(struct argparse *self, const struct argparse_option *option)
{
    (void) option;
    if (((int *) option->value)) {
        *((int *) option->value) = FEAT_VERSION;
    }

    return 0;
}

/**
 * argparse show vendor action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolShowVendorOptionCallback(struct argparse *self, const struct argparse_option *option)
{
    (void) option;
    if (((int *) option->value)) {
        *((int *) option->value) = FEAT_SHOW_VENDOR;
    }

    return 0;
}


/**
 * validate redfish server connect option
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolValidateConnectOptions(UtoolCommandOption *commandOption, char **result)
{
    // only output if not command help action is requested.
    int ret = UtoolValidateIPMIConnectOptions(commandOption, result);
    if (ret == UTOOLE_OK && commandOption->flag != ILLEGAL) {
        /* get redfish HTTPS port from ipmitool */
        UtoolResult *utoolResult = &(UtoolResult) {0};
        int httpPort = UtoolIPMIGetHttpsPort(commandOption, utoolResult);
        commandOption->port = httpPort;
    }

    return UTOOLE_OK;
}

int UtoolValidateIPMIConnectOptions(UtoolCommandOption *commandOption, char **result)
{
    // only output if not command help action is requested.
    ZF_LOGI("Try to parse arguments now...");
    ZF_LOGI("HUAWEI server connect authentication options");
    ZF_LOGI(" ");
    ZF_LOGI("\t\thost\t\t : %s", commandOption->host);
    ZF_LOGI("\t\tport\t\t : %d", commandOption->port);
    ZF_LOGI("\t\tusername\t : %s", commandOption->username);
    ZF_LOGI("\t\tpassword\t : %s", commandOption->password ? "********" : NULL);
    ZF_LOGI(" ");

    if (!commandOption->host) {
        ZF_LOGW("Option input error : host is required.");
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString("Error: option `host` is required."), result);
    }

    if (!commandOption->username) {
        ZF_LOGW("Option input error : username is required.");
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString("Error: option `username` is required."),
                                      result);
    }

    if (!commandOption->password) {
        ZF_LOGW("Option input error : password is required.");
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString("Error: option `password` is required."),
                                      result);
    }

    return UTOOLE_OK;
}

int
UtoolValidateSubCommandBasicOptions(UtoolCommandOption *commandOption, struct argparse_option *options,
                                    const char *const *usage, char **result)
{
    struct argparse parser = {0};
    argparse_init(&parser, options, usage, 0);
    argparse_describe(&parser, TOOL_DESC, CMD_EPI_LOG);
    int left_argc = argparse_parse(&parser, commandOption->commandArgc, commandOption->commandArgv);

    if (parser.error) {
        commandOption->flag = ILLEGAL;
        int ret = UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(parser.reason), result);
        FREE_OBJ(parser.reason)
        return ret;
    }

    if (commandOption->flag == FEAT_HELP) {
        ZF_LOGI(LOG_CMD_HELP_ACTION, commandOption->commandArgv[0]);
        return UTOOLE_OK;
    }

    if (left_argc != 0) {
        /** too many argument is provided */
        ZF_LOGI(TOO_MANY_ARGUMENTS);
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(TOO_MANY_ARGUMENTS), result);
    }

    return UTOOLE_OK;
}