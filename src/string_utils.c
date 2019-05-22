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
bool UtoolStringIgnoreCaseStartsWith(const char *source, const char *prefix)
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

