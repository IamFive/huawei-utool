/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
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
#define IPMI_GET_HTTPS_PORT_NETFUN "0x30"
#define IPMI_GET_HTTPS_PORT_CMD "0x93"
#define IPMI_GET_HTTPS_PORT_DATA "0xdb 0x07 0x00 0x38 0x06 0x00 0x03 0xff 0x00 0x00 0x1 0x00 0x02 0x00 0x03 0x00"
#define IPMI_GET_HTTPS_PORT_DATA_XFUSION "0x14 0xe3 0x00 0x38 0x06 0x00 0x03 0xff 0x00 0x00 0x1 0x00 0x02 0x00 0x03 0x00"
#define IPMI_GET_VENDOR_ID "0x06 0x01"

/**
* execute a ipmi command
*
* @param option
* @param ipmiSubCommand
* @param result
* @return
*/
char *
UtoolIPMIExecRawCommand(UtoolCommandOption *option, UtoolIPMIRawCmdOption *ipmiRawCmdOption, UtoolResult *result);


/**
* get HTTPS port through ipmi
*
* @param option
* @param result
* @return
*/
int UtoolIPMIGetHttpsPort(UtoolCommandOption *option, UtoolResult *result);

/**
* get vendor id through ipmi
*
* @param option
* @param result
* @return
*/
bool UtoolIPMIGetVendorId(UtoolCommandOption *option, UtoolResult *result);

#ifdef __cplusplus
}
#endif //UTOOL_IPMI_H
#endif
