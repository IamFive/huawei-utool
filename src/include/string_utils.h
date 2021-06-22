/*
* Copyright © Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: string utilities header
* Author:
* Create: 2019-06-16
* Notes:
*/
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
bool UtoolStringIsEmpty(const char *str);

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
* @return the last split string segment
*/
char *UtoolStringLastSplit(char *source, char split);

/**
* find a string in an array ignore case
*
* @param str
* @param array
* @return the element in array if found else NULL
*/
char *UtoolStringCaseFindInArray(const char *str, const char **array);


/**
* check whether a string is numeric
*
* @param str
* @return
*/
bool UtoolStringIsNumeric(const char *str);

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
char **UtoolStringSplit(char *source, const char delim);

/**
 * strndup implementation.
 *
 * due to strndup is present in glibc extension. so, mingw64 does not have it.
 *
 * @param str
 * @param size
 * @return
 */
char *UtoolStringNDup(char *str, size_t size);


/**
 * replace original string with new characters.
 * You must free the result if result is non-NULL.
 *
 * @param orig
 * @param rep
 * @param with
 * @return
 */
char *UtoolStringReplace(const char *orig, char *rep, char *with);

#ifdef __cplusplus
}
#endif //UTOOL_STRING_UTILS_H
#endif
