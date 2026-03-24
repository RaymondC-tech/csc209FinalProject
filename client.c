
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>

#include "socket.h"


int main(int argc, char* argv[]){
    // call it as ./client.c 127.0.0.1 57179
    setbuf(stdout, NULL);

    // create socket + connect (// port + hostname)
    int client_socket = connect_to_server(strtol(argv[2], NULL, 10), argv[1]);

    int maxfd, sockets_ready;
    fd_set allset;
    fd_set tmpset;
    FD_ZERO(&allset);
    FD_SET(client_socket, &allset);
    FD_SET(STDIN_FILENO, &allset);

    maxfd = (client_socket > STDIN_FILENO) ? client_socket : STDIN_FILENO;

    bool client_not_quit = true;

    while(client_not_quit) {
        tmpset = allset;

        sockets_ready = select(maxfd + 1, &tmpset, NULL, NULL, NULL);

        if (sockets_ready == 0){
            continue;
        }

        if (sockets_ready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(client_socket, &tmpset)) {
        
        }

        if (FD_ISSET(STDIN_FILENO, &tmpset)) {

        }

    }

    close(client_socket);

}