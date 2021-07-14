#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <libgen.h>
#include <sys/wait.h>
#include <assert.h>
#include "http_parser.h"
#include "http_conn.h"
#include "threadpool.h"

#define PORT 8080
#define BUF 1024
#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define URLPREFIX "htdocs"

/**
 * use to out put the error and exit
 * @PARAMETER
 * err_msg: a printf-like string, contains %s, %d, etc, followed by other arguement
 */
void exit_on_error(const char *err_msg, ...)
{
    va_list arglist;
    va_start(arglist, err_msg);
    vfprintf(stderr, err_msg, arglist);
    va_end(arglist);
    exit(EXIT_FAILURE);
}

/**
 * same as exit_on_error, use to show log
 **/

void logging(const char *format_str, ...)
{
    va_list arglist;
    va_start(arglist, format_str);
    vprintf(format_str, arglist);
    va_end(arglist);
}

/**
 * show the error page, tell client the server got a bad request
*/

void bad_request(int client_sock)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<p>Please check your request.</p>");
    send(client_sock, buf, strlen(buf), 0);
}

/**
 * add a prefix to the request url, if url is "/", add index.html
*/

void modify_url(char *url)
{
    char local_url[1024];

    if (strcmp(url, "/") == 0)
    {
        sprintf(url, "%s/index.html", URLPREFIX);
        return;
    }
    strcpy(local_url, url);
    sprintf(url, "%s%s", URLPREFIX, local_url);
}

/**
 * show a error page, tell client the resource requested can not be found.
*/

void not_found(int client_sock, char *url)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n<html><head>404</head><body><p>the page you request : %s is not found.</p></body></html>\r\n", url);
    send(client_sock, buf, strlen(buf), 0);
    puts("not_found");
}

/**
 * when receive request which is not POST or GET, tell client it has not been implemented,
 * and I think it won't be.
*/

void unimplemented(int client_sock)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<p>request method is not implemented yet.</p>\r\n");
    send(client_sock, buf, strlen(buf), 0);
}

/**
 * when client request a regular file, this function use url to retrive the file and give it to client.
*/

void return_file(int client_sock, char *url)
{
    char buf[1024];
    bzero(buf, 0);
    FILE *fp = fopen(url, "r");
    if (fp == NULL)
    {
        exit_on_error("open regular file error");
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    send(client_sock, buf, strlen(buf), 0);
    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        send(client_sock, buf, strlen(buf), 0);
    }
}

/**
 * if client request cgi, this function call cgi program
 * unfinished
*/

void exec_cgi(int client_sock, Http_request_t *req)
{
    char line_buf[1024];
    size_t content_length = 0;
    Http_header *hp = req->header;
    while (hp != NULL)
    {
        if (strstr(hp->key, "Content-Type") != NULL)
        {
            content_length = atoi(hp->value);
            break;
        }
        hp = hp->next;
    }

    int cgi_input[2];
    int cgi_output[2];
    if (pipe(cgi_input) == -1 || pipe(cgi_output) == -1)
    {
        exit_on_error("EXEC_CGI: open pipe error!");
        pthread_exit(NULL);
    }

    if (fork() == 0)
    {
        //child
        close(cgi_input[1]);
        close(cgi_output[0]);
        dup2(cgi_input[0], STDIN_FILENO);
        dup2(cgi_output[1], STDOUT_FILENO);
        sprintf(line_buf, "Content-Length=%zu", content_length);
        putenv(line_buf);
        execl(req->url, basename(req->url), (char *)NULL);
    }

    //parent
    close(cgi_input[0]);
    close(cgi_output[1]);
    ssize_t recv_num;
    while ((recv_num = recv(client_sock, line_buf, sizeof(line_buf), 0)) > 0) // use content length to control
    {
        write(cgi_input[1], line_buf, recv_num);
    }
    ssize_t read_num;
    while ((read_num = read(cgi_output[0], line_buf, sizeof(line_buf))) > 0)
    {
        send(client_sock, line_buf, read_num, 0);
    }
}

/**
 * accept a client request in a pthread.
*/

void *accept_request(void *arg)
{
    int client_sock = *(int *)arg;

    free(arg);
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    getpeername(client_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    char client_ip[100];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    Http_request_t req;
    http_parser_init(req);
    char buf[4096];
    ssize_t recvived = recv(client_sock, buf, sizeof(buf), 0);
    buf[recvived] = '\0';
    http_parser(buf, strlen(buf), &req);
    modify_url(req.url);

    if (req.method != GET && req.method != POST)
    {
        unimplemented(client_sock);
        pthread_exit(NULL);
    }

    struct stat request_file;
    if (stat(req.url, &request_file) == -1)
    {
        if (errno == ENOENT)
        {
            bzero(buf, 0);
            while (strcmp(buf, "\r\n") != 0 && (read(client_sock, buf, sizeof(buf)) > 0))
                ;

            not_found(client_sock, req.url);
            close(client_sock);
            pthread_exit(NULL);
        }
    }

    if ((request_file.st_mode & S_IXUSR) || (request_file.st_mode & S_IXGRP) || (request_file.st_mode & S_IXOTH))
    {
        exec_cgi(client_sock, &req);
    }
    else
    {
        return_file(client_sock, req.url);
    }
    close(client_sock);
    pthread_exit(NULL);
}

int preread(http_conn *conn)
{
    //read buf full
    if (conn->read_buf_tail >= HTTP_BUF_SIZ)
        return 0;
    ssize_t read_count;
    while (1)
    {
        read_count = recv(conn->client_sock, conn->read_buf + conn->read_buf_tail, HTTP_BUF_SIZ - conn->read_buf_tail, 0);
        if (read_count == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 1;
            }
            return 0;
        }
        if (read_count == 0)
        {
            return 0;
        }
        conn->read_buf_tail += read_count;
    }
    return 1;
}

int afterwrite(http_conn *conn)
{
    ssize_t have_send = 0;
    while (1)
    {
        have_send = write(conn->client_sock, conn->write_buf + have_send, conn->write_buf_tail - have_send);
        if (have_send == -1)
        {
            if (errno == EAGAIN)
            {
                //发送缓冲区满
                struct epoll_event event;
                event.data.fd = conn->client_sock;
                event.events = EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
                epoll_ctl(http_epollfd, EPOLL_CTL_ADD, conn->client_sock, &event);
                return 1;
            }
            return 0;
        }
        if (have_send >= conn->write_buf_tail)
        {
            return 1;
        }
        return 0;
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    //let the kernal decide the server ip address
    serv_addr.sin_addr.s_addr = htonl(0);
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        exit_on_error("Nano_httpd: serve_sock create error!");
    }
    int on = 1;
    if ((setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)sizeof(on))) < 0)
    {
        exit_on_error("setsockopt failed");
    }
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        exit_on_error("Nano_httpd: bind error!\n");
    if (listen(serv_sock, 5) == -1)
        exit_on_error("Nano_httpd: listen error!\n");
    else
        logging("Nano_httpd: Listening...\n");
    http_conn *users = (http_conn *)malloc(sizeof(http_conn) * MAX_FD);
    if (users == NULL)
    {
        exit_on_error("Nano_httpd: malloc error\n");
    }
    threadpool *pool = threadpool_init(process);
    if (pool == NULL)
    {
        exit_on_error("Nano_httpd: thread pool initialization error\n");
    }

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    if (epollfd == -1)
    {
        exit_on_error("Nano_httpd: epoll_create error\n");
    }
    http_epollfd = epollfd;
    struct epoll_event event;
    event.data.fd = serv_sock;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serv_sock, &event) == -1)
    {
        exit_on_error("Nano_httpd: epoll_ctl error\n");
    }
    int old_option = fcntl(serv_sock, F_GETFL);
    fcntl(serv_sock, F_SETFL, old_option | O_NONBLOCK);

    while (1)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        int i = 0;
        for (; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == serv_sock)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(serv_sock, (struct sockaddr *)&client_address, &client_addrlength);
                if (user_count >= MAX_FD)
                {
                    printf("Internal server busy");
                    continue;
                }
                http_conn_init(&users[connfd], connfd);
            }
            else if (events[i].events & EPOLLIN)
            {
                if (preread(&users[sockfd]))
                {
                    append_request(pool, users + sockfd);
                }
                else
                {
                    http_conn_close(&users[sockfd]);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (afterwrite(&users[sockfd]))
                {
                    http_conn_close(&users[sockfd]);
                }
            }
        }
    }
}