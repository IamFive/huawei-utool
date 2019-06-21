//#include <stdio.h>
//#include <string.h>
//#include <typedefs.h>
//#include <zf_log.h>
//
//#define IPMITOOL_CMD "ipmitool -I lanplus -H %s -U %s -P %s %s"
//
//int execIPMICommand(UtoolCommandOption *option, char *ipmiSubCommand)
//{
//
//    char cmd_str[256] = {0};
//    char result_str[1024] = {0};
//    char tmp_str[1024] = {0};
//    int len = 0;
//    int port = 0;
//    FILE *fp = NULL;
//
//    char ipmiCmd[256];
//    snprintf(ipmiCmd, sizeof(ipmiCmd), IPMITOOL_CMD, option->host, option->username, option->password, ipmiSubCommand);
//
//    ZF_LOGI("execute ipmi command: %s", ipmiCmd);
//
//    snprintf(cmd_str, sizeof(cmd_str), "ipmitool -I lanplus -H %s -U %s -P %s raw 0x30 0x93 0xdb 0x07 0x00 0x38 0x06 0x00 0x03 0xff 0x00 0x00 0x1 0x00 0x02 0x00 0x03 0x00",
//             option->host, option->username, option->password);
//
//    if ((fp = popen(cmd_str, "r")) == NULL) {
//        *https_port = 443;
//        printf("%s:%d: get https port failed, popen failed. use default port 443.\n", __FUNCTION__, __LINE__);
//        //return UTOOL_MAIN_ERROR;
//    }
//
//
//    while (fgets(tmp_str, sizeof(tmp_str), fp) != NULL) {
//        strncat(result_str, sizeof(result_str), tmp_str);
//    }
//
//
//    pclose(fp);
//    //UTOOL_LOG(LOG_LEVEL_DBG, "shell cmd result_str=%s\n", result_str);
//
//    tmp_str[0] = result_str[157];
//    tmp_str[1] = result_str[158];
//    tmp_str[2] = result_str[160];
//    tmp_str[3] = result_str[161];
//
//    hexstr2uchar(tmp_str, result_str);
//
//    *https_port = port;
//    //非法端口值时，则设置为HTTPS 默认端口号。
//
//    if ((port <= 0) || (port > 65535)) {
//        *https_port = 443;
//        printf("%s:%d: get https port failed. use default port 443.\n", __FUNCTION__, __LINE__);
//    }
//
//    return UTOOL_MAIN_OK;
//}