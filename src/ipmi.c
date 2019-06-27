/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: ipmi utilities
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <ipmi.h>
#include <stdio.h>
#include <zf_log.h>
#include <commons.h>

#define MAX_IPMI_CMD_OUTPUT_LEN 5012
#define IPMITOOL_CMD "./ipmitool -I lanplus -H %s -U %s -P %s -p %d %s 2>&1"
#define IPMITOOL_CMD_RUN_FAILED "Failure: failed to execute IPMI command"


char *UtoolIPMIExecCommand(UtoolCommandOption *option, const char *ipmiSubCommand, UtoolResult *result) {
    FILE *fp = NULL;
    char *cmdOutput = NULL;
    char buffer[512] = {0};

    char ipmiCmd[256];
    snprintf(ipmiCmd, sizeof(ipmiCmd), IPMITOOL_CMD, option->host, option->username, option->password, option->ipmiPort,
             ipmiSubCommand);


    char secureIpmiCmd[256];
    snprintf(secureIpmiCmd, sizeof(secureIpmiCmd), IPMITOOL_CMD, option->host, option->username, "******",
             option->ipmiPort,
             ipmiSubCommand);
    ZF_LOGD("execute IPMI command: %s", secureIpmiCmd);

    if ((fp = popen(ipmiCmd, "r")) == NULL) {
        ZF_LOGI("Failed to execute IPMI command, command is: %s", ipmiCmd);
        result->desc = IPMITOOL_CMD_RUN_FAILED;
        result->broken = 1;
        return NULL;
    }

    cmdOutput = (char *) malloc(MAX_IPMI_CMD_OUTPUT_LEN);
    if (cmdOutput == NULL) {
        result->broken = 1;
        result->code = UTOOLE_INTERNAL;
        return NULL;
    }

    *cmdOutput = '\0';
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(cmdOutput, buffer, sizeof(buffer));
    }

    int ret = pclose(fp);
    if (ret != 0) {
        result->broken = 1;
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, cmdOutput, &(result->desc));
        free(cmdOutput);
        return NULL;
    }

    return cmdOutput;
}


unsigned char hex2uchar(unsigned char hexChar) {
    if (hexChar >= '0' && hexChar <= '9') {
        return hexChar - '0';
    }

    if (hexChar >= 'a' && hexChar <= 'f') {
        return hexChar - 'a' + 10;
    }

    if (hexChar >= 'A' && hexChar <= 'F') {
        return hexChar - 'A' + 10;
    }

    ZF_LOGE("Convert hex char %s failed", hexChar);
    return 0x00;
}


unsigned int hexstr2uchar(unsigned char *hexstr, unsigned char *binstr) {
    unsigned int binLen = 0;
    unsigned int hexLen = strlen((char *) hexstr);
    unsigned int index = 0;
    binLen = hexLen / 2;
    hexLen = binLen * 2;

    for (index = 0; index < hexLen; index += 2) {
        binstr[index / 2] = ((hex2uchar(hexstr[index]) << 4) & 0xF0) + hex2uchar(hexstr[index + 1]);
    }

    return binLen;
}

int UtoolIPMIGetHttpsPort(UtoolCommandOption *option, UtoolResult *result) {
    int port = 0;
    char *ipmiCmdOutput = NULL;
    char hexPortString[5] = {0};
    char ucharPortStr[1024] = {0};

    ipmiCmdOutput = UtoolIPMIExecCommand(option, IPMI_GET_HTTPS_PORT_RAW_CMD, result);
    if (result->broken) {
        FREE_OBJ(result->desc)
        return port;
    }

    hexPortString[0] = ipmiCmdOutput[157];
    hexPortString[1] = ipmiCmdOutput[158];
    hexPortString[2] = ipmiCmdOutput[160];
    hexPortString[3] = ipmiCmdOutput[161];
    hexPortString[4] = '\0';

    hexstr2uchar(hexPortString, ucharPortStr);
    port = ((ucharPortStr[1] << 8) & 0x0000FF00) + (ucharPortStr[0] & 0x000000FF);

    if((port <= 0) || (port > 65535)) {
        port = 443;
        ZF_LOGI("Failed to get HTTPS port through ipmi, use default port 443");
    }

    FREE_OBJ(ipmiCmdOutput)
    return port;
}

//db 07 00 00 01 00 79 01 01 02 00 71 02 16 00 03 00 75 04 02 00 00 00 01 00 79 01 01 02 00 71 02 50 00 03 00 75 04 09 00 00 00 01 00 79 01 01 02 00 71 02 bb 01 03 00 75 04 0b 00 00 00
