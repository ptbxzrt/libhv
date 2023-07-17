#include "libevent_compat.h"

void timer_cb(evutil_socket_t fd, short event, void *arg) {
    printf("Timer triggered!\n");
}

void test_timer() {
    struct event_base *base = event_base_new();

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    struct event *timer_event = event_new(base, -1, 0, timer_cb, NULL);

    event_add(timer_event, &tv);

    event_base_dispatch(base);

    event_del(timer_event);
    event_free(timer_event);
    event_base_free(base);
}

void read_callback(evutil_socket_t fd, short event, void *arg) {
    char buffer[1024];
    int n = recv(fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        send(fd, buffer, n, 0);
    }
}

void accept_callback(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;

    struct sockaddr_storage client_address;
    socklen_t client_address_len = sizeof(client_address);
    int client_socket = accept(listener, (struct sockaddr *)&client_address, &client_address_len);

    struct event *client_event = event_new(base, client_socket, EV_READ | EV_PERSIST, read_callback, NULL);
    event_add(client_event, NULL);
}

int main() {
    struct event_base *base = event_base_new();

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket, 10);

    struct event *listener_event = event_new(base, server_socket, EV_READ | EV_PERSIST, accept_callback, base);
    event_add(listener_event, NULL);

    event_base_dispatch(base);

    event_free(listener_event);
    event_base_free(base);

    return 0;
}