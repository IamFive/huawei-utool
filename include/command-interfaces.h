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

int UtoolCmdGetMemory(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetTemperature(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetVoltage(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetPowerSupply(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetFan(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetRAID(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetPhysicalDisks(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetLogicalDisks(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetNIC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetUsers(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetServices(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetEventSubscriptions(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPowerCapping(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetMgmtPort(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetSNMP(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetVNC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetSystemBoot(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetSensor(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetBiosSettings(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetPendingBiosSettings(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetHealth(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetHealthEvent(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetEventLog(UtoolCommandOption *commandOption, char **result);

int UtoolCmdGetTasks(UtoolCommandOption *commandOption, char **result);

int UtoolCmdAddUser(UtoolCommandOption *commandOption, char **result);

int UtoolCmdSetPassword(UtoolCommandOption *commandOption, char **result);

int UtoolCmdMountVMM(UtoolCommandOption *commandOption, char **result);

int UtoolCmdDeleteUser(UtoolCommandOption *commandOption, char **result);

int UtoolCmdUpdateOutbandFirmware(UtoolCommandOption *commandOption, char **result);

int UtoolCmdClearSEL(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetTime(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetTimezone(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetIP(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetService(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetVLAN(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSystemPowerControl(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdRestoreBIOS(UtoolCommandOption *commandOption, char **outputStr);


/** Test purpose **/
int UtoolCmdUploadFileToBMC(UtoolCommandOption *commandOption, char **result);

int UtoolCmdDownloadBMCFile(UtoolCommandOption *commandOption, char **result);

#ifdef __cplusplus
}
#endif //UTOOL_COMMAND_INTERFACES_H
#endif
