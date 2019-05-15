//
// Created by qianbiao on 5/15/19.
//

#ifndef UTOOL_COMMAND_INTERFACES_H
#define UTOOL_COMMAND_INTERFACES_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

int UtoolGetProduct(UtoolCommandOption *commandOption, char **result);

int UtoolGetFirmware(UtoolCommandOption *commandOption, char **result);

#ifdef __cplusplus
}
#endif //UTOOL_COMMAND_INTERFACES_H
#endif
