
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>

#include "socket.h"

static void call_server(char* client_input, int client_socket, int bytes_to_write);

int main(int argc, char* argv[]){
    // call it as ./client.c 127.0.0.1 57179  (hostname, port)
    setbuf(stdout, NULL);

    // create socket + connect (// port + hostname)
    // this adds welcome message to the buffer
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


    char client_input[MAX_BUF] = {'\0'};

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
            // full network is true to get \r\n from server
            // ab\r\erere\r\n
            // ab\r\n
            new_msg_start = find_network_newline(message, buf_len, true)) != -1)

            if ((new_msg_start != -1)){
                // write it witht he \r\n, dosent hurt
                if ((write(STDOUT_FILENO, message, new_msg_start)) == -1) {
                    perror("write");
                    exit(1);
                }
                
                // can do &message[new_msg_start] and new_msg_start is out of bounds only when new_msg_start is one pass the boundary
                memmove(message, &message[new_msg_start], buf_len - new_msg_start); 

                // clear buf
                if ((buf_len - new_msg_start) == 0) {
                    memset(message, '\0', MAX_BUF);
                }

                buf_len -= new_msg_start;
            }

            after = &message[buf_len];
            room = MAX_BUF - buf_len;
            
        }

        if (FD_ISSET(STDIN_FILENO, &tmpset)) {
            // read from terminal into server
            n_bytes = read(STDIN_FILENO, client_input, MAX_BUF - 2);
            if (n_bytes == -1) {
                perror("read");
                exit(1);
            }

            // add \r\n when sending it to server, replace the \r at the \n
            // on window, they send it with the \r\n
            int index;
            if ((index = find_network_newline(client_input, n_bytes, true)) == -1) {
                index = find_network_newline(client_input, n_bytes, false);
                client_input[index - 2] = '\r';
                client_input[index - 1] = '\n';
            }
            call_server(client_input, client_socket, index);
        }
    }
    close(client_socket);
}

static void call_server(char *client_input, int client_socket, int bytes_to_write) {
    // get rid of trailing front spcaes (if all trailing spcae, we still send it over to get server to invalidate it because this function is returning nothing)
    int i = 0;
    while(i < (bytes_to_write - 1) && client_input[i] == ' ') {
        i += 1;
    }
    client_input = &client_input[i];

    // write it over to the server
    int n_bytes;
    if ((n_bytes = write(client_socket, client_input, bytes_to_write - i)) == -1) {
        perror("write");
        exit(1);
    }
}


// find where the \n characther is
// int j;
// for (int j = 0; j < MAX_BUF; j ++){
//     if (client_input[j] == '\n') {
//         break;
//     }
// }
 // empty|empty|/msg_all:Hello\r\n
    // 
    // j = 15
    // bytes to read: 16

    // if strncmp("/msg_all:", client_input, j + 1) {
    //     // see if they wrote empty or not


    // }