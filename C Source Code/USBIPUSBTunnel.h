/* 
 * File:   USBIPUSBTunnel.h
 * Author: missvaleska
 *
 * Created on July 30, 2015, 8:12 PM
 */

#ifndef USBIPUSBTUNNEL_H
#define	USBIPUSBTUNNEL_H

#ifdef	__cplusplus
extern "C" {
#endif

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

#define SERVER_PORT 2000
    
    
/*
 * To start a new USB/IP attachment, a userland program needs to setup a TCP
 * connection and then write its socket descriptor with remote device
 * information into this sysfs file.
 *
 * A remote device is virtually attached to the root-hub port of @rhport with
 * @speed. @devid is embedded into a request to specify the remote device in a
 * server host.
 *
 * write() returns 0 on success, else negative errno.
 */
/*static ssize_t store_attach(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count);*/

static int create_usb_tunnel(int sockfd, void *buf, size_t len);

static int send_cmd_unlink(void);

#ifdef	__cplusplus
}
#endif

#endif	/* USBIPUSBTUNNEL_H */

