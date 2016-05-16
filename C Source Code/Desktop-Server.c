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
#include <netinet/tcp.h>
#include <linux/usbip.h>
#include "USBTransmit.c"
#include "CursorPos.h"

#define SERVER_PORT 2000

int main(void) {   
    int sockfd;
    float headrot[4];
    struct sockaddr_in self;

    USBTransmit();
    
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
            if(clientfd == -1) {
                printf("accept issues: %d", errno);
            }
            printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if(recv(clientfd, headrot, sizeof(headrot), 0) == -1) {
                printf("recv issues, clientfd and errno: %d %d\n", clientfd, errno);
            }
            
            for(int i=0; i<4; i++) {
                printf("buf: %f\n", headrot[i]);
            }
            
            close(clientfd);
	}

        //MoveCursorPos(x, y);
        
	close(sockfd);
	return 0;
}