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

UtoolParsedUrl *UtoolParseURL(const char *);

void UtoolFreeParsedURL(UtoolParsedUrl *purl);

#ifdef __cplusplus
}
#endif

#endif 