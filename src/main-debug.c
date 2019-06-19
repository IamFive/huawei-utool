/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: main debug script
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>
#include <cJSON.h>
#include <typedefs.h>
#include <sys/time.h>
#include <time.h>
#include "zf_log.h"
#include "string_utils.h"
#include "url_parser.h"

int run_utool_main()
{
    char *argv2[] = {
            "utool",
            //"--version",
            "-H", "112.93.129.9",
            "-U", "qianbiao",
            "-P", "********",
            //"getproduct",
            //"getfw",
            //"getip",
            //"getpsu",
            //"getfan",
            //"getcpu",
            //"getmemory",
            //"getpdisk",
            //"getraid",
            //"getldisk",
            //"getnic",
            //"gettemp",
            //"getvolt",
            //"getuser",
            //"gethealth",
            //"getsysboot",
            //"getsensor",
            //"getbios",
            //"getbiossetting",
            //"gethealthevent",
            //"getservice",
            //"geteventsub",
            //"getpwrcap",
            //"getmgmtport",
            //"gettrap",
            //"getvnc",
            //"gettaskstate",
            //"geteventlog",
            //"getpcie",

            //"adduser", "-n", "utool", "-p", "chajian12#$", "--role-id", "Administrator", "-l", "None",
            //"setpwd", "-n", "utool", "-p", "chajian12#$5",
            //"setpriv", "-n", "utool", "-r", "Administrator",
            //"deluser", "-n", "utool",
            //"upload",
            //"download",
            //"fwupdate",
            //"-e", "Auto", "-t", "BMC",
            ////"-u", "/data/nfs/2288H_V5_5288_V5-iBMC-V318.hpm",
            ////"waittask", "-i", "2",
            ////"-u", "/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            ////"-u", "nfs://112.93.129.100/data/nfs/CH121_V5-CPLD-V110.hpm",
            ////"-u", "http://112.93.129.100/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            ////"-u", "/data/nfs/ubuntu-iso.zip",
            //"-u", "nfs://112.93.129.100/data/nfs/cpldimage.hpm",
            //"mountvmm", "-o", "Mount", "-i", "nfs://112.93.129.100/data/nfs/mini.iso",
            //"mountvmm", "-o", "Unmount",
            //"clearsel",
            //"settimezone", "-z", "+08:00",
            //"powercontrol", "-t", "On",
            //"setservice", "-s", "VirtualMedia", "-e", "Enabled", "-p", "10086", "-t", "Enabled",
            //"setvlan", "-e", "Enabled", "-v", "100",
            //"resetbmc",
            //"restorebios",
            //"settrapcom", "-c", "huawei123$%^", "-e", "Enabled", "-s", "WarningAndCritical",
            //"settrapdest", "-d", "4", "-e", "Disabled", "-a", "10.1.1.4", "-p", "200",
            //"setvnc", "-e", "Disabled", "-t", "100", "-p", "p",
            //"setsysboot", "-d", "HDD", "-e", "Once", "-m", "UEFI",
            //"collect", "-u", "/data/nfs/collect2.tar.gz",
            //"locateserver", "-s", "Blink", "-f", "2",
            //"delvncsession",
            //"setip", "-v", "4", "-m", "Static", "-a", "112.93.129.9", "-g", "112.93.129.1", "-s", "255.255.255.0",
            //"setip", "-v", "6", "-m", "Static", "-a", "2000::2000", "-g", "2000::2100", "-s", "16",
            "setadaptiveport", "-p", "Dedicated,1,2;",

            //"exportbmccfg", "-u", "/data/nfs/utool_bmc_9.xml",
            //"importbmccfg", "-u", "/data/nfs/utool_bmc_235.xml",
            //"setbios", "-a", "BootRetry", "-v", "Enabled", "-f", "/data/nfs/bios.json",
            //"setfan", "-i", "255", "-m", "Automatic", "-s", "30",
            NULL
    };
    char *result = NULL;

    int argc2 = sizeof(argv2) / sizeof(char *);
    int ret = utool_main(argc2 - 1, argv2, &result);
    if (result != NULL) {
        fprintf(ret == 0 ? stdout : stderr, "%s\n", result);
        free(result);
    }
    return ret;
}

int debug(UtoolResult *result)
{
    //time_t now = time(NULL);
    //struct tm *tm_now = localtime(&now);
    ////printf("now datetime: %d-%02d-%02d %02d:%02d:%02d\n",
    ////       tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min,
    ////       tm_now->tm_sec);
    //
    //char text[100];
    //strftime(text, sizeof(text)-1, "%Y%m%d%H%M%S", tm_now);
    //strftime(text, sizeof(text)-1, "%d %m %Y %H:%M", tm_now);
    //printf("Current Date: %s", text);

}


int main(int argc, const char **argv)
{
    int ret = 0;
    debug(NULL);
    ret = run_utool_main();
    return ret;
}


