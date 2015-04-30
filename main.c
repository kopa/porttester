#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int check_connected(char *hostname, char *portno, int timeout) {
    int sockfd, ret, s;
    struct addrinfo hints;
    struct addrinfo *addr, *rp;
    struct timeval tv;
    fd_set set;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(hostname, portno, &hints, &addr);
    if (s != 0) {
        fprintf(stderr, "Could not resolve %s: %s\n", hostname, gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = addr; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sockfd < 0) {
            printf("ERROR opening socket");
        }

        FD_ZERO(&set);
        FD_SET(sockfd, &set);

        int saved_flags = fcntl(sockfd, F_SETFL, O_NONBLOCK);

        int status = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
        if (status < 0 && errno != EINPROGRESS) {
            printf("Connection failed: %s\n", strerror(errno));
            return -1;
        }

        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        ret = select(sockfd + 1, NULL, &set, NULL, &tv);

        if (ret > 0) {
            int so_error;
            socklen_t len;

            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error == 0) {
                if (addr->ai_socktype == SOCK_DGRAM) {
                    char buf[256];
                    char *senddata= "\x38\x21\xdd\xc9\x90\x3a\x82\xd7\xcd";

                    fcntl(sockfd, F_SETFL, saved_flags & ~O_NONBLOCK);

                    struct timeval tv2;
                    tv2.tv_sec = 5;  /* 5 Secs Timeout */
                    tv2.tv_usec = 0;  // Not init'ing this can cause strange errors

                    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv2,sizeof(struct timeval));

                    const ssize_t sent = send(sockfd, senddata, strlen(senddata), MSG_CONFIRM);
                    if (sent < 0) {
                        printf("UDP send failed: %s\n", strerror(errno));
                        return -1;
                    }
                    ssize_t recvd = recv(sockfd, buf, sizeof(buf), 0);
                    if (recvd < 0 && errno != EINPROGRESS) {
                        printf("UDP Connection to port %s on %s failed: %s\n", portno, hostname, strerror(errno));
                        return -1;
                    } else {
                        printf("UDP Port %s on %s is open\n", portno, hostname);
                    }
                } else {
                    printf("TCP Port %s on %s is open\n", portno, hostname);
                }
            } else {
                printf("TCP Connection to port %s on %s failed. Error: %s\n", portno, hostname, strerror(so_error));
            }
        } else if (ret == -1) {
            printf("select() error\n");
        } else {
            printf("Port %s on %s is closed (timed out %ds)\n", portno, hostname, timeout);
        }

        freeaddrinfo(addr);
        close(sockfd);
    }

    return 0;

}

int main() {

    check_connected("digicluster365.at", "1194", 5);
    check_connected("digicluster365.at", "1195", 5);
    check_connected("144.76.4.6", "1194", 5);
    check_connected("144.76.4.6", "50000", 5);

}