#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "babble_server.h"
#include "babble_types.h"
#include "babble_utils.h"
#include "babble_communication.h"



static void display_help(char *exec)
{
    printf("Usage: %s -p port_number\n", exec);
}





int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    int portno=BABBLE_PORT;
    
    int opt;
    int nb_args=1;

    

    while ((opt = getopt (argc, argv, "+p:")) != -1){
        switch (opt){
        case 'p':
            portno = atoi(optarg);
            nb_args+=2;
            break;
        case 'h':
        case '?':
        default:
            display_help(argv[0]);
            return -1;
        }
    }
    
    if(nb_args != argc){
        display_help(argv[0]);
        return -1;
    }

    server_data_init();

    if((sockfd = server_connection_init(portno)) == -1){
        return -1;
    }

    printf("Babble server bound to port %d\n", portno);    
    
    /* main server loop */
    while(1){
        if((newsockfd= server_connection_accept(sockfd))==-1){
            return -1;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_connection, (void *) &newsockfd);

        // bzero(client_name, BABBLE_ID_SIZE+1);
        // if((recv_size = network_recv(newsockfd, (void**)&recv_buff)) < 0){
        //     fprintf(stderr, "Error -- recv from client\n");
        //     close(newsockfd);
        //     continue;
        // }

        // cmd = new_command(0);
        
        // if(parse_command(recv_buff, cmd) == -1 || cmd->cid != LOGIN){
        //     fprintf(stderr, "Error -- in LOGIN message\n");
        //     close(newsockfd);
        //     free(cmd);
        //     continue;
        // }

        // /* before processing the command, we should register the
        //  * socket associated with the new client; this is to be done only
        //  * for the LOGIN command */
        // cmd->sock = newsockfd;
    
        // if(process_command(cmd) == -1){
        //     fprintf(stderr, "Error -- in LOGIN\n");
        //     close(newsockfd);
        //     free(cmd);
        //     continue;    
        // }

        // /* notify client of registration */
        // if(answer_command(cmd) == -1){
        //     fprintf(stderr, "Error -- in LOGIN ack\n");
        //     close(newsockfd);
        //     free(cmd);
        //     continue;
        // }

        // /* let's store the key locally */
        // client_key = cmd->key;

        // strncpy(client_name, cmd->msg, BABBLE_ID_SIZE);
        // free(recv_buff);
        // free(cmd);

        // /* looping on client commands */
        // while((recv_size=network_recv(newsockfd, (void**) &recv_buff)) > 0){
        //     cmd = new_command(client_key);
        //     if(parse_command(recv_buff, cmd) == -1){
        //         fprintf(stderr, "Warning: unable to parse message from client %s\n", client_name);
        //         notify_parse_error(cmd, recv_buff);
        //     }
        //     else{
        //         if(process_command(cmd) == -1){
        //             fprintf(stderr, "Warning: unable to process command from client %lu\n", client_key);
        //         }
        //         if(answer_command(cmd) == -1){
        //             fprintf(stderr, "Warning: unable to answer command from client %lu\n", client_key);
        //         }
        //     }
        //     free(recv_buff);
        //     free(cmd);
        // }

        // if(client_name[0] != 0){
        //     cmd = new_command(client_key);
        //     cmd->cid= UNREGISTER;
            
        //     if(unregisted_client(cmd)){
        //         fprintf(stderr,"Warning -- failed to unregister client %s\n",client_name);
        //     }
        //     free(cmd);
        // }
    }
    close(sockfd);
    return 0;
}
