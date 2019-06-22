/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: ipmi header file
* Author:
* Create: 2019-06-16
* Notes:
*/

#ifndef UTOOL_IPMI_H
#define UTOOL_IPMI_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <typedefs.h>

#define MAX_IPMI_CMD_LEN 2048
#define IPMI_GET_HTTPS_PORT_RAW_CMD \
    "raw 0x30 0x93 0xdb 0x07 0x00 0x38 0x06 0x00 0x03 0xff 0x00 0x00 0x1 0x00 0x02 0x00 0x03 0x00"

/**
* execute a ipmi command
*
* @param option
* @param ipmiSubCommand
* @param result
* @return
*/
char *UtoolIPMIExecCommand(UtoolCommandOption *option, const char *ipmiSubCommand, UtoolResult *result);


/**
* get HTTPS port through ipmi
*
* @param option
* @param result
* @return
*/
int UtoolIPMIGetHttpsPort(UtoolCommandOption *option, UtoolResult *result);

#ifdef __cplusplus
}
#endif //UTOOL_IPMI_H
#endif
