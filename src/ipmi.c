/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: ipmi utilities
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <ipmi.h>
#include <stdio.h>
#include <unistd.h>
#include <zf_log.h>
#include <commons.h>
#include <securec.h>
#include <string_utils.h>

#define MAX_IPMI_CMD_OUTPUT_LEN 5012
#define IPMITOOL_CMD_RUN_FAILED "Failure: failed to execute IPMI command"
#if defined(__MINGW32__)
#define IPMITOOL_CMD "ipmitool -I lanplus -H %s -U %s -P %s -p %d %s 2>&1"
#else
#define IPMITOOL_CMD "chmod +x %s/ipmitool && %s/ipmitool -I lanplus -H %s -U %s -P %s -p %d %s 2>&1"
#endif

#define ESCAPE_CHARS "|;&$><`\\!\n"

static bool changeVendorId = false;

char *
UtoolIPMIExecRawCommand(UtoolCommandOption *option, UtoolIPMIRawCmdOption *ipmiRawCmdOption, UtoolResult *result)
{
    FILE *fp = NULL;
    char *cmdOutput = NULL;
    char buffer[512] = {0};
    char path[MAX_IPMI_CMD_LEN] = {0};

    /* build dynamic command part */

    /* [-b channel] [-t target] */
    char ipmiRawCmd[MAX_IPMI_CMD_LEN] = {0};
    if (ipmiRawCmdOption->bridge != NULL) {
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " -b ");
        UtoolWrapStringNAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, ipmiRawCmdOption->bridge, strnlen(ipmiRawCmdOption->bridge, 128));
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " ");
    }

    if (ipmiRawCmdOption->target != NULL) {
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " -t ");
        UtoolWrapStringNAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, ipmiRawCmdOption->target, strnlen(ipmiRawCmdOption->target, 128));
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " ");
    }

    /* RAW Commands: raw <netfn> <cmd> [data] */
    UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " raw ");
    if (ipmiRawCmdOption->netfun != NULL) {
        UtoolWrapStringNAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, ipmiRawCmdOption->netfun, strnlen(ipmiRawCmdOption->netfun, 32));
    }

    if (ipmiRawCmdOption->command != NULL) {
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " ");
        UtoolWrapStringNAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, ipmiRawCmdOption->command, strnlen(ipmiRawCmdOption->command, 128));
    }


    if (ipmiRawCmdOption->data != NULL) {
        UtoolWrapStringAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, " ");
        UtoolWrapStringNAppend(ipmiRawCmd, MAX_IPMI_CMD_LEN, ipmiRawCmdOption->data,
                               strnlen(ipmiRawCmdOption->data, MAX_IPMI_CMD_LEN - 1));
    }

    int res = readlink("/proc/self/exe", path, MAX_IPMI_CMD_LEN - 1);
    if (res < 0 || (res >= MAX_IPMI_CMD_LEN - 1)) {
        ZF_LOGI("Get utool path error");
        return NULL;
    }

    char *binFileName = strrchr(path, FILEPATH_SEP);
    if (binFileName != NULL) {
        *binFileName = '\0';
    }

    const char ipmiCmd[MAX_IPMI_CMD_LEN] = {0};
    UtoolWrapSecFmt(ipmiCmd, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, IPMITOOL_CMD, path, path, option->host,
                    option->username, option->password, option->ipmiPort, ipmiRawCmd);


    char secureIpmiCmd[MAX_IPMI_CMD_LEN] = {0};
    UtoolWrapSecFmt(secureIpmiCmd, MAX_IPMI_CMD_LEN, MAX_IPMI_CMD_LEN - 1, IPMITOOL_CMD, path, path, option->host,
                    option->username, "******", option->ipmiPort, ipmiRawCmd);
    ZF_LOGI("execute IPMI command: %s", secureIpmiCmd);

    size_t firstOccur = strcspn(ipmiRawCmd, ESCAPE_CHARS);
    if (strlen(ipmiRawCmd) != firstOccur) {
        result->code = UTOOLE_INSECURE_INPUT_CHARS;
        return NULL;
    }

    cmdOutput = (char *) malloc(MAX_IPMI_CMD_OUTPUT_LEN);
    if (cmdOutput == NULL) {
        result->broken = 1;
        result->code = UTOOLE_INTERNAL;
        return NULL;
    }

    if ((fp = popen(ipmiCmd, "r")) == NULL) {
        ZF_LOGI("Failed to execute IPMI command, command is: %s", ipmiCmd);
        result->desc = IPMITOOL_CMD_RUN_FAILED;
        result->broken = 1;
        free(cmdOutput);
        return NULL;
    }

    *cmdOutput = '\0';
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        UtoolWrapStringNAppend(cmdOutput, MAX_IPMI_CMD_OUTPUT_LEN, buffer, sizeof(buffer));
    }

    int ret = pclose(fp);
    if (ret != 0) {
        result->broken = 1;
        result->code = UtoolBuildStringOutputResult(STATE_FAILURE, cmdOutput, &(result->desc));
        free(cmdOutput);
        return NULL;
    }

    // remove ending \n if exists
    int len = strnlen(cmdOutput, MAX_IPMI_CMD_OUTPUT_LEN);
    if (len > 0) {
        cmdOutput[len-1] = '\0';
    }

    char *escaped = UtoolStringReplace(cmdOutput, ESCAPE_CHARS, " ");
    free(cmdOutput);
    if (!escaped) {
        result->broken = 1;
        result->code = UTOOLE_INTERNAL;
        return NULL;
    }

    return escaped;
}


unsigned char hex2uchar(unsigned char hexChar)
{
    if (hexChar >= '0' && hexChar <= '9') {
        return (unsigned char) (hexChar - '0');
    }

    if (hexChar >= 'a' && hexChar <= 'f') {
        return (unsigned char) (hexChar - 'a' + 10);
    }

    if (hexChar >= 'A' && hexChar <= 'F') {
        return (unsigned char) (hexChar - 'A' + 10);
    }

    ZF_LOGE("Convert hex char %c failed", hexChar);
    return 0x00;
}

int hexstr2uchar(unsigned char *hexstr, int hexLen, unsigned char *binstr)
{
    int binLen = 0;
    binLen = hexLen / 2;
    hexLen = binLen * 2;

    for (int idx = 0; idx < hexLen; idx += 2) {
        binstr[idx / 2] = ((hex2uchar(hexstr[idx]) << 4) & 0xF0) + hex2uchar(hexstr[idx + 1]);
    }

    return binLen;
}

int UtoolIPMIGetHttpsPort(UtoolCommandOption *option, UtoolResult *result)
{
    int port = 0;
    char *ipmiCmdOutput = NULL;
    unsigned char hexPortString[5] = {0};
    unsigned char ucharPortStr[1024] = {0};

    UtoolIPMIRawCmdOption *rawCmdOption = &(UtoolIPMIRawCmdOption) {
            .netfun = IPMI_GET_HTTPS_PORT_NETFUN,
            .command = IPMI_GET_HTTPS_PORT_CMD,
            .data = changeVendorId ? IPMI_GET_HTTPS_PORT_DATA_XFUSION : IPMI_GET_HTTPS_PORT_DATA,
    };

    ipmiCmdOutput = UtoolIPMIExecRawCommand(option, rawCmdOption, result);
    if (!result->broken && ipmiCmdOutput != NULL) {
        hexPortString[0] = (unsigned char) ipmiCmdOutput[157];
        hexPortString[1] = (unsigned char) ipmiCmdOutput[158];
        hexPortString[2] = (unsigned char) ipmiCmdOutput[160];
        hexPortString[3] = (unsigned char) ipmiCmdOutput[161];
        hexPortString[4] = '\0';

        hexstr2uchar(hexPortString, 4, ucharPortStr);
        port = ((ucharPortStr[1] << 8) & 0x0000FF00) + (ucharPortStr[0] & 0x000000FF);
    } else {
        /* ignore error */
        FREE_OBJ(result->desc)
    }

    if ((port <= 0) || (port > 65535)) {
        port = 443;
        ZF_LOGI("Failed to get HTTPS port through ipmi, use default port 443");
    }

    FREE_OBJ(ipmiCmdOutput)
    return port;
}

bool UtoolIPMIGetVendorId(UtoolCommandOption *option, UtoolResult *result)
{
    bool res = false;
    char *ipmiCmdOutput = NULL;
    UtoolIPMIRawCmdOption *rawCmdOption = &(UtoolIPMIRawCmdOption) {
        .command = IPMI_GET_VENDOR_ID,
    };
    ipmiCmdOutput = UtoolIPMIExecRawCommand(option, rawCmdOption, result);
    if (!result->broken && ipmiCmdOutput != NULL) {
        if (strstr(ipmiCmdOutput, "14 e3 00")) {
            res = true;
            changeVendorId = true;
        }
    } else {
        FREE_OBJ(ipmiCmdOutput)
        return NULL;
    }
    FREE_OBJ(ipmiCmdOutput)
    return res;
}
