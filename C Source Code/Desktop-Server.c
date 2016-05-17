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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

//#include <linux/usbip.h>

#include <math.h>

#include "USBTransmit.c"
#include "CursorPos.h"

#define SHMSZ 1024

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

inline void SendQuaternion(float headRotData[4]) {
    
    //char c;
    int shmid;
    key_t key;
    //char *shm, *s;

    float headRotData1[4];
    
    /*
     * We'll name our shared memory segment
     * "5678".
     */
    key = 5678;

    /*
     * Create the segment.
     */
    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((headRotData1 = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }

    /*
     * Now put some things into the memory for the
     * other process to read.
     */
    //s = shm;

    //for (c = 'a'; c <= 'z'; c++)
    //    *s++ = c;
    //*s = NULL;

    headRotData = headRotData1;
    
    /*
     * Finally, we wait until the other process 
     * changes the first character of our memory
     * to '*', indicating that it has read what 
     * we put there.
     */
    /*while (*shm != '*')
        sleep(1);*/
    
    /* detach from the segment: */
    if (shmdt(data) == -1) {
        perror("shmdt");
        exit(1);
    }
    
    //exit(0);
    
}

int main(void) {   
    int sockfd;
    int connection_num = 0;
    float headRotData[4];
    struct sockaddr_in self;

    //USBTransmit();
    
    FILE *fp;

    fp = fopen("IsCardboardConnected.txt", "w+");
            
    fprintf(fp, "%s\n", "Non");
            
    fclose(fp);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        //perror("Error in socket(): \n");
        close(sockfd);
	exit(errno);
    }

	bzero(&self, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_port = htons(SERVER_PORT);
	self.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd, (struct sockaddr*)&self, sizeof(self)) != 0 ) {
        //perror("Error in bind(): \n");
        close(sockfd);
	exit(errno);
    }

	if(listen(sockfd, 20) != 0 ) {
            //perror("Error in listen(): \n");
            close(sockfd);
            exit(errno);
	}

        //printf("Listening...\n");
        
	while (1) {
            int clientfd;
            struct sockaddr_in client_addr;
            socklen_t addrlen=sizeof(client_addr);

            /* Check if still connected */
            
            /*fp = fopen("IsCardboardConnected.txt", "w+");
            
            fprintf(fp, "%s\n", "Non");
            
            fclose(fp);*/
            
            clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
            /*if(clientfd == -1) {
                printf("accept issues: %d", errno);
            }*/
            //printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if(recv(clientfd, headRotData, sizeof(headRotData), 0) == -1) {
                //printf("recv issues, clientfd and errno: %d %d\n", clientfd, errno);
                ;
            }
            
            /* Consider making this a thread to improve performance, But be careful
             * of thread unsafe things happening! */
            
            SendQuaternion(headRotData);
            
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

            if(connection_num == 0) {
            fp = fopen("IsCardboardConnected.txt", "w+");
            
            fprintf(fp, "%s\n", "Oui");
            
            fclose(fp);
            }
            
            connection_num++;
            
            close(clientfd);
	}

        //MoveCursorPos(x, y);
        
	close(sockfd);
	return 0;
}
