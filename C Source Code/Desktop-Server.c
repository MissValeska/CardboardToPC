#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
//#include <linux/usbip.h>

#include <math.h>

#include "USBTransmit.c"
#include "CursorPos.h"

#define SERVER_PORT 2000

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

struct HeadRot{
    float swing;//which way the user is facing on the plane paralell to the ground
    float tilt;//the angle between the horizon and the user's line of sight
    float twist;//the angle the user has their head tilted away from level. 
    //NOTE: since we expect the phone to be turned sideways, twist when the user's head is level should be 90
};

//utility function that converts radians to degrees
float radiansToDegrees(const float radians){
    return (radians * 180) / M_PI;
}

//Prints the head rotation passed to it in degrees (assumes HeadRot is in radians)
void printHeadRot(const struct HeadRot rot){
    
    printf("swing: %2.2f degrees\n", radiansToDegrees(rot.swing));
    printf("tilt:  %2.2f degrees\n", radiansToDegrees(rot.tilt));
    printf("twist: %2.2f degrees\n",(radiansToDegrees(rot.twist) - 90));//-90 because we expect the phone to be sideways (90 degree twist)
}

int main(void) {   
    int sockfd;
    float headRotData[4];
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

            if(recv(clientfd, headRotData, sizeof(headRotData), 0) == -1) {
                printf("recv issues, clientfd and errno: %d %d\n", clientfd, errno);
            }
            
            //Copy the data received into a nice structure
            struct HeadRot headRot;
            headRot.tilt  = headRotData[0];
            headRot.swing = headRotData[1];
            headRot.twist = headRotData[2];
            
            
            //Print the head rotation TODO: Valeska, put your mouse code here
            printHeadRot(headRot);
            
            /*
            for(int i=0; i<4; i++) {
                printf("buf: %f\n", headrot[i]);
            }
            */
            close(clientfd);
	}

        //MoveCursorPos(x, y);
        
	close(sockfd);
	return 0;
}
