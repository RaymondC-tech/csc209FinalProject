#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <netinet/in.h>    /* Internet domain header, for struct sockaddr_in */
#include <stdbool.h>

#define MAX_BUF 1024

struct sockaddr_in *server_address_struct(int port);
int set_up_server_socket(struct sockaddr_in *self, int num_queue);

int connect_to_server(int port, const char *hostname);

int bindandlisten(int port);

int accept_connection(int listenfd);

int find_network_newline(const char *buf, int n, bool full_network); 

#endif