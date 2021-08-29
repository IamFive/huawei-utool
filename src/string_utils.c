/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: String utilities
* Author:
* Create: 2019-06-16
* Notes:
*/
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <string_utils.h>
#include <assert.h>
#include <stdlib.h>
#include <securec.h>
#include <constants.h>


/**
* check whether a string is empty
*
* @param str
* @return true if empty else false
*/
bool UtoolStringIsEmpty(const char *str)
{
    if (str == NULL) {
        return true;
    }

    if (0 == strnlen(str, 1)) {
        return true;
    }

    return false;
}

/**
 * check whether source string starts with prefix string
 *
 * @param source
 * @param target
 * @return
 */
bool UtoolStringStartsWith(const char *source, const char *prefix)
{
    size_t lenSource = strlen(source),
            lenPrefix = strlen(prefix);
    return lenSource < lenPrefix ? false : strncmp(prefix, source, lenPrefix) == 0;
}


/**
 * check whether source string starts with prefix string while ignoring differences in case
 *
 * @param source
 * @param target
 * @return
 */
bool UtoolStringCaseStartsWith(const char *source, const char *prefix)
{
    size_t lenSource = strlen(source),
            lenPrefix = strlen(prefix);
    return lenSource < lenPrefix ? false : strncasecmp(prefix, source, lenPrefix) == 0;
}

/**
* check whether source string ends with suffix string
*
* @param source
* @param suffix
* @return
*/
bool UtoolStringEndsWith(const char *source, const char *suffix)
{
    if (!source || !suffix) {
        return 0;
    }
    size_t lenSource = strlen(source);
    size_t lenSuffix = strlen(suffix);
    if (lenSuffix > lenSource) {
        return 0;
    }
    return strncmp(source + lenSource - lenSuffix, suffix, lenSuffix) == 0;
}

/**
* check whether string is in the const string array
*
* @param str
* @param array
* @return
*/
bool UtoolStringInArray(const char *str, const char *array[])
{
    if (str == NULL) {
        return false;
    }

    for (int idx = 0;; idx++) {
        const char *item = array[idx];
        if (item == NULL) {
            break;
        }
        if (strncmp(item, str, strlen(item) + 1) == 0) {
            return true;
        }
    }
    return false;
}

/**
* check whether string is in the const string array while ignoring differences in case
*
* @param str
* @param array
* @return
*/
bool UtoolStringCaseInArray(const char *str, const char **array)
{
    if (str == NULL) {
        return false;
    }

    for (int idx = 0;; idx++) {
        const char *item = array[idx];
        if (item == NULL) {
            break;
        }
        if (strncasecmp(item, str, strlen(item) + 1) == 0) {
            return true;
        }
    }
    return false;
}

/**
* check whether string  equals to a string literal
*
* @param str
* @param array
* @return
*/
bool UtoolStringEquals(const char *str, const char *literal)
{
    return strncmp(str, literal, strlen(literal) + 1) == 0;
}


/**
* check whether string  equals to a string literal
*
* @param str
* @param array
* @return
*/
void UtoolStringToUpper(char *str)
{
    while (*str != '\0') {
        *str = toupper((unsigned char) *str);
        ++str;
    }
}

/**
* get the last segment of split string
*
* @param source
* @param split
* @return
*/
char *UtoolStringLastSplit(const char *source, const char split)
{
    char *pLastSlash = strrchr(source, split);
    char *pLastSplit = pLastSlash ? pLastSlash + 1 : source;
    return pLastSplit;
}

/**
* find a string in an array ignore case
*
* @param str
* @param array
* @return the element in array if found else NULL
*/
char *UtoolStringCaseFindInArray(const char *str, const char **array)
{
    if (str == NULL) {
        return NULL;
    }

    for (int idx = 0;; idx++) {
        const char *item = array[idx];
        if (item == NULL) {
            break;
        }
        if (strncasecmp(item, str, strlen(item) + 1) == 0) {
            return (char *) item;
        }
    }

    return NULL;
}

/**
* check whether a string is numeric
*
* @param str
* @return
*/
bool UtoolStringIsNumeric(const char *str)
{
    if (str == NULL) {
        return false;
    }

    while (*str != '\0') {
        if (*str < '0' || *str > '9') {
            return false;
        }
        str++;
    }
    return true;
}

/**
* split string by delim.
*
* Caller should free return array themselves
* .
*
* @param source
* @param delim
* @return string pointer array
*/
char **UtoolStringSplit(char *source, const char delim)
{
    char **result = 0;
    int count = 0;
    char *tmp = source;
    char *lastComma = 0;

    char *nextp = NULL;
    char delimStr[2];
    delimStr[0] = delim;
    delimStr[1] = '\0';

    /* Count how many elements will be extracted. */
    bool shouldSkip = true;
    while (*tmp) {
        if (delim == *tmp) {
            if (!shouldSkip) {
                shouldSkip = true;
                count++;
                lastComma = tmp;
            }
        } else {
            shouldSkip = false;
        }
        tmp++;
    }

    if (lastComma == NULL) {
        count++;
    }

    if (lastComma != NULL) { /* Add space for trailing token. */
        count += (lastComma < (source + strlen(source) - 1)) ? 1 : 0;
    }

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **) malloc(sizeof(char *) * count);
    if (result) {
        int idx = 0;
        char *token = UtoolStringTokens(source, delimStr, &nextp);
        while (token) {
            *(result + idx++) = strdup(token);
            token = UtoolStringTokens(NULL, delimStr, &nextp);
        }
        *(result + idx) = 0;
    }

    return result;
}

void UtoolStringFreeArrays(char **arrays)
{
    if (arrays != NULL) {
        for (int idx = 0; *(arrays + idx); idx++) {
            free(*(arrays + idx));
        }
        free(arrays);
    }
}

/**
 * strndup implementation.
 *
 * due to strndup is present in glibc extension. so, mingw64 does not have it.
 *
 * @param str
 * @param size
 * @return
 */
char *UtoolStringNDup(const char *str, size_t size)
{
    int n;
    char *buffer;

    if (size <= 0) {
        return NULL;
    }

    buffer = (char *) malloc(size + 1);
    if (buffer) {
        for (n = 0; ((n < size) && (str[n] != 0)); n++) {
            buffer[n] = str[n];
        }
        buffer[n] = 0;
    }

    return buffer;
}


/**
 *
 * replace original string with new characters.
 * You must free the result if result is non-NULL.
 *
 * @param orig original string
 * @param rep  to be replaced characters
 * @param with replace with characters
 * @return
 */
char *UtoolStringReplace(const char *orig, const char *rep, const char *with)
{
    char *result = NULL; // the return string
    char *ins = NULL;    // the next insert point
    char *tmp = NULL;    // varies
    size_t len_rep;  // length of rep (the string to remove)
    size_t len_with; // length of with (the string to replace rep with)
    size_t len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count


    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    size_t destLen = strlen(orig) + (len_with - len_rep) * count;
    if (destLen <= 0) {
        return NULL;
    }
    tmp = result = malloc(destLen + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    errno_t ok;
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        ok = strncpy_s(tmp, len_front + 1, orig, len_front);
        if (ok != EOK) {
            perror("Failed to call `strncpy_s`.");
            exit(EXIT_SECURITY_ERROR);
        }
        tmp = tmp + len_front;
        ok = strncpy_s(tmp, len_with + 1, with, len_with);
        if (ok != EOK) {
            perror("Failed to call `strncpy_s`.");
            exit(EXIT_SECURITY_ERROR);
        }

        tmp = tmp + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }

    size_t left_segment_len = strlen(orig);
    ok = strncpy_s(tmp, left_segment_len + 1, orig, left_segment_len);
    if (ok != EOK) {
        perror("Failed to call `strncpy_s`.");
        exit(EXIT_SECURITY_ERROR);
    }
    return result;
}

/*
 * Divide S into tokens separated by characters in DELIM.
 *
 * public domain strtok_r() by Charlie Gordon from comp.lang.c 9/14/2007
 *      http://groups.google.com/group/comp.lang.c/msg/2ab1ecbb86646684
 *     (Declaration that it's public domain):
 *      http://groups.google.com/group/comp.lang.c/msg/7c7b39328fefab9c
 */

char *UtoolStringTokens(char *str, const char *delim, char **nextp)
{
    char *ret;

    if (str == NULL) {
        str = *nextp;
    }

    str += strspn(str, delim);
    if (*str == '\0') {
        return NULL;
    }

    ret = str;
    str += strcspn(str, delim);

    if (*str) {
        *str++ = '\0';
    }

    if (nextp != NULL) {
        *nextp = str;
    }
    return ret;
}