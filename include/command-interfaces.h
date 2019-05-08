//
// Created by qianbiao on 5/15/19.
//

#ifndef UTOOL_COMMAND_INTERFACES_H
#define UTOOL_COMMAND_INTERFACES_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

int utool_get_product(utool_CommandOption *commandOption, char **result);

int utool_get_fw(utool_CommandOption *commandOption, char **result);

#ifdef __cplusplus
}
#endif //UTOOL_COMMAND_INTERFACES_H
#endif
