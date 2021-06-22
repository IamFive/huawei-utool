/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: command interfaces
* Author:
* Create: 2019-06-16
* Notes:
*/
#ifndef UTOOL_COMMAND_INTERFACES_H
#define UTOOL_COMMAND_INTERFACES_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

int UtoolCmdGetCapabilities(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetProduct(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetFirmware(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetBmcIP(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetProcessor(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetMemory(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetTemperature(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetVoltage(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPowerSupply(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetFan(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetRAID(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPhysicalDisks(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetLogicalDisks(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetNIC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetUsers(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetServices(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetEventSubscriptions(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPowerCapping(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetMgmtPort(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetSNMP(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetVNC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetSystemBoot(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetSensor(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetBiosSettings(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetBiosResult(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPendingBiosSettings(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetHealth(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetHealthEvent(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetEventLog(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetPCIe(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetTasks(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetTime(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdAddUser(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetPassword(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetUserPriv(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdMountVMM(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdDeleteUser(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdUpdateOutbandFirmware(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdCollectAllBoardInfo(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetIndicatorLED(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdClearSEL(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetTime(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetTimezone(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetIP(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetService(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetVLAN(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdResetBMC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetSNMPTrapNotification(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetSNMPTrapNotificationDest(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSystemPowerControl(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdRestoreBIOS(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdRestoreBMC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetVNCSettings(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetBootSourceOverride(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdDeleteVNCSession(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetFan(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetAdaptivePort(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetBIOS(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdImportBMCCfg(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdExportBMCCfg(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdLocateDisk(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSendIPMIRawCommand(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdGetIpmiWhitelist(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdSetIpmiWhitelist(UtoolCommandOption *commandOption, char **outputStr);


/** Test purpose **/
int UtoolCmdUploadFileToBMC(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdDownloadBMCFile(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdWaitRedfishTask(UtoolCommandOption *commandOption, char **outputStr);

int UtoolCmdScpFileToBMC(UtoolCommandOption *commandOption, char **outputStr);

#ifdef __cplusplus
}
#endif //UTOOL_COMMAND_INTERFACES_H
#endif
