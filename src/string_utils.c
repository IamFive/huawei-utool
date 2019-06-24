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
char *UtoolStringLastSplit(char *source, char split)
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
    size_t count = 0;
    char *tmp = source;
    char *lastComma = 0;

    char delimStr[2];
    delimStr[0] = delim;
    delimStr[1] = '\0';

    /* Count how many elements will be extracted. */
    while (*tmp) {
        if (delim == *tmp) {
            count++;
            lastComma = tmp;
        }
        tmp++;
    }

    if (lastComma != NULL) { /* Add space for trailing token. */
        count += (lastComma < (source + strlen(source) - 1)) ? 1 : 0;
    }

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **) malloc(sizeof(char *) * count);
    if (result) {
        size_t idx = 0;
        char *token = strtok(source, delimStr);
        while (token) {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delimStr);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}
