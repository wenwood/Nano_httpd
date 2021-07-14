/**
 * this is a http parser for my Nano_httpd
*/

#ifndef _HTTP_PARSER_H_
#define _HTTP_PARSER_H_

#include <string.h>

#ifndef BUF_SIZ
#define BUF_SIZ 4096
#endif

typedef enum
{
    GET = 1,
    POST
} Request_method;

typedef enum
{
    HTTP_1_0 = 1,
    HTTP_1_1,
    HTTP_2_0
} Http_ver;

typedef struct Http_header_t
{
    char *key;
    char *value;
    struct Http_header_t *next;
} Http_header;

typedef struct
{
    long capacity;
    long used;
    char *pos;
} Http_body;

typedef enum
{
    URL_TOO_LONG = 414
} Error_type;

typedef struct
{
    Request_method method;
    char *url;
    Http_ver ver;
    Http_header *header;
} Http_request_t;

typedef enum
{
    GOOD_REQUEST = 1,
    BAD_REQUEST,
} Request_status;

typedef enum
{
    LINE_OK = 0,
    LINE_BAD,
    LINE_END,
    LINE_TOOLONG
} Line_state;

typedef enum
{
    CHECK_STATE_REQUEST_LINE = 0,
    CHECK_STATE_HEADER
} Check_state;

// init a request obj
Http_request_t *http_parser_init();

//entrance, handle the request, store result to result
int http_parser(char *data, size_t buf_len, Http_request_t *result);

// desroy the result obj
void http_parser_destroy(Http_request_t *request_struct);

#endif