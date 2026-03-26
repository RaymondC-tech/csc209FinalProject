#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_CLIENTS 32
#define MAX_CHANNELS 32
#define MAX_QUEUE 20
#define MAX_NAME 40
#define MAX_CHANNEL_NAME 40
#define MAX_CHANNEL_PER_CLIENT 4

#ifndef PORT
  #define PORT 57179
#endif

#include "socket.h"

struct client {
    int fd;
    char name[40];
    char buf[MAX_BUF];
    int buf_len;
    int channel[MAX_CHANNEL_PER_CLIENT];
    char* after;
    int buf_room;
};

struct channel {
    int id;
    char name[MAX_CHANNEL_NAME];
};

struct client all_client_array[MAX_CLIENTS];
struct channel all_channel_array[MAX_CLIENTS];


static void initialize_client_array();
static int client_array_has_room(int fd);
static void add_to_client_array(int fd);
static struct client* return_client_struct(int fd, char* name);

static struct client add_client(int listen_soc, char *name);
static void remove_client(struct client *select_client, fd_set *allset, int *maxfd);


static void initialize_channel_array();
static struct channel* get_channel_struct(char *channel_name);
static int exists_channel_room(char *channel_name);


static int handle_client_orchestration(int fd, struct client *select_client, fd_set *allset, int *maxfd);
static int handle_client_action(int fd, struct client *select_client, fd_set *allset, int *maxfd);

static void broadcast_everyone(char *message);
static void broadcast_room(struct client *select_client, char *channel_name, char *message);

static bool check_empty_input(struct client* select_client, char *position, char *message);
static bool extract_content(char *dest, char *src, int offset, char condition);


int main(int argc, char* argv[]){
    // initialize the client array and channel array to be default -1 file descriptor each
    initialize_client_array();
    initialize_channel_array();

    // socket + bind + listen
    int listenfd = bindandlisten(PORT); 

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

        // 1ST ACTION: Client request to join
        if (FD_ISSET(listenfd, &tmpset)) {
            // 1. error checking for accept
            if ((client_fd = accept_connection(listenfd)) == -1) {
                perror("accept");
                exit(1);
            }
            // 2. see if there is enough space to add to the client_array
            int space_client_array = client_array_has_room(client_fd);
            int write_bytes;

            // 3. if no space tell them no space else add them to array
            if (space_client_array == -1) {
                char *message = "Chat Server Reached Max Capacity. Try Again Later!!\r\n";
                if ((write_bytes = write(client_fd, message, strlen(message))) == -1){
                    perror("write");
                    exit(1);
                }
                close(client_fd);
            }
            else {
                // custom socket created for communicatoin between client and server
                add_to_client_array(client_fd);
                char *message = "Welcome. One More Step: Do /join:<name> To Join\r\n";
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

        // 2nd ACTION. RESPONDING TO EXISTING CLIENT'S REQUEST
        for(int i = 0; i <= maxfd; i++) {
            if (i != listenfd && FD_ISSET(i, &tmpset)) {
                // call our orchestartion function to hanle it
                struct client* client_strut = return_client_struct(i, NULL);
                int result = handle_client_orchestration(i, client_strut, &allset, &maxfd);
            }
        }
    }
    // LAST ACTION: CLOSE SERVER LISTENING SOCKET
    close(listenfd);
}

static int handle_client_orchestration(int fd, struct client *select_client, fd_set *allset, int *maxfd){
    // read message and delegate task to handle_client_action
    int nbytes;
    nbytes = read(fd, select_client->after, select_client->buf_room);
    if (nbytes == 0) {
        // socket has closed mid connection
        remove_client(select_client, allset, maxfd);
        return -1;
    }
    else if (nbytes > 0) {
        select_client->buf_len += nbytes;
        int message_end;
        
        while ((message_end = find_network_newline(select_client->buf, select_client->buf_len, true)) != -1){
            int result = handle_client_action(fd, select_client, allset, maxfd);
            if (result == -1) {
                // client disconnected
                break;
            }
            
            // no need to subtract since pointer arithmetic gives int
            memmove(select_client->buf, &select_client->buf[message_end], (select_client->buf_len - message_end)); 

            select_client->buf_len = select_client->buf_len - message_end;
            
            // if select_client_buf_len == message_end + 2
            if (select_client->buf_len == 0) {
                memset(select_client->buf, '\0', sizeof(select_client->buf));
            }
        }
        select_client->after = &select_client->buf[select_client->buf_len]; 
        select_client->buf_room = MAX_BUF - select_client->buf_len;
    }
    else {
        perror("read");
        exit(1);
    }
    return 1;
}

static int handle_client_action(int fd, struct client *select_client, fd_set *allset, int *maxfd){
    // final_message are all mesages with person who sent it + message like Alice: So call guys
    char final_message[MAX_BUF];

    // server_message is the specific message send by the person
    char server_message[MAX_BUF];

    // server_event are not direclty messages to someone, but broadcast an event
    // Alice: joined coffee channel
    char server_event[MAX_BUF];
    char *position;

    // 1. JOIN COMMAND [/join:<name>]
    if(strncmp(select_client->buf, "/join:", 6) == 0) {
        // 1. Check if message is empty
        position = (select_client->buf) + 6;
        char *message = "Message Blank\r\n";

        if (check_empty_input(select_client, position, message)){
            return 1;
        }

        // 2. not empty, get the name, add the name, and write message
        int i = 6;
        while(select_client->buf[i] != '\r'){
            select_client->name[i - 6] = select_client->buf[i];
            i += 1;
        }
        select_client->name[i - 6] = '\0';

        snprintf(final_message, sizeof(final_message), "WELCOME %s To Chat Application\r\n", select_client->name); 

        if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
            perror("write");
            exit(1); //chagne ghe static functon header + this sissdue
        }
    }

    else{
        // HERE ARE ALL THE NON-JOIN COMMANDS. CLIENT MUST JOIN FIRST BEFORE DOING ANYTHING. THIS WILL BE ONE OF THE ERROR CHECKING WE WILL WRITE ON REPORT
        if (select_client->name[0] == '\0'){
            char *message = "You Must Do Command: /join:<name> First\r\n";
            if (write(select_client->fd, message, strlen(message)) == -1){
                perror("write");
                exit(1);
            }
            return 1;
        }

        // 2. MESSAGE EVERYONE COMMAND [msg_all:<message>]
        if (strncmp(select_client->buf, "/msg_all:", 9) == 0) {
            // 1. check if message is empty
            position = select_client->buf + 9;
            char *message = "Message Empty\r\n";

            if (check_empty_input(select_client, position, message)){
                return 1;
            }
            bool valid_command = extract_content(server_message, select_client->buf, 9, '\r');

            if (!valid_command) {
                char *message = "Invalid Command\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }
            // 2. not empty, get the person name who sent it, get the message, broadcast message to everyone
            snprintf(final_message, sizeof(final_message),"%s: %s\r\n", select_client->name, server_message);

            broadcast_everyone(final_message);
        }
        else if(strncmp(select_client->buf, "/msg_channel_", 13) == 0){
            // see if they put channel name
            position = (select_client->buf) + 13;
            char *message = "Channel Name Missing! Try again\r\n";

            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // see if the channel exists + valid command
            char channel_name[MAX_CHANNEL_NAME] = {'\0'};
            // check if there is a : before calling helper for safety
            if (strstr(select_client->buf, ":") == NULL){
                char *message = "Message Format Wrong. Try again\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            bool valid_command = extract_content(channel_name, select_client->buf, 13, ':');
            struct channel* channel_struct = get_channel_struct(channel_name);

            if (channel_struct == NULL){
                char *message = "Channel Name Does Not Exist! Try again\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }
            
            if (!valid_command) {
                char *message = "Invalid Command\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // see if they are in the channel already
            bool in_channel = false;

            for (int i = 0; i < MAX_CHANNEL_PER_CLIENT; i ++){
                if (select_client->channel[i] == channel_struct->id){
                    in_channel = true;
                }
            }

            if (!in_channel){
                char *message = "You Are Not In The Channel. Join First!\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // extract the message, ignore return value, message always has \r
            extract_content(server_message, select_client->buf, 13 + strlen(channel_name) + 1, '\r');

            // see if they inputed a message:
            if (server_message[0] == '\0'){
                char *message = "Message Is Empty. Try Again\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }
            snprintf(final_message, sizeof(final_message),"%s: %s\r\n", select_client->name, server_message);

            // message everyone in the channel
            broadcast_room(select_client, channel_name, final_message);

        // messaging mutiple people /msg_multichannel_<channel_name, channel_name>:<message>
        } else if(strncmp(select_client->buf, "/msg_multichannel_", 18) == 0){
            // first check if there is a :
            if (strstr(select_client->buf, ":") == NULL){
                char *message = "Message Format Is Wrong. Try again\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }
            // get the part of the string specifying all the channels
            char channel_names[(MAX_CHANNEL_NAME * MAX_CHANNELS) + 31] = {'\0'};
            extract_content(channel_names, select_client->buf, 18, ':');

            // check message is empty
            position = select_client->buf + 18 + strlen(channel_names) + 1;
            char *message = "Message Is Blank\r\n";

            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // extract the message, ignore return value, message always has \r
            extract_content(server_message, select_client->buf, 18 + strlen(channel_names) + 1, '\r');
            snprintf(final_message, sizeof(final_message),"%s: %s\r\n", select_client->name, server_message);
     
            // loop through it and call the function to write to everyone else inside those channels
            char* segment = strtok(channel_names, ",");
            while(segment != NULL) {
                broadcast_room(select_client, segment, final_message);
                segment = strtok(NULL, ",");
            }
        }

        // 3. Message a specific person: /msg_<person>:<message>
        else if(strncmp(select_client->buf, "/msg_", 5) == 0) {
            // extract the person name to send message to out
            char sender_name[MAX_NAME] = {'\0'};

            // check if they have a : in the request
            bool valid_command = extract_content(sender_name, select_client->buf, 5, ':');

            if (!valid_command) {
                char *message = "Invalid Command. Please Follow The Commands Exactly\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // check if they even specified the person
            if (sender_name[0] == '\0'){
                char *message = "Did not specify a person to message\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1){
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // check if that person exists
            struct client *receiver_struct = return_client_struct(-1, sender_name);

            if (receiver_struct == NULL) {
                char *message = "Person You Message Does Not Exists\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1){
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // check if message is empty
            position = select_client->buf + 5 + strlen(sender_name) + 1;
            char *message = "Message Is Empty\r\n";
                
            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // extract the message, ignore return value, message always has \r
            extract_content(server_message, select_client->buf, 5 + strlen(sender_name) + 1, '\r');
            snprintf(final_message, sizeof(final_message),"%s: %s\r\n", select_client->name, server_message);

            // message to that person
            if (write(receiver_struct->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        }
        else if(strncmp(select_client->buf, "/create_channel:", 16) == 0) {
            // see if they put channel name
            position = (select_client->buf) + 16;
            char *message = "Channel Name Did Not Specify!\r\n";

            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // extract the channel name out
            char channel_name[MAX_CHANNEL_NAME];
            extract_content(channel_name, select_client->buf, 16, '\r');

            // see if there are any more rooms to add channel or duplicate channel name
            int channel_array_index = exists_channel_room(channel_name);

            if (channel_array_index == -1){
                char *message = "Max Channel Created For The Server. Sorry\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            if (channel_array_index == -2){
                char *message = "Duplicated Channel Name. Use Different Name\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            all_channel_array[channel_array_index].id = channel_array_index;
            strcpy(all_channel_array[channel_array_index].name, channel_name);

            snprintf(final_message, sizeof(final_message),"Successfully Created Channel: %s\r\n", all_channel_array[channel_array_index].name);

            // send confirming message that you joined channel channel
            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        }
        // JOIN A CHANNEL
        else if(strncmp(select_client->buf, "/join_channel:", 14) == 0) {
            // see if they put channel name
            position = (select_client->buf) + 14;
            char *message = "Channel Name did Not Specify!\r\n";

            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // get channel name
            char channel_name[MAX_BUF];
            extract_content(channel_name, select_client->buf, 14, '\r');

            // see if the channel exists
            struct channel* channel_struct = get_channel_struct(channel_name);

            if (channel_struct == NULL){
                char *message = "Channel Name Does Not Exist!\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // see if they are in the channel already OR if they reached max channel limit
            bool in_channel;
            int num_channel_join = 0;
            int valid_index;

            for (int i = 0; i < MAX_CHANNEL_PER_CLIENT; i ++){
                if (select_client->channel[i] == channel_struct->id){
                    in_channel = true;
                }

                if (select_client->channel[i] != -1) {
                    num_channel_join += 1;
                }
                else{
                    valid_index = i;
                }
            }

            if (in_channel){
                char *message = "You Are Already In This Channel.\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }
            if (num_channel_join == MAX_CHANNEL_PER_CLIENT){
                char *message = "You Are In Max Number Of Channels. Leave One First Before You Join This One\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // add channel id to the client struct and give messsage
            select_client->channel[valid_index] = channel_struct->id;

            snprintf(final_message, sizeof(final_message),"Successfully Join Channel: %s\r\n", channel_struct->name);

            // send confirming message that you joined channel
            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        } else if(strncmp(select_client->buf, "/leave_channel:", 15) == 0) {
            // see if they put channel name
            position = (select_client->buf) + 15;
            char *message = "Channel Name Did Not Specify! Try Again\r\n";
            if (check_empty_input(select_client, position, message)){
                return 1;
            }

            // get channel name
            char channel_name[MAX_BUF];
            extract_content(channel_name, select_client->buf, 15, '\r');

            // see if the channel exists
            struct channel* channel_struct = get_channel_struct(channel_name);

            if (channel_struct == NULL){
                char *message = "Channel Name Does Not Exist! Try Again\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // see if they are in the channel already
            int client_channel_index = -1;

            for (int i = 0; i < MAX_CHANNEL_PER_CLIENT; i ++){
                if (select_client->channel[i] == channel_struct->id){
                    client_channel_index = i;
                }
            }

            if (client_channel_index == -1){
                char *message = "You Are Not In This Channel. Cannot Leave\r\n";
                if (write(select_client->fd, message, strlen(message)) == -1) {
                    perror("write");
                    exit(1);
                }
                return 1;
            }

            // remove channel id in client struct and give messsage
            select_client->channel[client_channel_index] = -1;

            snprintf(final_message, sizeof(final_message),"Successfully Left Channel: %s\r\n", channel_struct->name);

            // send confirming message that you left channel
            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        } else if(strncmp(select_client->buf, "/who_online", 11) == 0) {
            // assume that results fit here
            char result[MAX_BUF] = {'\0'};
            for (int i = 0; i < MAX_CLIENTS; i ++){
                if (all_client_array[i].fd != -1){
                    if (result[0] != '\0') {
                        strcat(result, ", ");
                    }
                    strcat(result, all_client_array[i].name);
                }
            }
            snprintf(final_message, sizeof(final_message), "People Online: %s\r\n", result);

            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        } else if(strncmp(select_client->buf, "/list_channel", 13) == 0) {
            // assume that results fit here
            char result[(MAX_CHANNEL_NAME * 4) + 6] = {'\0'};
            for (int i = 0; i < MAX_CHANNEL_PER_CLIENT; i ++){
                if (select_client->channel[i] != -1){
                    if (result[0] != '\0') {
                        strcat(result, ", ");
                    }
                    // the value of at channel[i] if not -1 is the id of the channel which is the same as the index inside all_channel_array
                    strcat(result, all_channel_array[select_client->channel[i]].name);
                }
            }

            snprintf(final_message, sizeof(final_message), "Channels Currently In: %s\r\n", result);

            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        } 
        else if(strncmp(select_client->buf, "/list_command", 13) == 0) {
            // assume that results fit here
            char *result = "Join Chat Application: /join:<name>\r\n"
                            "Message Everyone in Application: /msg_all<message>\r\n"
                           "Message Specific Person: /msg_<person_name>:<message>\r\n"
                            "Message Specific Channel: /msg_channel_<channel_name>:<message>\r\n"
                            "Message Multiple Channel: /msg_multichannel_<channel_name>,<channel_name>....:<message>\r\n"
                            "Join Channel: /join_channel:<channel_name>\r\n"
                            "Create Channel: /create_channel:<channel_name>\r\n"
                            "Leave Channel leave_channel:<channel_name>\r\n"
                            "Check Who Is Online: /who_online\r\n"
                            "List Channels You Are Currently In: /list_channel\r\n"
                            "List Commands Chat Application Handles: /list_command\r\n"
                            "Leave Chat Server: /quit";

            snprintf(final_message, sizeof(final_message), "Chat Application Commands:\n%s\r\n", result);

            if (write(select_client->fd, final_message, strlen(final_message)) == -1) {
                perror("write");
                exit(1);
            }
        } else if(strncmp(select_client->buf, "/quit", 5) == 0) {
            // reset client_array for this client
            remove_client(select_client, allset, maxfd);
            return -1; // -1 to symbol when we return, do not memove anbd just continue

        }
        else {
            // check to see if the user joined first before hand
            char *message = "Invalid Command Try Again\r\n";
            if (write(select_client->fd, message, strlen(message)) == -1) {
                perror("write");
                exit(1);
            }
        }
    }
    return 1;
}

static void remove_client(struct client *select_client, fd_set *allset, int *maxfd){
    int tmp_fd = select_client->fd;
    select_client->fd = -1;
    memset(select_client->name, '\0', sizeof(select_client->name));
    memset(select_client->buf, '\0', sizeof(select_client->buf));
    select_client->buf_len = 0;
    memset(select_client->channel, -1, sizeof(select_client->channel));
    select_client->after  = select_client->buf;
    select_client->buf_room = MAX_BUF;
    
    FD_CLR(tmp_fd, allset);
    close(tmp_fd);
    if (tmp_fd == *maxfd){
        *maxfd -= 1;
    }
}
static void broadcast_everyone(char *message){
    for (int i = 0; i < MAX_CLIENTS; i ++){
        if (all_client_array[i].fd != -1) {
            if (write(all_client_array[i].fd, message, strlen(message)) == -1) {
                perror("write");
                exit(1);
            }
        }
    }
}

static void broadcast_room(struct client *select_client, char *channel_name, char *message){

    struct channel* channel_struct = get_channel_struct(channel_name);

    if (channel_struct == NULL) {
        return;
    }

    // check if select_client is inside the room to broadcast it
    bool sender_in_channel = false;
    for (int i = 0; i < MAX_CHANNEL_PER_CLIENT; i++) {
        if (select_client->channel[i] == channel_struct->id){
            sender_in_channel = true;
            break;
        }
    }

    if (!sender_in_channel){
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i ++){
        if (all_client_array[i].fd != -1){
            for (int j = 0; j < MAX_CHANNEL_PER_CLIENT; j ++){
                if (all_client_array[i].channel[j] == channel_struct->id){
                    if (write(all_client_array[i].fd, message, strlen(message)) == -1) {
                        perror("write");
                        exit(1);
                    }
                }
            }
        }
    }
}

static void initialize_client_array(){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        all_client_array[i].fd = -1;
    }
}

static int client_array_has_room(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i].fd == -1){
            return i;
        }
    }
    // no client arary available so return -1
    return -1;
}

/* Assuming there is room inside array */
static void add_to_client_array(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (all_client_array[i].fd == -1){
            all_client_array[i].fd = fd;
            memset(all_client_array[i].name, '\0', sizeof(all_client_array[i].name));
            memset(all_client_array[i].buf, '\0', sizeof(all_client_array[i].buf));
            all_client_array[i].buf_len = 0;
            memset(all_client_array[i].channel, -1, sizeof(all_client_array[i].channel));
            all_client_array[i].after  = all_client_array[i].buf;
            all_client_array[i].buf_room = MAX_BUF;
            break;
        }
    }
}

static struct client* return_client_struct(int fd, char *name){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fd != -1){
            if (all_client_array[i].fd == fd){
                return &all_client_array[i];
            }
        }
        else {
            if (strcmp(all_client_array[i].name, name) == 0){
                return &all_client_array[i];
            }
        }
    }
    return NULL;
}

static void initialize_channel_array(){
    for (int i = 0; i < MAX_CHANNELS; i++) {
        all_channel_array[i].id = -1;
        memset(all_channel_array[i].name, '\0', sizeof(all_channel_array[i].name));
    }
}

static struct channel* get_channel_struct(char *channel_name) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(all_channel_array[i].name, channel_name) == 0){
            return &all_channel_array[i];
        }
    }
    return NULL;
}

static int exists_channel_room(char *channel_name){
    int channel_array_index = -1;
    for(int i = 0; i < MAX_CHANNELS; i ++){
        if (all_channel_array[i].id == -1){
            channel_array_index = i;
        }
        if (strcmp(all_channel_array[i].name, channel_name) == 0){
            return -2;
        }
    }
    return channel_array_index;
}

static bool check_empty_input(struct client* select_client, char *position, char *message){
    if (*position == '\r' && *(position + 1) == '\n') {
        if (write(select_client->fd, message, strlen(message)) == -1) {
            perror("write");
            exit(1);
        }
        return true;
    }
    return false;
}

static bool extract_content(char *dest, char *src, int offset, char condition){
    int i = offset;
    while (src[i] != condition && src[i] != '\r') {
        dest[i - offset] = src[i];
        i += 1;
    }

    // condition did not get meet
    if (src[i] == '\r' && condition != '\r'){
        return false;
    }
    dest[i - offset] = '\0';

    return true;
}
