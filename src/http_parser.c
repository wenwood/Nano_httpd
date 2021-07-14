#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>

static Line_state get_line(char *, size_t, char **);
static Request_status parse_request_line(Http_request_t *, char *);
static void head_destroy(Http_header *);
static Request_status parse_request_header(Http_request_t *, char *);

static void head_destroy(Http_header *head)
{
    if (head == NULL)
    {
        return;
    }
    free(head->key);
    head_destroy(head->next);
    free(head);
}

Http_request_t *http_parser_init()
{
    Http_request_t *handle = (Http_request_t *)malloc(sizeof(Http_request_t));
    handle->header = NULL;
    return handle;
}

void http_parser_destroy(Http_request_t *request_struct)
{
    free(request_struct->url);
    head_destroy(request_struct->header);
    free(request_struct);
}

Request_status parse_request_line(Http_request_t *result, char *start_pos)
{
    size_t req_len = strlen(start_pos);
    char *url_space = (char *)malloc(sizeof(char) * req_len);
    char method[10];
    char version[10];
    if (sscanf(start_pos, "%s %s %s", method, url_space, version) != 3)
    {
        return BAD_REQUEST;
    }

    if (strstr(method, "GET") != NULL)
        result->method = GET;
    else if (strstr(method, "POST") != NULL)
        result->method = POST;
    else
        return BAD_REQUEST;
    if (strstr(version, "HTTP/1.1") != NULL)
        result->ver = HTTP_1_1;
    else
        return BAD_REQUEST;

    result->url = url_space;
    return GOOD_REQUEST;
}

static Request_status parse_request_header(Http_request_t *result, char *start_pos)
{
    if (strchr(start_pos, ':') == NULL)
        return BAD_REQUEST;
    size_t key_len = strchr(start_pos, ':') - start_pos;
    Http_header *header = (Http_header *)malloc(sizeof(Http_header));
    char *key_value = (char *)malloc(strlen(start_pos) + 1);
    strcpy(key_value, start_pos);
    key_value[strlen(start_pos)] = '\0';
    key_value[key_len] = '\0';     // remove colon
    key_value[key_len + 1] = '\0'; //remove space after colon;
    header->key = key_value;       // remove first double quote
    header->value = key_value + key_len + 2;
    header->next = NULL;
    if (result->header == NULL)
    {
        result->header = header;
    }
    else
    {
        Http_header *temp = result->header;
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = header;
    }
    return GOOD_REQUEST;
}

int http_parser(char *buf, size_t str_len, Http_request_t *result)
{
    Line_state linestate;
    Request_status req;
    Check_state checkstate = CHECK_STATE_REQUEST_LINE;
    char *start_pos = buf;
    // size_t end_pos = 0;
    //get_line will give new line's start position by start_pos,and set new line's /r/n to /0/0
    int deli_line = 0;
    while ((linestate = get_line(buf, str_len, &start_pos)) == LINE_OK)
    {
        if (strlen(start_pos) == 0)
        {
            start_pos += strlen(start_pos) + 2;
            if (++deli_line == 2)
            {
                break;
            }
            continue;
        }
        switch (checkstate)
        {
        case CHECK_STATE_REQUEST_LINE:
        {
            req = parse_request_line(result, start_pos);
            if (req == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            checkstate = CHECK_STATE_HEADER;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            req = parse_request_header(result, start_pos);
            if (req == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        }
        start_pos += strlen(start_pos) + 2;
    }
    if (linestate == LINE_TOOLONG)
    {
        return BAD_REQUEST;
    }
    return GOOD_REQUEST;
}

Line_state get_line(char *buf, size_t str_len, char **start_pos)
{
    char *i;

    for (i = *start_pos; i - buf < str_len; i++)
    {
        if (*i == '\r')
        {
            if (*(i + 1) == '\n')
            {
                *i = '\0';
                *(i + 1) = '\0';
                return LINE_OK;
            }
            else
                return LINE_BAD;
        }
    }
    if (i - buf == str_len - 1)
        return LINE_TOOLONG;
    return LINE_BAD;
}
