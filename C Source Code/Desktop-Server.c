#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT 2000

int main(void) {   
    int sockfd;
    float headrot[4];
    struct sockaddr_in self;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Error in socket(): \n");
        close(sockfd);
	exit(errno);
    }

	bzero(&self, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_port = htons(SERVER_PORT);
	self.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd, (struct sockaddr*)&self, sizeof(self)) != 0 ) {
        perror("Error in bind(): \n");
        close(sockfd);
	exit(errno);
    }

	if(listen(sockfd, 20) != 0 ) {
            perror("Error in listen(): \n");
            close(sockfd);
            exit(errno);
	}

        printf("Listening...\n");
        
	while (1) {
            int clientfd;
            struct sockaddr_in client_addr;
            socklen_t addrlen=sizeof(client_addr);

            clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
            printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            char *buf;
            buf = malloc(1024);
            
            recv(clientfd, buf, sizeof(buf), 0);
            headrot[0] = atof(buf);
            recv(clientfd, buf, sizeof(buf), 0);
            headrot[1] = atof(buf);
            recv(clientfd, buf, sizeof(buf), 0);
            headrot[2] = atof(buf);
            recv(clientfd, buf, sizeof(buf), 0);
            headrot[3] = atof(buf);
            
            for(int i=0; i<3; i++) {
                printf("headrot: %14.11f\n", headrot[i]);
            }
            
            close(clientfd);
	}

	close(sockfd);
	return 0;
}