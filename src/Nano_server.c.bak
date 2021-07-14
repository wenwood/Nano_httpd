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
 * readline a line from socket, and put it in buf.
 * @RETURN
 * on success, return number of character stored in buf
 * on error, -1 or -2 is returned, -1 indicate recv system call error, -2 indicate buf is too small. 
*/

ssize_t readline(int client_sock, char *buf, size_t buf_len) // need update
{
    char local_buf[BUF];
    // find where the line end
    ssize_t recv_ret = recv(client_sock, local_buf, sizeof(local_buf) - 1, MSG_PEEK);
    if (recv_ret == -1)
    {
        return -1;
    }
    local_buf[recv_ret] = '\0';
    int should_read = strstr(local_buf, "\r\n") - local_buf + 2;
    if (should_read == NULL)
    {
        return -2;
    }
    // read until the line end
    // should_read won't be bigger than buf_len, but what if it is bigger than buf_len?
    recv_ret = recv(client_sock, buf, should_read, 0);
    if (recv_ret == -1)
    {
        return -1;
    }
    buf[recv_ret] = '\0';
    return recv_ret;
}

/**
 * show the error page, tell client the server got a bad request
*/

void bad_request(int client_sock)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n\<p>Please check your request.</p>");
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
    puts(buf);
    send(client_sock, buf, strlen(buf), 0);
    puts("not_found");
}

/**
 * when receive request which is not POST or GET, tell client it has not been implemented,
 * and I think it won't be.
*/


void unimplemented(int client_sock, char *method)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<p>request method %s is not implemented yet.</p>\r\n", method);
    send(client_sock, buf, strlen(buf), 0);
}

/**
 * when client request a regular file, this function use url to retrive the file and give it to client.
*/

void return_file(int client_sock, char *url)
{
    char buf[1024];
    bzero(buf, 0);
    while (strcmp(buf, "\r\n") != 0 && (readline(client_sock, buf, sizeof(buf) - 1) > 0))
        ;
    FILE *fp = fopen(url, "r");
    if (fp == NULL)
    {
        exit_on_error("open regular file error");
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    send(client_sock, buf, strlen(buf), 0);
    size_t n = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        send(client_sock, buf, strlen(buf), 0);
    }
}

/**
 * if client request cgi, this function call cgi program
 * unfinished
*/

void exec_cgi(int client_sock, char *url)
{
    char line_buf[1024];
    size_t content_length = 0;
    do
    {
        readline(client_sock, line_buf, sizeof(line_buf));
        if (strstr(line_buf, "Content-Length:") != NULL)
        {
            content_length = atoi(strlen("Content-Length:") + line_buf);
        }

    } while (strcmp(line_buf, "\r\n") != 0);

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
        sprintf(line_buf, "Content-Length=%d", content_length);
        putenv(line_buf);

        execl(url, basename(url), (char *)NULL);
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
    puts(client_ip);

    char line_buf[BUF];
    if (readline(client_sock, line_buf, sizeof(line_buf)) == -1)
    {
        exit_on_error("CLIENT(%s) : %s", client_ip, "readline error!");
    }
    char http_request_method[10];
    char http_request_url[1024];
    char http_request_ver[10];
    if (sscanf(line_buf, "%s %s %s", http_request_method, http_request_url, http_request_ver) != 3)
    {
        bad_request(client_sock);
        pthread_exit(NULL);
    }
    modify_url(http_request_url);

    if (strcmp("GET", http_request_method) != 0 && strcmp("POST", http_request_method) != 0)
    {
        unimplemented(client_sock, http_request_method);
        pthread_exit(NULL);
    }

    struct stat request_file;
    if (stat(http_request_url, &request_file) == -1)
    {
        if (errno == ENOENT)
        {
            char buf[1024];
            bzero(buf, 0);
            ssize_t n;
            int i = 0;
            while (strcmp(buf, "\r\n") != 0 && (readline(client_sock, buf, sizeof(buf) - 1) > 0))
                ;

            not_found(client_sock, http_request_url);
            close(client_sock);
            pthread_exit(NULL);
        }
    }

    if ((request_file.st_mode & S_IXUSR) || (request_file.st_mode & S_IXGRP) || (request_file.st_mode & S_IXOTH))
    {
        exec_cgi(client_sock, http_request_url);
    }
    else
    {
        return_file(client_sock, http_request_url);
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