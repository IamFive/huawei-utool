/*
* Copyright © xFusion Digital Technologies Co., Ltd. 2012-2018. All rights reserved.
* Description: URL parser header file
* Author:
* Create: 2019-06-16
* Notes:
*/
#ifndef _URL_PARSER_H
#define _URL_PARSER_H

/*
 * URL storage
 */
typedef struct ParsedUrl
{
    char *scheme;               /* mandatory */
    char *host;                 /* mandatory */
    char *port;                 /* optional */
    char *path;                 /* optional */
    char *query;                /* optional */
    char *fragment;             /* optional */
    char *username;             /* optional */
    char *password;             /* optional */
} UtoolParsedUrl;

#ifdef __cplusplus
extern "C" {
#endif

/**
* parsing a string into structured URL
*
* @return
*/
UtoolParsedUrl *UtoolParseURL(const char *);

/**
* Free a structured URL
* @param purl
*/
void UtoolFreeParsedURL(UtoolParsedUrl *purl);

#ifdef __cplusplus
}
#endif

#endif 