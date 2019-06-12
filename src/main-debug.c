#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>
#include <cJSON.h>
#include <typedefs.h>
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
            "getmgmtport",
            //"gettrap",
            //"getvnc",
            //"gettaskstate",
            //"geteventlog",

            //"adduser", "-n", "utool", "-p", "chajian12#$", "-r", "Administrator", "-l", "None",
            //"setpwd", "-n", "utool", "-p", "chajian12#$5",
            //"setpriv", "-n", "utool", "-r", "Administrator",
            //"deluser", "-n", "utool",
            //"upload",
            //"download",
            //"fwupdate",
            //"-e", "Auto", "-t", "BMC",
            //"-u", "/data/nfs/2288H_V5_5288_V5-iBMC-V318.hpm",
            //"waittask", "-i", "2",
            //"-u", "/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            //"-u", "nfs://112.93.129.100/data/nfs/CH121_V5_CH121L_V5-iBMC-V338.hpm",
            //"-u", "/data/nfs/ubuntu-iso.zip",
            //"-u", "nfs://112.93.129.100/data/nfs/cpldimage.hpm",
            //"mountvmm", "-o", "Mount", "-i", "nfs://112.93.129.100/data/nfs/mini.iso",
            //"clearsel",
            //"settimezone", "-z", "+08:00",
            //"powercontrol", "-t", "On",
            //"setservice", "-s", "VNC", "-e", "Enabled", "-p", "10086",
            //"setvlan", "-e", "Enabled", "-v", "100",
            //"resetbmc",
            //"restorebios",
            //"settrapcom", "-c", "huawei123$%^", "-e", "Enabled", "-s", "WarningAndCritical",
            //"settrapdest", "-d", "4", "-e", "Disabled", "-a", "10.1.1.4", "-p", "200",
            //"setvnc", "-e", "Disabled", "-t", "100", "-p", "p",
            //"setsysboot", "-d", "HDD", "-e", "Once", "-m", "UEFI",
            //"collect", "-u", "/data/nfs/collect1.tar.gz",
            //"locateserver", "-s", "Blink", "-f", "2",
            //"delvncsession",
            //"setip", "-v", "4",
            //"setadaptiveport", "-p", "[{\"NIC\":\"Dedicated\",\"Port\":0},{\"NIC\":\"MEZZ\",\"Port\":1}]",
            NULL
    };
    char *result = NULL;

    int argc2 = sizeof(argv2) / sizeof(char *);
    int ret = utool_main(argc2 - 1, (void *) argv2, &result);
    if (result != NULL) {
        fprintf(ret == 0 ? stdout : stderr, "%s", result);
        free(result);
    }
    return ret;
}

int debug(UtoolResult *result)
{
    char str[30] = "0x8100";
    char *ptr;
    double ret;

    ret = strtod(str, NULL);
    printf("The number(double) is %lf\n", ret);
    printf("String part is |%s|", ptr);

}


int main(int argc, const char **argv)
{
    int ret = 0;
    //debug(NULL);
    ret = run_utool_main();
    return ret;
}


