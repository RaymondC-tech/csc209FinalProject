#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define MAX_CLIENTS 32
#define MAX_BUF 128
#define MAX_QUEUE 20

#ifndef PORT
  #define PORT 57179
#endif

#include "socket.h"

struct client {
    int fd;
    char* name;
    char buf[MAX_BUF];
    int buf_len;
    int private_room;
    char* after;
    int room;
};

struct client all_client_array[MAX_CLIENTS];


static void initialize_client_array();
static int client_array_has_room(int fd);
static void add_to_client_array(int fd);

static struct client add_client(int listen_soc, char *name);
static struct client remove_client(struct client *top, int fd);

static int handle_client_orchestration(int fd, struct client select_client);
static void handle_client_action(int fd, struct client select_client, char *message_end);

static void broadcast_everyone(char *message);
static void broadcast_room(struct client *top, char *s, int size);


int main(int argc, char* argv[]){
    // initialize the client array to be default -1 file descriptor each
    initialize_client_array();

    // socket + bind + listen
    int listenfd = bindandlisten(PORT); // sets up a listening socket

    int client_fd, maxfd, sockets_ready;
    fd_set allset;
    fd_set tmpset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd; 

    while(1) {
        tmpset = allset;

        sockets_ready = select(maxfd + 1, &tmpset, NULL, NULL, NULL);

        if (sockets_ready == 0){
            continue;
        }

        if (sockets_ready == -1) {
            perror("select");
            continue;
        }

        // if there is a new client trying to join
        if (FD_ISSET(listenfd, &tmpset)) {
            if ((client_fd = accept_connection(listenfd)) == -1) {
                perror("accept");
                exit(1);
            }
            // see if there is enough space to add to the client_array
            int space_client_array = client_array_has_room(client_fd);
            int write_bytes;

            if (space_client_array == -1) {
                char *message = "Chat Server Reached Max Capacity. Try again Later!!\r\n";
                if ((write_bytes = write(client_fd, message, strlen(message))) == -1){
                    perror("write");
                    exit(1);
                }
                close(client_fd);
            }
            else {
                // custom socket created for communicatoin between client and server
                char *message = "WELCOME TO THE CHAT SERVER\r\n";
                if ((write_bytes = write(client_fd, message, strlen(message))) == -1){
                    perror("write");
                    exit(1);
                }
                FD_SET(client_fd, &allset);
                if (client_fd > maxfd) {
                    maxfd = client_fd;
                }
            }
        }

        // check client file descriptor to see if they are ready for server to act
        for(int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &tmpset)) {
                // read the message from the client
                struct client select_client = all_client_array[i];
                int result = handle_client_orchestration(i, select_client);

            }
        }
    }
}


static int handle_client_orchestration(int fd, struct client select_client){
    // read message and delegate task to handle_client_action
    int nbytes;
    while((nbytes = read(fd, select_client.after, select_client.buf_len)) > 0) {
        if (nbytes == 0) {
            // socket has closed
            return -1;
        }
        else if (nbytes > 0) {
            select_client.buf_len += nbytes;
            char *message_end;
            
            if ((message_end = strstr(select_client.buf,"\r\n")) != NULL){
                *message_end = '\0';
                handle_client_action(fd, select_client, message_end);
                
                // no need to subtract since pointer arithmetic gives int
                memmove(select_client.buf, message_end + 2, (select_client.buf_len - (message_end + 2 - select_client.buf))); 

                select_client.buf_len = select_client.buf_len - (message_end + 2 - select_client.buf);
            }
            select_client.after = &select_client.buf[select_client.buf_len]; 
            select_client.room = MAX_BUF - select_client.buf_len;
        }
        else {
            perror("read");
            exit(1);
        }
    }
    return 1;
}

static void handle_client_action(int fd, struct client select_client, char *message_end){
    // client can 2. private dm, 3. dm all, 4. create room, 5. join room, 6. see who is online, 7. send emojis
    // assuming when they first connect, they already joined the chat application server
    char msg[MAX_BUF];
    char *position;
    int n;

    // /msg_all:<message>
    if (strstr(select_client.buf, "/msg_all:") != NULL) {
        position = select_client.buf + 9;

        // check if the message is empty
        if (*position == '\0') {
            char *message = "Message is blank. Please write something if you wanted to message everyone";
            if ((n = write(select_client.fd, message, sizeof(message))) == -1) {
                perror("write");
                exit(1);
            }
        }
        sprintf(msg, "%s: %s", select_client.name, select_client.buf);
        broadcast_everyone(msg);
        
    }
    else {
        // check to see if the user joined first before hand
    }
}

static void broadcast_everyone(char *message){
    for (int i = 0; i < MAX_CLIENTS; i ++){
        if (all_client_array[i].fd != -1) {
            if (write(all_client_array[i].fd, message, sizeof(message)) == -1) {
                perror("write");
                exit(1);
            }
        }
    }
}

static void initialize_client_array(){
    for (int i = 0; i < MAX_CLIENTS; i ++) {
        all_client_array[i].fd = -1;
    }
}

static int client_array_has_room(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i].fd != -1){
            return i;
        }
    }
    // no client arary available so return -1
    return -1;
}

/* Assuming there is room inside array */
static void add_to_client_array(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i].fd != -1){
            all_client_array[i].fd  = fd;
            memset(all_client_array[i].buf, '\0', sizeof(all_client_array[i].buf));
            all_client_array[i].buf_len = 0;
            all_client_array[i].private_room  = -1;
            all_client_array[i].after  = all_client_array[i].buf;
        }
    }
}