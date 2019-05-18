//
// Created by qianbiao on 5/15/19.
//

#ifndef UTOOL_COMMAND_INTERFACES_H
#define UTOOL_COMMAND_INTERFACES_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

int UtoolCmdGetCapabilities(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetProduct(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetFirmware(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetBmcIP(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetProcessor(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetTemperature(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetVoltage(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetPowerSupply(UtoolCommandOption *commandOption, char **result);

int UtoolCmdAddUser(UtoolCommandOption *commandOption, char **result);

#ifdef __cplusplus
}
#endif //UTOOL_COMMAND_INTERFACES_H
#endif
