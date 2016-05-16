/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   OSVR-Cardboard.c
 * Author: valeska
 *
 * Created on May 16, 2016, 3:27 AM
 */

#include <stdio.h>
#include <stdlib.h>

/* Internal Includes */
#include <osvr/PluginKit/PluginKitC.h>
#include <osvr/PluginKit/AnalogInterfaceC.h>
#include <osvr/PluginKit/ButtonInterfaceC.h>
#include <osvr/PluginKit/TrackerInterfaceC.h>
#include <osvr/Util/BoolC.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <jsoncpp/json/json.h>

#define SHMSZ 1024

typedef struct GoogleCardboardHMD {
    OSVR_DeviceToken devToken;
    //OSVR_AnalogDeviceInterface analog;
    OSVR_ButtonDeviceInterface button;
    OSVR_TrackerDeviceInterface tracker;
    //double myVal;
    OSVR_Quaternion Quaternion;
    OSVR_CBool buttonPressed;
} GoogleCardboardHMD;
 
/*void jsonthing(void) {
    
    Json::Value root;
    root["deviceVender"] = "Google"//getManufacturer();
    root["deviceName"] = "Google Cardboard HMD"//getProductName();
    root["author"] = "Valeska Victoria <missvaleskavv@gmail.com>";
    root["version"] = 1;
    root["lastModified"] = "2016-05-16T4:54:14Z";
    root["interfaces"]["tracker"]["count"] = 1; // I guess? //getTrackerCount();
    //root["interfaces"]["tracker"]["position"] = 0; // FIXME
    root["interfaces"]["tracker"]["orientation"] = 1; // FIXME
    root["semantic"]["camera"]["$target"] = "tracker/1";
    root["semantic"]["leveled_camera"]["$target"] = "tracker/2";
    root["automaticAliases"]["/me/head"] = "semantic/hmd";

    std::ostringstream ostr;
    ostr << root;

    return ostr.str();
}*/

static OSVR_HardwareDetectCallback myDeviceDetect(void *userdata) {
    
    FILE *fp;
    char *c;
    
    fp = fopen("participants-num.txt", "r");
    
    fgets(c, 100, fp);
    
    if(strcmp(c, "Oui") == 0) {
        return OSVR_RETURN_SUCCESS;
    }
    
    if(strcmp(c, "Non") == 0) {
        return OSVR_RETURN_FAILURE;
    }
    
}

static OSVR_ReturnCode myDeviceUpdate(void *userdata) {
    GoogleCardboardHMD *mydev = (GoogleCardboardHMD *)userdata;
     /* dummy time-wasting loop - simulating "blocking waiting for device data",
      * which is possible because this is an async device. */
     int i;
     //for (i = 0; i < 1000; ++i) {
     //}
 
     /* Make up some dummy data that changes to report. */
     /*mydev->myVal = (mydev->myVal + 0.1);
     if (mydev->myVal > 10.) {
         mydev->myVal = 0.;
     }*/
 
     /* Report the value of channel 0 */
     //osvrDeviceAnalogSetValue(mydev->devToken, mydev->analog, mydev->myVal, 0);
 
     /* Toggle the button 0 */
     /*if (OSVR_TRUE == mydev->buttonPressed) {
         mydev->buttonPressed = OSVR_FALSE;
     } else {
         mydev->buttonPressed = OSVR_TRUE;
     }*/
     /*osvrDeviceButtonSetValue(mydev->devToken, mydev->button,
                              (mydev->buttonPressed == OSVR_TRUE)
                                  ? OSVR_BUTTON_PRESSED
                                  : OSVR_BUTTON_NOT_PRESSED,
                              0);*/
 
     /* Report the identity pose for sensor 0 */
     //OSVR_PoseState pose;
     //osvrPose3SetIdentity(&pose);
     //osvrDeviceTrackerSendPose(mydev->devToken, mydev->tracker, &pose, 0);
 
     //OSVR_Quaternion quat;
     //osvrQuatSetIdentity(quat);
     
     OSVR_Quaternion quat;
     
    int shmid;
    key_t key;
    //char *shm, *s;

    /*
     * We need to get the segment named
     * "5678", created by the server.
     */
    key = 5678;

    /*
     * Locate the segment.
     */
    if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((quat = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }

    mydev->Quaternion = quat;
    
    if (shmdt(quat) == -1) {
        perror("shmdt");
        exit(1);
    }
    
    /*
     * Now read what the server put in the memory.
     */
    /*for (s = shm; *s != NULL; s++)
        putchar(*s);
    putchar('\n');*/

    /*
     * Finally, change the first character of the 
     * segment to '*', indicating we have read 
     * the segment.
     */
    //*shm = '*';
     
     osvrDeviceTrackerSendOrientation(mydev->devToken, mydev->tracker, &mydev->Quaternion, 0);
     
     return OSVR_RETURN_SUCCESS;
 }
 
 /* Startup routine for a device instance - similar function as the constructor
  * in the C++ examples. Note that error checking (return values) has been
  * omitted for clarity - see documentation. */
 static void myDeviceInit(OSVR_PluginRegContext ctx, GoogleCardboardHMD *mydev) {
     /* Set initial values in the struct. */
     //mydev->myVal = 0.;
     //mydev->buttonPressed = OSVR_FALSE;
 
     /* Create the initialization options */
     OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);
 
     /* Indicate that we'll want 1 analog channel. */
     //osvrDeviceAnalogConfigure(opts, &mydev->analog, 1);
 
     /* Indicate that we'll want 1 button. */
     //osvrDeviceButtonConfigure(opts, &mydev->button, 1);
 
     /* Indicate that we'll report tracking too. */
     osvrDeviceTrackerConfigure(opts, &mydev->tracker);
 
     /* Create the async device token with the options */
     osvrDeviceSyncInitWithOptions(ctx, "GoogleCardboardHMD", opts, &mydev->devToken);
 
    /* Send the JSON device descriptor. */
     //osvrDeviceSendJsonDescriptor(mydev->devToken,
                                  //org_osvr_example_SampleCPlugin_json,
                                  //sizeof(org_osvr_example_SampleCPlugin_json));
 
     /* Register Hardware Detect Callback */
     osvrPluginRegisterHardwareDetectCallback(ctx, &myDeviceDetect, NULL);
     
     /* Register update callback */
     osvrDeviceRegisterUpdateCallback(mydev->devToken, &myDeviceUpdate,
                                      (void *)mydev);
     
 }
 /* Shutdown and free routine for a device instance - parallels the combination
  * of the destructor and `delete`/`osvr::util::generic_deleter<>` in the C++
  * examples */
 static void myDeviceShutdown(void *mydev) {
     printf("Shutting down Google Cardboard device\n");
     free(mydev);
 }
 
 OSVR_PLUGIN(org_osvr_GoogleCardboardPlugin) {
     /* Allocate a struct for our device data. */
     GoogleCardboardHMD *mydev = (GoogleCardboardHMD *)malloc(sizeof(GoogleCardboardHMD));
 
     /* Ask the server to tell us when to shutdown the device and free this
      * struct */
     osvrPluginRegisterDataWithDeleteCallback(ctx, &myDeviceShutdown,
                                              (void *)mydev);
 
     /* Call a function to set up the device callbacks, etc. */
     myDeviceInit(ctx, mydev);
 
     return OSVR_RETURN_SUCCESS;
 }

