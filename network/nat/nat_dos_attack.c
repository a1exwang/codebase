#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>


int main(int argc, char *argv[]) {
    int socketfds[65536];
    struct epoll_event event_arr[65536];
    const char *host_ip = "54.223.110.164";
    int start_port = 1024, end_port = 65536;
    unsigned short target_port = (unsigned short)atoi(argv[1]);

    int ep = epoll_create(65536);

    for (int i = start_port; i < end_port; ++i) {
        if((socketfds[i] = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
            printf("Error : Could not create socket \n");
            perror("socket");
            return 1;
        }
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons((unsigned short) i);
        if (bind(socketfds[i], (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
            printf("%d bind failed\n", i);
            continue;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(target_port);
        if(inet_pton(AF_INET, host_ip, &serv_addr.sin_addr) <= 0) {
            printf("inet_pton error occured\n");
            return 1;
        }

        int ret = connect(socketfds[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if(ret != 0 && errno != EINPROGRESS) {
            printf("Error : Connect Failed \n");
            perror("connect");
            return 1;
        }
        if (i % 4096 == 0) {
            printf("%d OK\n", i);
        }
        struct epoll_event event;
        event.events = (uint32_t) (EPOLLIN | EPOLLOUT | EPOLLET);
        event.data.ptr = &socketfds[i];
        epoll_ctl(ep, EPOLL_CTL_ADD, socketfds[i], &event);
    }
    int connected_count = 0;
    while (1) {
        int n = epoll_wait(ep, event_arr, 20, -1);
        for (int i = 0; i < n; ++i) {
            struct epoll_event ev = event_arr[i];
            int events = ev.events;
            if (events & EPOLLERR) {
                int port = (int)(((int *) ev.data.ptr) - socketfds);
                printf("%d connection error\n", port);
            }
            if (events & EPOLLIN) {
                int port = (int)(((int *) ev.data.ptr) - socketfds);
                connected_count++;
                printf("Connected %d, count %d\n", port, connected_count);
            }
            if (events & EPOLLOUT) {
                int port = (int)(((int *) ev.data.ptr) - socketfds);
                char buf[10];
                int len = sprintf(buf, "%d", port);
                int nwrite = write(socketfds[port], buf, len);
            }
        }

    }
    return 0;
}