#include <stdio.h>
#include <stdlib.h>
#include <utool.h>
#include <curl/curl.h>

int main(int argc, char **argv)
{
    char *result = NULL;
    int ret = utool_main(argc, argv, &result);
    fprintf(ret == 0 ? stdout : stderr, result);
    if (result != NULL) {
        free(result);
    }
    return ret;
}
