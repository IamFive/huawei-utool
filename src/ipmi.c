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
    ZF_LOGD("execute IPMI command: %s", ipmiCmd);

    //snprintf(cmd_str, sizeof(cmd_str),
    //         "ipmitool -I lanplus -H %s -U %s -P %s raw 0x30 0x93 0xdb 0x07 0x00 0x38 0x06 0x00 0x03 0xff 0x00 0x00 0x1 0x00 0x02 0x00 0x03 0x00",
    //         option->host, option->username, option->password);

    if ((fp = popen(ipmiCmd, "r")) == NULL) {
        ZF_LOGI("Failed to execute IPMI command, command is: %s", ipmiCmd);
        result->desc = IPMITOOL_CMD_RUN_FAILED;
        result->interrupt = 1;
        return NULL;
    }

    cmdOutput = (char *) malloc(MAX_IPMI_CMD_OUTPUT_LEN);
    *cmdOutput = '\0';
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(cmdOutput, buffer, sizeof(buffer));
    }

    int ret = pclose(fp);
    if (ret != 0) {
        result->interrupt = 1;
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, cmdOutput, &(result->desc));
        free(cmdOutput);
        return NULL;
    }

    return cmdOutput;
}


unsigned char hex2uchar(unsigned char hex_ch) {
    if (hex_ch >= '0' && hex_ch <= '9') {
        return hex_ch - '0';
    }

    if (hex_ch >= 'a' && hex_ch <= 'f') {
        return hex_ch - 'a' + 10;
    }

    if (hex_ch >= 'A' && hex_ch <= 'F') {
        return hex_ch - 'A' + 10;
    }

    printf("%s:%d: convert failed.\n", __FUNCTION__, __LINE__);
    return 0x00;
}


unsigned int hexstr2uchar(unsigned char *hexstr, unsigned char *binstr) {
    unsigned int bin_len = 0;
    unsigned int hex_len = strlen((char *) hexstr);
    unsigned int index = 0;
    bin_len = hex_len / 2;
    hex_len = bin_len * 2;

    for (index = 0; index < hex_len; index += 2) {
        binstr[index / 2] = ((hex2uchar(hexstr[index]) << 4) & 0xF0) + hex2uchar(hexstr[index + 1]);
    }

    return bin_len;
}

int UtoolIPMIGetHttpsPort(UtoolCommandOption *option, UtoolResult *result) {
    int port = 0;
    char *ipmiCmdOutput = UtoolIPMIExecCommand(option, IPMI_GET_HTTPS_PORT_RAW_CMD, result);
    if (result->interrupt) {
        FREE_OBJ(result->desc)
        return port;
    }

    char hexPortString[4] = {0};
    hexPortString[0] = ipmiCmdOutput[157];
    hexPortString[1] = ipmiCmdOutput[158];
    hexPortString[2] = ipmiCmdOutput[160];
    hexPortString[3] = ipmiCmdOutput[161];

    char result_str[1024] = {0};
    hexstr2uchar(hexPortString, result_str);

    char *token = strtok(ipmiCmdOutput, " ");


    //*https_port = port;
    ////非法端口值时，则设置为HTTPS 默认端口号。
    //if((port <= 0) || (port > 65535))
    //{
    //    *https_port = 443;
    //    printf("%s:%d: get https port failed. use default port 443.\n", __FUNCTION__, __LINE__);
    //}

    return port;
}

//db 07 00 00 01 00 79 01 01 02 00 71 02 16 00 03 00 75 04 02 00 00 00 01 00 79 01 01 02 00 71 02 50 00 03 00 75 04 09 00 00 00 01 00 79 01 01 02 00 71 02 bb 01 03 00 75 04 0b 00 00 00
