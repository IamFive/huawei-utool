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
bool utool_str_starts_with(const char *source, const char *target);

/**
 * check whether source string starts with target string while ignoring differences in case
 *
 * @param source
 * @param target
 * @return
 */
bool utool_str_case_starts_with(const char *source, const char *target);

#ifdef __cplusplus
}
#endif //UTOOL_STRING_UTILS_H
#endif
