#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "url_parser.h"

/*
 * Prototype declarations
 */
static __inline__ int _is_scheme_char(int);

/*
 * Check whether the character is permitted in scheme string
 */
static __inline__ int
_is_scheme_char(int c)
{
    return (!isalpha(c) && '+' != c && '-' != c && '.' != c) ? 0 : 1;
}

/*
 * See RFC 1738, 3986
 */
UtoolParsedUrl *UtoolParseURL(const char *url)
{
    struct ParsedUrl *parsedUrl;
    const char *tmpstr;
    const char *curstr;
    int len;
    int i;
    int userpassFlag;
    int bracketFlag;

    /* Allocate the parsed url storage */
    parsedUrl = malloc(sizeof(struct ParsedUrl));
    if (NULL == parsedUrl) {
        return NULL;
    }
    parsedUrl->scheme = NULL;
    parsedUrl->host = NULL;
    parsedUrl->port = NULL;
    parsedUrl->path = NULL;
    parsedUrl->query = NULL;
    parsedUrl->fragment = NULL;
    parsedUrl->username = NULL;
    parsedUrl->password = NULL;

    curstr = url;

    /*
     * <scheme>:<scheme-specific-part>
     * <scheme> := [a-z\+\-\.]+
     *             upper case = lower case for resiliency
     */
    /* Read scheme */
    tmpstr = strchr(curstr, ':');
    if (NULL == tmpstr) {
        /* Not found the character */
        UtoolFreeParsedURL(parsedUrl);
        return NULL;
    }
    /* Get the scheme length */
    len = tmpstr - curstr;
    /* Check restrictions */
    for (i = 0; i < len; i++) {
        if (!_is_scheme_char(curstr[i])) {
            /* Invalid format */
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
    }
    /* Copy the scheme to the storage */
    parsedUrl->scheme = malloc(sizeof(char) * (len + 1));
    if (NULL == parsedUrl->scheme) {
        UtoolFreeParsedURL(parsedUrl);
        return NULL;
    }
    (void) strncpy(parsedUrl->scheme, curstr, len);
    parsedUrl->scheme[len] = '\0';
    /* Make the character to lower if it is upper case. */
    for (i = 0; i < len; i++) {
        parsedUrl->scheme[i] = tolower(parsedUrl->scheme[i]);
    }
    /* Skip ':' */
    tmpstr++;
    curstr = tmpstr;

    /*
     * //<user>:<password>@<host>:<port>/<url-path>
     * Any ":", "@" and "/" must be encoded.
     */
    /* Eat "//" */
    for (i = 0; i < 2; i++) {
        if ('/' != *curstr) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        curstr++;
    }

    /* Check if the user (and password) are specified. */
    userpassFlag = 0;
    tmpstr = curstr;
    while ('\0' != *tmpstr) {
        if ('@' == *tmpstr) {
            /* Username and password are specified */
            userpassFlag = 1;
            break;
        }
        else if ('/' == *tmpstr) {
            /* End of <host>:<port> specification */
            userpassFlag = 0;
            break;
        }
        tmpstr++;
    }

    /* User and password specification */
    tmpstr = curstr;
    if (userpassFlag) {
        /* Read username */
        while ('\0' != *tmpstr && ':' != *tmpstr && '@' != *tmpstr) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        parsedUrl->username = malloc(sizeof(char) * (len + 1));
        if (NULL == parsedUrl->username) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        (void) strncpy(parsedUrl->username, curstr, len);
        parsedUrl->username[len] = '\0';
        /* Proceed current pointer */
        curstr = tmpstr;
        if (':' == *curstr) {
            /* Skip ':' */
            curstr++;
            /* Read password */
            tmpstr = curstr;
            while ('\0' != *tmpstr && '@' != *tmpstr) {
                tmpstr++;
            }
            len = tmpstr - curstr;
            parsedUrl->password = malloc(sizeof(char) * (len + 1));
            if (NULL == parsedUrl->password) {
                UtoolFreeParsedURL(parsedUrl);
                return NULL;
            }
            (void) strncpy(parsedUrl->password, curstr, len);
            parsedUrl->password[len] = '\0';
            curstr = tmpstr;
        }
        /* Skip '@' */
        if ('@' != *curstr) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        curstr++;
    }

    if ('[' == *curstr) {
        bracketFlag = 1;
    }
    else {
        bracketFlag = 0;
    }
    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ('\0' != *tmpstr) {
        if (bracketFlag && ']' == *tmpstr) {
            /* End of IPv6 address. */
            tmpstr++;
            break;
        }
        else if (!bracketFlag && (':' == *tmpstr || '/' == *tmpstr)) {
            /* Port number is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    parsedUrl->host = malloc(sizeof(char) * (len + 1));
    if (NULL == parsedUrl->host || len <= 0) {
        UtoolFreeParsedURL(parsedUrl);
        return NULL;
    }
    (void) strncpy(parsedUrl->host, curstr, len);
    parsedUrl->host[len] = '\0';
    curstr = tmpstr;

    /* Is port number specified? */
    if (':' == *curstr) {
        curstr++;
        /* Read port number */
        tmpstr = curstr;
        while ('\0' != *tmpstr && '/' != *tmpstr) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        parsedUrl->port = malloc(sizeof(char) * (len + 1));
        if (NULL == parsedUrl->port) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        (void) strncpy(parsedUrl->port, curstr, len);
        parsedUrl->port[len] = '\0';
        curstr = tmpstr;
    }

    /* End of the string */
    if ('\0' == *curstr) {
        return parsedUrl;
    }

    /* Skip '/' */
    if ('/' != *curstr) {
        UtoolFreeParsedURL(parsedUrl);
        return NULL;
    }
    curstr++;

    /* Parse path */
    tmpstr = curstr;
    while ('\0' != *tmpstr && '#' != *tmpstr && '?' != *tmpstr) {
        tmpstr++;
    }
    len = tmpstr - curstr;
    parsedUrl->path = malloc(sizeof(char) * (len + 1));
    if (NULL == parsedUrl->path) {
        UtoolFreeParsedURL(parsedUrl);
        return NULL;
    }
    (void) strncpy(parsedUrl->path, curstr, len);
    parsedUrl->path[len] = '\0';
    curstr = tmpstr;

    /* Is query specified? */
    if ('?' == *curstr) {
        /* Skip '?' */
        curstr++;
        /* Read query */
        tmpstr = curstr;
        while ('\0' != *tmpstr && '#' != *tmpstr) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        parsedUrl->query = malloc(sizeof(char) * (len + 1));
        if (NULL == parsedUrl->query) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        (void) strncpy(parsedUrl->query, curstr, len);
        parsedUrl->query[len] = '\0';
        curstr = tmpstr;
    }

    /* Is fragment specified? */
    if ('#' == *curstr) {
        /* Skip '#' */
        curstr++;
        /* Read fragment */
        tmpstr = curstr;
        while ('\0' != *tmpstr) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        parsedUrl->fragment = malloc(sizeof(char) * (len + 1));
        if (NULL == parsedUrl->fragment) {
            UtoolFreeParsedURL(parsedUrl);
            return NULL;
        }
        (void) strncpy(parsedUrl->fragment, curstr, len);
        parsedUrl->fragment[len] = '\0';
        curstr = tmpstr;
    }

    return parsedUrl;
}

/*
 * Free memory of parsed url
 */
void UtoolFreeParsedURL(UtoolParsedUrl *purl)
{
    if (NULL != purl) {
        if (NULL != purl->scheme) {
            free(purl->scheme);
        }
        if (NULL != purl->host) {
            free(purl->host);
        }
        if (NULL != purl->port) {
            free(purl->port);
        }
        if (NULL != purl->path) {
            free(purl->path);
        }
        if (NULL != purl->query) {
            free(purl->query);
        }
        if (NULL != purl->fragment) {
            free(purl->fragment);
        }
        if (NULL != purl->username) {
            free(purl->username);
        }
        if (NULL != purl->password) {
            free(purl->password);
        }
        free(purl);
    }
}