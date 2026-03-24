


int handleclient(struct client *p, struct client *top);

int main(int argc, char* argv[]){
    setbuf(stdout, NULL);

    int fd;

    if ((fd = socket(AF_INET, SOCKET_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    connect()



    close()

}