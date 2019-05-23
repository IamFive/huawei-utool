//
// Created by qianbiao on 5/11/19.
//

#ifndef UTOOL_STRING_UTILS_H
#define UTOOL_STRING_UTILS_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
* check whether source string starts with target string
* @param source
* @param target
* @return
*/
bool UtoolStringStartsWith(const char *source, const char *target);

/**
 * check whether source string starts with target string while ignoring differences in case
 *
 * @param source
 * @param target
 * @return
 */
bool UtoolStringIgnoreCaseStartsWith(const char *source, const char *target);

/**
* check whether source string ends with suffix string
*
* @param source
* @param suffix
* @return
*/
bool UtoolStringEndsWith(const char *source, const char *suffix);

/**
* check whether string is in the const string array
*
* @param str
* @param array
* @return
*/
bool UtoolStringInArray(const char *str, const char *array[]);

/**
* check whether string  equals to a string literal
* @param str
* @param array
* @return
*/
bool UtoolStringEquals(const char *str, const char *literal);

#ifdef __cplusplus
}
#endif //UTOOL_STRING_UTILS_H
#endif
