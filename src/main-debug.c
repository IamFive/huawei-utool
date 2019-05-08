#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <utool.h>
#include <constants.h>


int
main(int argc, const char **argv)
{

    char *result = NULL;
    char *argv2[] = {
            "utool",
//            "--version",
            "-H", "112.93.129.9",
            "-U", "qianbiao",
            "-P", "********",
            "getproduct",
            NULL
    };

    int argc2 = sizeof(argv2) / sizeof(char *);
    int ret = utool_main(argc2 - 1, (void *) argv2, &result);
    printf("ret code -> %d \n", ret);
    printf("result -> %s", result);

    free(result);
    return 0;
}
