//
// Created by qianbiao on 5/11/19.
//

#include <stddef.h>
#include <string.h>
#include <string_utils.h>

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