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
#include "utils/http_parser/http_parser.h"

#define PORT 8080
#define BUF 1024
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
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n<html><head>404</head><body><p>the page you request : %s is not found.</p></body></html>\r\n",url);
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
    while(hp != NULL)
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

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = htonl(0);
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        exit_on_error("API_SERVER: serve_sock create error!");
    }
    int on = 1;
    if ((setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)sizeof(on))) < 0)
    {
        exit_on_error("setsockopt failed");
    }
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        exit_on_error("API_SERVER: bind error!\n");
    if (listen(serv_sock, 5) == -1)
        exit_on_error("API_SERVER: listen error!\n");
    else
        logging("API_SERVER: Listening...\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sock = accept(serv_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1)
            exit_on_error("API_SERVER: accept error!\n");
        else
            logging("API_SERVER: Accept connection from %s\n", inet_ntoa(client_addr.sin_addr));
        int *client_sock_ptr = (int *)malloc(sizeof(int));
        *client_sock_ptr = client_sock;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, (void *)accept_request, (void *)client_sock_ptr) == 0)
        {
            logging("API_SERVER: thread %ld create successful\n", thread_id);
        }
    }
}