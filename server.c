struct client {
    int fd;
    char* name;
    char bufer[CLIENT_BUF];
    int buf_len;
    struct in_addr ipaddr;
    int private_room;

}

static struct client add_client(struct client *top, int fd, struct in_addr addr);
static struct client remove_client(struct client *top, int fd);

static void broadcast_everyone(struct client *top, char *s, int size);
static void broadcast_room(struct client *top, char *s, int size);

int handleclient(struct client *p, struct client *top);


int main(int argc, char* argv[]){
    setbuf(stdout, NULL);



}

