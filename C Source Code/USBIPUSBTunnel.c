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

#include "USBDataGenerator.h"

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
			    const char *buf, size_t count)
{
	struct vhci_device *vdev;
	struct socket *socket;
	int sockfd = 0;
	__u32 rhport = 0, devid = 0, speed = 0;
	int err;

	/*
	 * @rhport: port number of vhci_hcd
	 * @sockfd: socket descriptor of an established TCP connection
	 * @devid: unique device identifier in a remote host
	 * @speed: usb device speed in a remote host
	 */
	/*if (sscanf(buf, "%u %u %u %u", &rhport, &sockfd, &devid, &speed) != 4)
		return -EINVAL;

	usbip_dbg_vhci_sysfs("rhport(%u) sockfd(%u) devid(%u) speed(%u)\n",
			     rhport, sockfd, devid, speed);

	/* check received parameters */
	/*if (valid_args(rhport, speed) < 0)
		return -EINVAL;

	/* Extract socket from fd. */
	/*socket = sockfd_lookup(sockfd, &err);
	if (!socket)
		return -EINVAL;

	/* now need lock until setting vdev status as used */

	/* begin a lock */
	/*spin_lock(&the_controller->lock);
	vdev = port_to_vdev(rhport);
	spin_lock(&vdev->ud.lock);

	if (vdev->ud.status != VDEV_ST_NULL) {
		/* end of the lock */
		/*spin_unlock(&vdev->ud.lock);
		spin_unlock(&the_controller->lock);

		sockfd_put(socket);

		dev_err(dev, "port %d already used\n", rhport);
		return -EINVAL;
	}

	dev_info(dev,
		 "rhport(%u) sockfd(%d) devid(%u) speed(%u) speed_str(%s)\n",
		 rhport, sockfd, devid, speed, usb_speed_string(speed));

	vdev->devid         = devid;
	vdev->speed         = speed;
	vdev->ud.tcp_socket = socket;
	vdev->ud.status     = VDEV_ST_NOTASSIGNED;

	spin_unlock(&vdev->ud.lock);
	spin_unlock(&the_controller->lock);
	/* end the lock */

	/*vdev->ud.tcp_rx = kthread_get_run(vhci_rx_loop, &vdev->ud, "vhci_rx");
	vdev->ud.tcp_tx = kthread_get_run(vhci_tx_loop, &vdev->ud, "vhci_tx");

	rh_port_connect(rhport, speed);

	return count;
}*/

static int send_cmd_unlink(void) {
    
}

static int create_usb_tunnel(int sockfd, void *buf, size_t len) {
    
	/*struct usbip_device *ud = data;
	struct vhci_device *vdev = container_of(ud, struct vhci_device, ud);

	while (!kthread_should_stop()) {
		if (vhci_send_cmd_submit(vdev) < 0)
			break;

		if (vhci_send_cmd_unlink(vdev) < 0)
			break;

		wait_event_interruptible(vdev->waitq_tx,
					 (!list_empty(&vdev->priv_tx) ||
					  !list_empty(&vdev->unlink_tx) ||
					  kthread_should_stop()));

		usbip_dbg_vhci_tx("pending urbs ?, now wake up\n");
	}

	return 0;*/
        
        struct sockaddr_in client_addr;
        socklen_t addrlen=sizeof(client_addr);

        getpeername(sockfd, (struct sockaddr *)client_addr, addrlen);
            
        printf("%s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        sendmsg(sockfd, *buf, len, );
        
        /*if(recv(sockfd, headrot, sizeof(headrot), 0) == -1) {
            printf("recv issues, clientfd and errno: %d %d\n", sockfd, errno);
        }*/
        
        /* I think this runs immediately after, Not sure yet */
        
        /*send_cmd_unlink();*/
            
	return 0;
}