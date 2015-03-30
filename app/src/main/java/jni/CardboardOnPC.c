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
#include <android/log.h>

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
            __android_log_print(ANDROID_LOG_VERBOSE, "headrot", "%14.11f", headrot[i]);
        }
        
        (*env)->ReleaseFloatArrayElements(env, javaheadrot, body, 0);

         int sockfd;
         struct sockaddr_in dest;

         if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            __android_log_print(ANDROID_LOG_VERBOSE, "Error in socket()", "%i", errno);
            close(sockfd);
            //exit(errno);
         }

         bzero(&dest, sizeof(dest));
         dest.sin_family = AF_INET;
         dest.sin_port = htons(SERVER_PORT);
            if((dest.sin_addr.s_addr = inet_addr(SERVER_ADDR)) == -1) {
            __android_log_print(ANDROID_LOG_VERBOSE, "Error in inet_pton", "%s %i", SERVER_ADDR, errno);
            close(sockfd);
            //exit(errno);
            }

        __android_log_print(ANDROID_LOG_VERBOSE, "SERVER_ADDR", "%s", SERVER_ADDR);

        __android_log_print(ANDROID_LOG_VERBOSE, "dest addr", "%s", inet_ntoa(dest.sin_addr));

         if(connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) != 0) {
            __android_log_print(ANDROID_LOG_VERBOSE, "Error in connect()", "%i", errno);
            close(sockfd);
            //exit(errno);
         }

        send(sockfd, headrot, sizeof(headrot), 0);

        __android_log_print(ANDROID_LOG_VERBOSE, "buf", "%f", headrot[0]);

         close(sockfd);

    }