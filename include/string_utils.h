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
* check whether a string is null or empty
*
*  - NULL -> true
*  - "" -> true
*  - " " -> false
*
* @param str
* @return
*/
bool UtoolStringIsEmpty(char *str);

/**
* check whether source string starts with target string
* @param source
* @param prefix
* @return
*/
bool UtoolStringStartsWith(const char *source, const char *prefix);

/**
 * check whether source string starts with target string while ignoring differences in case
 *
 * @param source
 * @param prefix
 * @return
 */
bool UtoolStringCaseStartsWith(const char *source, const char *suffix);

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
* check whether string is in the const string array while ignoring differences in case
*
* @param str
* @param array
* @return
*/
bool UtoolStringCaseInArray(const char *str, const char **array);

/**
* check whether string  equals to a string literal
* @param str
* @param array
* @return
*/
bool UtoolStringEquals(const char *str, const char *literal);

/**
* check whether string  equals to a string literal
*
* @param str
* @param array
* @return
*/
void UtoolStringToUpper(char *str);

/**
* get the last segment of split string
*
* @param source
* @param split
* @return
*/
char *UtoolStringLastSplit(char *source, char split);

#ifdef __cplusplus
}
#endif //UTOOL_STRING_UTILS_H
#endif
