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
#include "constants.h"

#define FREE_CJSON(json) if ((json) != NULL) { cJSON_Delete((json)); (json) = NULL; }
#define FREE_OBJ(obj) if ((obj) != NULL) { free((obj)); (obj) = NULL; }


/**
*   All command metadata define
*/
extern UtoolCommand commands[];


/**
 * build new json result and assign to (char **) result
 *
 * @param state
 * @param messages
 * @param result
 * @return
 */
int UtoolBuildOutputResult(const char *state, cJSON *messages, char **result);

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
int UtoolBuildStringOutputResult(const char *state, const char *messages, char **result);

/**
 *
 * @param source
 * @param target
 * @param mapping
 * @param count
 * @return
 */
int UtoolMappingCJSONItems(cJSON *source, cJSON *target, const UtoolOutputMapping *mapping, int count);

/**
* build default success output result
*
* @param result
*/
void UtoolBuildDefaultSuccessResult(char **result);


/**
* get string error of UtoolCode
*
* @param code
* @return
*/
const char *UtoolGetStringError(UtoolCode code);


#ifdef __cplusplus
}
#endif //UTOOL_COMMONS_H
#endif
