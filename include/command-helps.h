//
// Created by qianbiao on 5/8/19.
//

#ifndef UTOOL_COMMAND_HELPS_H
#define UTOOL_COMMAND_HELPS_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <typedefs.h>
#include "argparse.h"
#include "constants.h"

/**
 * parse sub-command argv
 *
 * @param commandOption
 * @param options
 * @param usage
 * @return
 */
static int UtoolParseSubCommandArgv(const UtoolCommandOption *commandOption,
                                    struct argparse_option *options,
                                    const char *const *usage)
{
    struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
    argparse_describe(&argparse, TOOL_DESC, TOOL_EPI_LOG);
    int argc = argparse_parse(&argparse, commandOption->commandArgc, commandOption->commandArgv);
    return argc;
}

/**
 * get help action handler
 *
 * @param self
 * @param option
 * @return
 */
static int UtoolGetHelpOptionCallback(struct argparse *self, const struct argparse_option *option)
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
static int UtoolGetVersionOptionCallback(struct argparse *self, const struct argparse_option *option)
{
    (void) option;
    if (((int *) option->value)) {
        *((int *) option->value) = FEAT_VERSION;
    }
//    fprintf(stdout, "HUAWEI server management command-line tool version v%s", UTOOL_VERSION);
    return 0;
}


/**
 * validate redfish server connect option
 *
 * @param commandOption
 * @param result
 * @return
 */
static int UtoolValidateConnectOptions(UtoolCommandOption *commandOption, char **result)
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

    return OK;
}

static int
UtoolValidateSubCommandBasicOptions(UtoolCommandOption *commandOption, struct argparse_option *options,
                                    const char *const *usage, char **result)
{
    int left_argc = UtoolParseSubCommandArgv(commandOption, options, usage);
    if (commandOption->flag == FEAT_HELP) {
        ZF_LOGI(LOG_CMD_HELP_ACTION, commandOption->commandArgv[0]);
        return UtoolBuildOutputResult(STATE_SUCCESS, cJSON_CreateString(HELP_ACTION_OUTPUT_MESSAGE), result);
    }

    if (left_argc != 0) {
        /** too many argument is provided */
        ZF_LOGI(TOO_MANY_ARGUMENTS);
        commandOption->flag = ILLEGAL;
        return UtoolBuildOutputResult(STATE_FAILURE, cJSON_CreateString(TOO_MANY_ARGUMENTS), result);
    }

    return OK;
}


#ifdef __cplusplus
}
#endif //UTOOL_COMMANDS_H
#endif
