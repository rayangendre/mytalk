#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <talk.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h> 
#include <limits.h>
#include <poll.h>


#define WIDTH 100 
#define SOCK_IND 0
#define STD_IND 1
#define BUF_SIZE 1024
int vflag = 0;
int aflag = 0;
int Nflag = 0;

int windowing(int sd){
    int len = 0;
    //set up the polls
    struct pollfd socketfd;
    struct pollfd stdfd;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    //initialize the standard values
    socketfd.events = POLLIN;
    socketfd.fd = sd;
    socketfd.revents = 0;
    stdfd.events = POLLIN;
    stdfd.revents = 0;
    stdfd.fd = STDIN_FILENO;
    //add the poll fds to the list
    struct pollfd fdlist[2] = {socketfd, stdfd};
    int break_flag = 0;
    if(Nflag == 0){
	start_windowing();
    }
    //loop and poll
    while(1){
	//poll the inputs
	poll(fdlist, 2, -1);
	//check the socket poll
	if(fdlist[SOCK_IND].revents & POLLIN){
	   len = recv(sd, buf, BUF_SIZE - 1, 0);
	   buf[len] = '\0';
	   if(len == 0){
		write_to_output("Connection closed.  ^C to terminate.\n", 36);
	        break_flag = 1;
		break;
	   }
	   write_to_output(buf, len);
	   memset(buf, 0, BUF_SIZE);
        }
	//check the standard in poll
	if(fdlist[STD_IND].revents & POLLIN){
	    update_input_buffer();	   
	    memset(buf, 0, BUF_SIZE);    
	    if(has_hit_eof()){
		stop_windowing();
		break;
	    }
	    if(has_whole_line()){
		len = read_from_input(buf, BUF_SIZE-1);
		buf[len] = '\0';
		if(len < 0){
		   perror("Read from input");
		   exit(1);
		}
		send(sd, buf, len, 0);	
	    }

	}	
    }
    if(break_flag == 1){
	while(1){
	}
    } 
    stop_windowing();
    return 0;

}

int client_mode(char *hostname, char  *port){

    int readval;
    int sock;
    char buffer[BUF_SIZE] = { 0 };
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    struct addrinfo *list; 
    //struct addrinfo *list = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    struct addrinfo *current;
    //struct sockaddr_in *server_address;
    struct sockaddr_in *server_address =(struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *copy = server_address;
    if(!server_address){
	perror("Allocation Error");
	exit(1);
    }

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    //add the port number and family to the server struct
    server_address->sin_port = htons(strtol(port, NULL, 10));
    server_address->sin_family = AF_INET;

    int val = 0;
    //this returns a list of address structures
    //Run through these trying to connect to one of these
    val = getaddrinfo(hostname, NULL, &hint, &list);
    if(val != 0){
	perror("Getting Addr Info, PLEASE RUN AGAIN");
	exit(1);
    }
    if(vflag){
        printf("Searching for sock\n");
    }
    sock = -1;
    //go through all the sockets and match on one
    for (current = list;  current != NULL; current = current->ai_next) {
        sock = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (sock == -1)
            continue;

        server_address = (struct sockaddr_in *)current->ai_addr;
        server_address->sin_port = htons(strtol(port, NULL, 10));
        if (connect(sock, (struct sockaddr *)server_address, sizeof(*server_address)) == 0){
            //this one has found and has been binded
            break;
        }

        //not binded, close the pointer to the socket
        close(sock);

    }

    if (sock == -1){
        perror("Socket not found");
        exit(EXIT_FAILURE);
    }
    //get the name of the user
    uid_t uid = getuid();
    struct passwd *pass = getpwuid(uid);
    //send the packet with the name
    send(sock, pass->pw_name, strlen(pass->pw_name), 0);   
    if(vflag){
        printf("Username sent\n");
    }
    printf("Waiting for Response from %s\n", hostname);

    readval = recv(sock, buffer, BUF_SIZE, 0);
    //if ok is returned keep going
    if(readval > 0){
	if(strcmp("ok", buffer)== 0){
	    windowing(sock);
	}else{
	    printf("Exiting");
	    exit(1);
	}
    }else{
	printf("Exit Here");
	exit(1);
    }
    free(copy);
    close(sock);



    return 0;
}


int server_mode(int port){

    int sd;
    int new_sock;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);



    //Open the connection and listen for connections somewhere else in the world
    if (-1 == (sd = socket(AF_INET, SOCK_STREAM, 0))){
        perror("Socket");
        exit(1);
    }
    if(vflag){
	printf("Created Socket Port %d\n",port);
    }
    //bind the socket
    if (0 > (bind(sd, (const struct sockaddr *) &addr, sizeof(addr)))){
        perror("Bind");
        exit(1);
    }

    //if(vflag){
      //  printf("Binded\n");
    //}

    if (listen(sd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr *acc_addr = malloc(sizeof(struct sockaddr));
    int addrlen = sizeof(struct sockaddr);
    if ((new_sock = accept(sd, (struct sockaddr *)&acc_addr, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    char username[100];
    int readval;

    if(vflag){
        printf("Accpeted Connection\n");
    }

    readval = recv(new_sock, username, 100, 0);
    if(readval < 0){
	perror("Receiving");
	exit(1);
    }
    
    char *host = malloc(LOGIN_NAME_MAX);  
 
    if(getnameinfo((struct sockaddr *)&acc_addr, sizeof(struct sockaddr), host, LOGIN_NAME_MAX, NULL, 0, 0)){
	perror("getnameinfo");
	exit(1);
    }
    if(vflag){
        printf("Prompting User\n");
    }    
    char *okmessage = "ok";
    char *badmessage = "bad";
    char str[4];

    if(aflag){
	send(new_sock, okmessage, strlen(okmessage), 0);
    }else{
        printf("Mytalk Request from %s@%s Accept (y/n)\n", username, host);
        if(fgets(str, 4, stdin) == NULL){
	    perror("Fgets");
	    exit(1);
        }
        if ((strcmp(str, "yes\n") == 0) || (strcmp(str, "y\n") == 0)){
	    send(new_sock, okmessage, strlen(okmessage), 0);
        }else{
	    send(new_sock, badmessage, strlen(badmessage), 0);
	    exit(1);
        }
    }
    windowing(new_sock);
   
 
    close(new_sock);
    close(sd);

    

    return 0;
}

int main(int argc, char **argv) {

    int opt;

    while ((opt = getopt(argc, argv, "vaN")) != -1){
        switch (opt) {
            case 'v':
                vflag = 1;
                break;
            case 'a':
                aflag = 1;
                break;
            case 'N':
                Nflag = 1;
                break;
            case '?':
                printf("not an option %c\n", opt);
                break;
            default:
                printf("we got here somehow");
                break;
        }
    }
//    printf("argc %d\n", argc);
//    printf("optind %d\n", optind);
    if ((argc - optind) == 2){
//	printf("Client Mode\n");
        client_mode(argv[argc - 2], argv[argc - 1]);
    } else{
//	printf("Server Mode\n");
        server_mode(strtol(argv[argc - 1], NULL, 10));
    }
    return 0;
}
