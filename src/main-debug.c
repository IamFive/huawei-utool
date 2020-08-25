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
#include <zconf.h>
#include <errno.h>
#include <securec.h>
#include "zf_log.h"
#include "string_utils.h"
#include "url_parser.h"

int run_utool_main() {
    char *argv2[] = {
            "utool",
            //"--version",
            "-H", "112.93.129.9",
            // "-H", "116.66.187.23",
            "-U", "************",
            "-P", "************",
            //"--version",
            //"getcapabilities", "-j",
            // "getproduct",
            //"getfw",
            //"getip",
            //"getpsu",
            //"getfan",
            //"getcpu",
            //"getmemory",
            // "getpdisk",
            // "getraid",
            //"getldisk",
            //"getnic",
            // "gettemp",
            // "getvolt",
            // "getuser",
            //"gethealth",
            //"getsysboot",
            // "getsensor",
            // "getbios", "-f", "bios.json",
            //"getbiossetting",
            //"gethealthevent",
            // "getservice",
            //"geteventsub",
            //"getpwrcap",
            //"getmgmtport",
            //"gettrap",
            //"getvnc",
            //"gettaskstate",
            //"geteventlog", "-u", "/data/nfs/sel.tar.gz",
            //"getpcie",
            //"getbiosresult",
            //"gettime",

            //"adduser", "-n", "utool", "-p", "ThisIs@SimplePwd", "--role-id", "Administrator", "-l", "None",
            //"setpwd", "-n", "utool", "-p", "ThisIs@SimplePwd2",
            //"setpriv", "-n", "utool", "-r", "Administrator",
            //"deluser", "-n", "utool",
            //"upload",
            //"download",
            // "fwupdate",
            // "-e", "Auto", "-t", "CPLD",
            // ////"-u", "/data/nfs/2288H_V5_5288_V5-iBMC-V318.hpm",
            // ////"waittask", "-i", "2",
            // ////"-u", "/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            // ////"-u", "nfs://112.93.129.100/data/nfs/CH121_V5-CPLD-V110.hpm",
            // ////"-u", "nfs://112.93.129.100/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            // "-u", "/data/cpldimage.hpm",
            //"-u", "nfs://112.93.129.100/data/nfs/cpldimage.hpm",
            //"mountvmm", "-o", "Mount", "-i", "nfs://112.93.129.100/data/nfs/mini.iso",
            //"mountvmm", "-o", "Unmount",
            //"clearsel",
            //"settimezone", "-z", "+08:00",
            //"powercontrol", "-t", "ForcePowerCycle",
            //"setservice", "-s", "KVM", "-e", "Enabled", "-p", "80", "-t", "Enabled",
            // "setservice", "-s", "SSH", "-e", "Enabled", "-p", "12222",
            //"setvlan", "-e", "Enabled", "-v", "100",
            //"resetbmc",
            //"restorebios",
            // "settrapcom", "-c", "huawei123$%^", "-e", "Enabled", "-s", "WarningAndCritical", "-v", "1",
            //"settrapdest", "-d", "4", "-e", "Disabled", "-a", "10.1.1.4", "-p", "200",
            //"setvnc", "-e", "Disabled", "-t", "100", "-p", "p",
            //"setsysboot", "-d", "HDD", "-e", "Once", "-m", "UEFI",
//            "collect", "-u", "/tmp/",
            //"locateserver", "-s", "Blink", "-f", "2",
            //"delvncsession",
            //"setip", "-v", "4", "-m", "Static", "-a", "112.93.129.9", "-g", "112.93.129.1", "-s", "255.255.255.0",
            //"setip", "-v", "6", "-m", "Static", "-a", "2000::2000", "-g", "2000::2100", "-s", "256",
            //"setadaptiveport", "-p", "Dedicated,1;",

            // "exportbmccfg", "-u", "235.xml",
            //"importbmccfg", "-u", "/data/nfs/utool_bmc_235.xml",
            //"setbios", "-a", "BootRetry", "-v", "Disabled",
            //"setbios", "-a", "BootRetry", "-v", "Enabled", "-f", "/data/nfs/bios.json",
            //"setfan", "-i", "255", "-m", "Automatic", "-s", "30",
            //"locatedisk", "-i", "HDDPlaneDisk1", "-s", "Off",
            "sendipmirawcmd", "-n", "Chassis", "-c", "0x00",
            // "scp",
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


int debug(UtoolResult *result) {

    // char source[20] = "hello,world,woo!";
    // char** context = (char **) malloc(sizeof(char *));
    // char* seg1 = strtok_s(source, ",", context);
    // char* seg2 = strtok_s(NULL, ",", context);
    // char* seg3 = strtok_s(NULL, ",", context);
    // char* seg4 = strtok_s(NULL, ",", context);

    char dest[14] = {0};
    char *source = "hello, world!";
    strncpy_s(dest, 14, source, strlen(source));
    return 0;
}


int main(int argc, const char **argv) {
    int ret = 0;
    debug(NULL);
    ret = run_utool_main();
    return ret;
}


