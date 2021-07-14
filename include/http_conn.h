#ifndef _HTTP_CONN_H_
#define _HTTP_CONN_H_

#include "http_parser.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#define HTTP_BUF_SIZ 4096
#define POLL_SIZ 5
typedef struct http_conn_t
{
    int client_sock;
    char read_buf[HTTP_BUF_SIZ];
    size_t read_buf_tail;
    char write_buf[HTTP_BUF_SIZ];
    size_t write_buf_tail;
    Http_request_t request;
} http_conn;

int http_epollfd;
int user_count;
void *process(void *arg);
void http_conn_init(http_conn *conn, int client_sock);
void http_conn_close(http_conn *conn);

#endif