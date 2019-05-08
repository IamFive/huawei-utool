//
// Created by qianbiao on 5/9/19.
//

#ifndef UTOOL_COMMONS_H
#define UTOOL_COMMONS_H
/* For c++ compatibility */

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include "typedefs.h"

#define FREE_cJSON(json) if ((json) != NULL) { cJSON_Delete((json)); (json) = NULL; }
#define FREE_OBJ(obj) if ((obj) != NULL) { free((obj)); (obj) = NULL; }


/**
 * build new json result and assign to (char **) result
 *
 * @param state
 * @param messages
 * @param result
 * @return
 */
int utool_copy_result(const char *state, cJSON *messages, char **result);

/**
 *
 * build new json result and assign to (char **) result.
 * Be aware that messages will be free at last
 *
 * @param state
 * @param messages
 * @param result
 * @return
 */
int utool_copy_string_result(const char *state, const char *messages, char **result);

/**
 *
 * @param source
 * @param target
 * @param mapping
 * @param count
 * @return
 */
int utool_mapping_cJSON_items(cJSON *source, cJSON *target, const utool_OutputMapping *mapping, int count);


#ifdef __cplusplus
}
#endif //UTOOL_COMMONS_H
#endif
