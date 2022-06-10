/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command argument parsing helper header file
* Author:
* Create: 2019-06-16
* Notes:
*/
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
 * get help action handler
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetHelpOptionCallback(struct argparse *self, const struct argparse_option *option);

/**
 * argparse get help action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolGetVersionOptionCallback(struct argparse *self, const struct argparse_option *option);

/**
 * argparse show vendor action callback
 *
 * @param self
 * @param option
 * @return
 */
int UtoolShowVendorOptionCallback(struct argparse *self, const struct argparse_option *option);

/**
 * validate IPMI connect option
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolValidateIPMIConnectOptions(UtoolCommandOption *commandOption, char **result);


/**
 * validate redfish server connect option
 *
 * @param commandOption
 * @param result
 * @return
 */
int UtoolValidateConnectOptions(UtoolCommandOption *commandOption, char **result);


/**
*  validate basic common sub command options
*
* @param commandOption
* @param options
* @param usage
* @param result
* @return
*/
int UtoolValidateSubCommandBasicOptions(UtoolCommandOption *commandOption, struct argparse_option *options,
                                        const char *const *usage, char **result);


#ifdef __cplusplus
}
#endif //UTOOL_COMMANDS_H
#endif
