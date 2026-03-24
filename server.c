#define MAX_CLIENTS 32;
#define MAX_BUF 128
#define MAX_QUEUE 20

#ifndef PORT
  #define PORT 57179
#endif

#include "socket.h"

struct client {
    int fd;
    char* name;
    char bufer[MAX_BUF];
    int buf_len;
    int private_room;

}
struct client *all_client_array = malloc(sizeof(struct client) * MAX_CLIENTS);

static struct client add_client(int listen_soc, char *name);
static struct client remove_client(struct client *top, int fd);

static void broadcast_everyone(struct client *top, char *s, int size);
static void broadcast_room(struct client *top, char *s, int size);

int main(int argc, char* argv[]){
    // initialize the client array to be default -1 file descriptor each
    initialize_client_array();

    // socket + bind + listen
    int listenfd = bindandlisten(); // sets up a listening socket

    fd_set allset;
    fd_set tmpset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd; 

    while(1) {
        tmpset = allset;

        sockets_ready = select(maxfd + 1, $rset, NULL, NULL, NULL)

        if (sockets_ready == 0){
            continue;
        }

        if (sockets_ready == -1) {
            perror("select");
            continue;
        }

        // if there is a new client trying to join
        if (FD_ISSET(listenfd, &rset)) {
            socklen_t len;
            struct sockaddr_in q;
            len = sizeof(q);

            if ((client_fd = accept_connection(listenfd), (struct sockaddr *)&q, &len) == -1) {
                perror("accept");
                exit(1);
            }
        }

        FD_SET(client_fd, &allset);
        if (clientfd > maxfd) {
            maxfd = clientfd;
        }




    }

    
}

/* bind and listen, abort on error
  * returns FD of listening socket
  */
  int bindandlisten(void) {
    // Initialize a struct containing the address of this server.
    struct sockaddr_in *self = server_address_struc(PORT);

    // Print out information about this server
    // char host[MAX_HOSTNAME];
    // if ((gethostname(host, sizeof(host))) == -1) {
    //     perror("gethostname");
    //     exit(1);
    // }

    //fprintf(stderr, "Server hostname: %s\n", host);
    //fprintf(stderr, "Port: %d\n", PORT);

    // Create an fd to listen to new connections.
    return set_up_server_socket(self, 1);
}


/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
 int accept_connection(int listenfd) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int client_socket = accept(listenfd, (struct sockaddr *)&peer, &peer_len);
    if (client_socket < 0) {
        perror("accept");
        return -1;
    } 

    // see if there is enough space to add to the client_array
    int space_client_array = client_array_has_room;

    if (space_client_array == -1) {
        int write_bytes;
        char *message = "Chat Server Reached Max Capacity. Try again Later!!\r\n"
        if ((write_bytes = write(client_socket, message, strlen(message))) == -1){
            perror("write");
            exit(1);
        }

        close(client_socket);

        char *message = "WELCOME TO THE CHAT SERVER\r\n"
        if ((write_bytes = write(client_socket, message, strlen(message))) == -1){
            perror("write");
            exit(1);
        }
    }

    // they are connected
    return client_socket;

}

void initialize_client_array(){
    for (int i = 0; i < MAX_CLIENTS; i ++) {
        all_client_array[i]->fd = -1;
    }
}

int client_array_has_room(fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i]->fd != -1){
            return i;
        }

    }
    // no client arary available so return -1
    return -1;
}

/* Assuming there is room inside array */
void add_to_client_array(fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i]->fd != -1){
            all_client_array[i]->fd  = fd;
        }

    }
}