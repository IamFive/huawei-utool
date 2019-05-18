#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>


int main(int argc, const char **argv)
{

    char *result = NULL;
    char *argv2[] = {
            "utool",
            //"--version",
            "-H", "112.93.129.9",
            "-U", "qianbiao",
            "-P", "******",
            "getpsu",
            //"getcapabilities",
            //"getproduct",
            //"getfw",
            // "adduser","-n", "utool", "-p", "chajian12#$", "-r", "Administrator",
            NULL
    };

    int argc2 = sizeof(argv2) / sizeof(char *);
    int ret = utool_main(argc2 - 1, (void *) argv2, &result);
    fprintf(ret == 0 ? stdout : stderr, "%s", result);
    if (result != NULL) {
        free(result);
    }
    return ret;
}
