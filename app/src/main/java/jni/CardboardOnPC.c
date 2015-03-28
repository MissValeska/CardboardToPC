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
#include <jni.h>

#define SERVER_PORT 2000
#define SERVER_ADDR "192.168.1.64"

    void Java_com_cardboard_missvaleska_cardboardonpc_FullscreenActivity_QuaternionToPC
        (JNIEnv *env, jobject x, jfloatArray javaheadrot) {

        int i;
        float headrot[4];
        jsize len = (*env)->GetArrayLength(env, javaheadrot);
        
        jfloat *body = (*env)->GetFloatArrayElements(env, javaheadrot, 0);
    for(i=0; i<len; i++) {
        headrot[i] = body[i];
    }
        for(i=0; i<len; i++) {
            printf("headrot: %14.11f\n", headrot[i]);
        }
        
        (*env)->ReleaseFloatArrayElements(env, javaheadrot, body, 0);

         int sockfd;
         struct sockaddr_in dest;

         if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error in socket(): \n");
            close(sockfd);
            exit(errno);
         }

         bzero(&dest, sizeof(dest));
         dest.sin_family = AF_INET;
         dest.sin_port = htons(SERVER_PORT);
            if(inet_pton(AF_INET, SERVER_ADDR, &dest.sin_addr.s_addr) == 0) {
            perror(SERVER_ADDR);
            close(sockfd);
            exit(errno);
            }

         if(connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
            perror("Error in connect() \n");
            close(sockfd);
            exit(errno);
         }
        
         char *buf;
         buf = malloc(1024);
         sprintf(buf, "%f", headrot[0]);

         send(sockfd, buf, sizeof(buf), 0);
         sprintf(buf, "%f", headrot[1]);
         send(sockfd, buf, sizeof(buf), 0);
         sprintf(buf, "%f", headrot[2]);
         send(sockfd, buf, sizeof(buf), 0);
         sprintf(buf, "%f", headrot[3]);
         send(sockfd, buf, sizeof(buf), 0);

         close(sockfd);

    }