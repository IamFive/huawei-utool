/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: zf log header
* Author:
* Create: 2019-06-16
* Notes:
*/
#ifndef UTOOL_LOG_H
#define UTOOL_LOG_H
/* For c++ compatibility */
#ifdef __cplusplus
extern "C" {
#endif


/**
* setting log to file path
*
* @param log_file_path
* @return 0 if success else 1
*/
int UtoolSetLogFilePath(const char *log_file_path);

#ifdef __cplusplus
}
#endif //UTOOL_LOG_H
#endif
