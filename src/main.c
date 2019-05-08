#include <stdio.h>
#include <stdlib.h>
#include <utool.h>

int
main(int argc, const char **argv)
{
    char *result = NULL;
    utool_main(argc, (void *) argv, &result);
    printf("Result is -> \n%s\n", result);
    free(result);
    return 0;
}
