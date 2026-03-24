
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

    // information on recieving from server
    char message[MAX_BUF] = {'\0'};
    int buf_len = 0;
    char *after = message;
    int n_bytes = 0;
    int room = MAX_BUF;
    int new_msg_start;

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

            n_bytes = read(client_socket, after, room);

            if (n_bytes == -1) {
                perror("read");
                exit(1);
            }
            
            if (n_bytes == 0) {
                // server shutted down
                // diconnect the client
            }

            buf_len += n_bytes;;
            
            while((new_msg_start = find_network_newline(message, MAX_BUF)) != -1){

                if ((new_msg_start != -1)){
                    // write it witht he \r\n, dosent hurt
                    if ((write(STDOUT_FILENO, message, new_msg_start)) == -1) {
                        perror("write");
                        exit(1);
                    }
    
                    memmove(message, &message[new_msg_start], buf_len - new_msg_start); 

                    buf_len -= new_msg_start;
                }
                after = &message[buf_len];
                room = MAX_BUF - buf_len;
            }   
        
        }

        if (FD_ISSET(STDIN_FILENO, &tmpset)) {
            // add \r\n when sending it to server, replace the \r at the \n

            // need to check for windows vs mac: windows end in \r\n and mac is just \n
            char message[MAX_BUF];

        }

    }

    close(client_socket);

}