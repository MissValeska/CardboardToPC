#include <stdio.h>
#include <string.h>
#include "com_cardboard_missvaleska_cardboardonpc_SendToPC.h"

int main(int argc, const char* argv[]) {

    JNIEXPORT void JNICALL Java_com_cardboard_missvaleska_cardboardonpc_SendToPC_QuaternionToPC
        (JNIEnv *env, jclass clazz, jfloatArray javaheadrot) {

        int i;
        float sum[4];
        jsize len = (*env)->GetArrayLength(env, javaheadrot);
        
        jfloat *body = (*env)->GetFloatArrayElements(env, javaheadrot, 0);
    for (i=0; i<len; i++) {
        sum[i] = body[i];
    }
        for (i=0; i<len; i++) {
            printf("%14.11f", sum[i]);
        }
        
        (*env)->ReleaseFloatArrayElements(env, javaheadrot, body, 0);
    }
    
  return 0;
}