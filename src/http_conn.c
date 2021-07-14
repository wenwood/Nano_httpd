#include "http_conn.h"

void http_conn_init(http_conn *conn, int client_sock)
{
    conn->client_sock = client_sock;
    int reuse = 1;
    setsockopt(conn->client_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int old_option = fcntl(conn->client_sock, F_GETFL);
    fcntl(conn->client_sock, F_SETFL, old_option | O_NONBLOCK);
    struct epoll_event event;
    event.data.fd = conn->client_sock;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl(http_epollfd, EPOLL_CTL_ADD, conn->client_sock, &event);
    user_count++;
    memset(conn->read_buf, '\0', HTTP_BUF_SIZ);
    memset(conn->write_buf, '\0', HTTP_BUF_SIZ);
    conn->read_buf_tail = 0;
    conn->write_buf_tail = 0;
}

void *process(void *arg)
{
    http_conn *conn = (http_conn *)*(void **)arg;
    Http_request_t *req = http_parser_init();
    if (http_parser(conn->read_buf, strlen(conn->read_buf), req) == 0)
    {
        struct epoll_event event;
        event.data.fd = conn->client_sock;
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
        epoll_ctl(http_epollfd, EPOLL_CTL_MOD, conn->client_sock, &event);
        return NULL;
    }
    sprintf(conn->write_buf, "HTTP/1.1 404 Not Found\r\n\r\n");
    conn->write_buf_tail = strlen(conn->write_buf);

    struct epoll_event event;
    event.data.fd = conn->client_sock;
    event.events = EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    if (epoll_ctl(http_epollfd, EPOLL_CTL_MOD, conn->client_sock, &event) == -1)
    {
        printf("error!");
        perror("epoll");
    }
    return NULL;
}

void http_conn_close(http_conn *conn)
{
    epoll_ctl(http_epollfd, EPOLL_CTL_DEL, conn->client_sock, 0);
    close(conn->client_sock);
    conn->client_sock = -1;
    user_count--;
}