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

#include <getopt.h>
#include <signal.h>
#include <poll.h>

#include <libusb-1.0/libusb.h>
#include <libudev.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "usbip_common.h"
#include "usbip_network.h"
#include "list.h"
#include "config.h"

#undef  PROGNAME
#define PROGNAME "usbipd"
#define MAXSOCKFD 20

#define MAIN_LOOP_TIMEOUT 10

#define DEFAULT_PID_FILE "/var/run/" PROGNAME ".pid"

#define DBG_UDEV_INTEGER(name)\
	dbg("%-20s = %x", to_string(name), (int) udev->name)

#define DBG_UINF_INTEGER(name)\
	dbg("%-20s = %x", to_string(name), (int) uinf->name)

static const char usbip_version_string[] = PACKAGE_STRING;

int main(void) {
    
}

static void addrinfo_to_text(struct addrinfo *ai, char buf[],
			     const size_t buf_size)
{
	char hbuf[NI_MAXHOST];
	char sbuf[NI_MAXSERV];
	int rc;

	buf[0] = '\0';

	rc = getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf, sizeof(hbuf),
			 sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc)
		err("getnameinfo: %s", gai_strerror(rc));

	snprintf(buf, buf_size, "%s:%s", hbuf, sbuf);
}

static int listen_all_addrinfo(struct addrinfo *ai_head, int sockfdlist[],
			     int maxsockfd)
{
	struct addrinfo *ai;
	int ret, nsockfd = 0;
	const size_t ai_buf_size = NI_MAXHOST + NI_MAXSERV + 2;
	char ai_buf[ai_buf_size];

	for (ai = ai_head; ai && nsockfd < maxsockfd; ai = ai->ai_next) {
		int sock;

		addrinfo_to_text(ai, ai_buf, ai_buf_size);
		dbg("opening %s", ai_buf);
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			err("socket: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			continue;
		}

		usbip_net_set_reuseaddr(sock);
		usbip_net_set_nodelay(sock);
		/* We use seperate sockets for IPv4 and IPv6
		 * (see do_standalone_mode()) */
		usbip_net_set_v6only(sock);

		if (sock >= FD_SETSIZE) {
			err("FD_SETSIZE: %s: sock=%d, max=%d",
			    ai_buf, sock, FD_SETSIZE);
			close(sock);
			continue;
		}

		ret = bind(sock, ai->ai_addr, ai->ai_addrlen);
		if (ret < 0) {
			err("bind: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			close(sock);
			continue;
		}

		ret = listen(sock, SOMAXCONN);
		if (ret < 0) {
			err("listen: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			close(sock);
			continue;
		}

		info("listening on %s", ai_buf);
		sockfdlist[nsockfd++] = sock;
	}

	return nsockfd;
}

static void signal_handler(int i)
{
	dbg("received '%s' signal", strsignal(i));
}

static void set_signal(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	act.sa_handler = SIG_IGN;
	sigaction(SIGCLD, &act, NULL);
}

static const char *pid_file;

static void write_pid_file(void)
{
	if (pid_file) {
		dbg("creating pid file %s", pid_file);
		FILE *fp;

		fp = fopen(pid_file, "w");
		if (!fp) {
			err("pid_file: %s: %d (%s)",
			    pid_file, errno, strerror(errno));
			return;
		}
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

static void remove_pid_file(void)
{
	if (pid_file) {
		dbg("removing pid file %s", pid_file);
		unlink(pid_file);
	}
}

static struct addrinfo *do_getaddrinfo(char *host, int ai_family)
{
	struct addrinfo hints, *ai_head;
	int rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = ai_family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;

	rc = getaddrinfo(host, usbip_port_string, &hints, &ai_head);
	if (rc) {
		err("failed to get a network address %s: %s", usbip_port_string,
		    gai_strerror(rc));
		return NULL;
	}

	return ai_head;
}

static int do_accept(int listenfd)
{
	int connfd;
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	char host[NI_MAXHOST], port[NI_MAXSERV];
	int rc;

	memset(&ss, 0, sizeof(ss));

	connfd = accept(listenfd, (struct sockaddr *)&ss, &len);
	if (connfd < 0) {
		err("failed to accept connection");
		return -1;
	}

	rc = getnameinfo((struct sockaddr *)&ss, len, host, sizeof(host),
			 port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc)
		err("getnameinfo: %s", gai_strerror(rc));

#ifdef HAVE_LIBWRAP
	rc = tcpd_auth(connfd);
	if (rc < 0) {
		info("denied access from %s", host);
		close(connfd);
		return -1;
	}
#endif
	info("connection from %s:%s", host, port);

	return connfd;
}

struct usbip_exported_device {
	struct udev_device *sudev;
	int32_t status;
	struct usbip_usb_device udev;
	struct list_head node;
	struct usbip_usb_interface uinf[];
};


static int send_reply_devlist(int connfd)
{
    
    	struct usbip_exported_device *edev;
	struct usbip_usb_device pdu_udev;
	struct usbip_usb_interface pdu_uinf;
	struct op_devlist_reply reply;
	struct list_head *j;
	int rc, i;

	reply.ndev = 0;
	/* number of exported devices */
	list_for_each(j, &host_driver->edev_list) {
		reply.ndev += 1;
	}
	info("exportable devices: %d", reply.ndev);
	info("exportable devices: %d", reply.ndev);

	rc = usbip_net_send_op_common(connfd, OP_REP_DEVLIST, ST_OK);
	if (rc < 0) {
		dbg("usbip_net_send_op_common failed: %#0x", OP_REP_DEVLIST);
		return -1;
	}
	PACK_OP_DEVLIST_REPLY(1, &reply);

	rc = usbip_net_send(connfd, &reply, sizeof(reply));
	if (rc < 0) {
		dbg("usbip_net_send failed: %#0x", OP_REP_DEVLIST);
		return -1;
	}
        list_for_each(j, &host_driver->edev_list) {
		edev = list_entry(j, struct usbip_exported_device, node);
                edev->udev.idProduct = 0x0001;
                edev->udev.idVendor = 0x2833;
		dump_usb_device(&edev->udev);
		memcpy(&pdu_udev, &edev->udev, sizeof(pdu_udev));
                printf("Vendor %04x Device %04x\n", pdu_udev.idVendor, 
                        pdu_udev.idProduct);
		usbip_net_pack_usb_device(1, &pdu_udev);

		rc = usbip_net_send(connfd, &pdu_udev, sizeof(pdu_udev));
		if (rc < 0) {
			dbg("usbip_net_send failed: pdu_udev");
			return -1;
		}

		for (i = 0; i < edev->udev.bNumInterfaces; i++) {
			dump_usb_interface(&edev->uinf[i]);
			memcpy(&pdu_uinf, &edev->uinf[i], sizeof(pdu_uinf));
			usbip_net_pack_usb_interface(1, &pdu_uinf);

			rc = usbip_net_send(connfd, &pdu_uinf,
					sizeof(pdu_uinf));
			if (rc < 0) {
				err("usbip_net_send failed: pdu_uinf");
				return -1;
			}
		}
        }

	return 0;
    
}

static int recv_request_devlist(int connfd)
{
	struct op_devlist_request req;
	int rc;

	memset(&req, 0, sizeof(req));

	rc = usbip_net_recv(connfd, &req, sizeof(req));
	if (rc < 0) {
		dbg("usbip_net_recv failed: devlist request");
		return -1;
	}

	rc = send_reply_devlist(connfd);
	if (rc < 0) {
		dbg("send_reply_devlist failed");
		return -1;
	}

	return 0;
}

int write_sysfs_attribute(const char *attr_path, const char *new_value,
			  size_t len)
{
	int fd;
	int length;

	fd = open(attr_path, O_WRONLY);
	if (fd < 0) {
		dbg("error opening attribute %s", attr_path);
		return -1;
	}

	length = write(fd, new_value, len);
	if (length < 0) {
		dbg("error writing to attribute %s", attr_path);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int usbip_host_export_device(struct usbip_exported_device *edev, int sockfd)
{
	char attr_name[] = "usbip_sockfd";
	char sockfd_attr_path[SYSFS_PATH_MAX];
	char sockfd_buff[30];
	int ret;

	if (edev->status != SDEV_ST_AVAILABLE) {
		dbg("device not available: %s", edev->udev.busid);
		switch (edev->status) {
		case SDEV_ST_ERROR:
			dbg("status SDEV_ST_ERROR");
			break;
		case SDEV_ST_USED:
			dbg("status SDEV_ST_USED");
			break;
		default:
			dbg("status unknown: 0x%x", edev->status);
		}
		return -1;
	}

	/* only the first interface is true */
	snprintf(sockfd_attr_path, sizeof(sockfd_attr_path), "%s/%s",
		 edev->udev.path, attr_name);

	snprintf(sockfd_buff, sizeof(sockfd_buff), "%d\n", sockfd);

	ret = write_sysfs_attribute(sockfd_attr_path, sockfd_buff,
				    strlen(sockfd_buff));
	if (ret < 0) {
		err("write_sysfs_attribute failed: sockfd %s to %s",
		    sockfd_buff, sockfd_attr_path);
		return ret;
	}

	info("connect: %s", edev->udev.busid);

	return ret;
}

static int recv_request_import(int sockfd)
{
	struct op_import_request req;
	struct usbip_exported_device *edev;
	struct usbip_usb_device pdu_udev;
	struct list_head *i;
	int found = 0;
	int error = 0;
	int rc;

	memset(&req, 0, sizeof(req));

	rc = usbip_net_recv(sockfd, &req, sizeof(req));
	if (rc < 0) {
		dbg("usbip_net_recv failed: import request");
		return -1;
	}
	PACK_OP_IMPORT_REQUEST(0, &req);

	list_for_each(i, &host_driver->edev_list) {
		edev = list_entry(i, struct usbip_exported_device, node);
		if (!strncmp(req.busid, edev->udev.busid, SYSFS_BUS_ID_SIZE)) {
			info("found requested device: %s", req.busid);
			found = 1;
			break;
		}
	}

	if (found) {
		/* should set TCP_NODELAY for usbip */
		usbip_net_set_nodelay(sockfd);

		/* export device needs a TCP/IP socket descriptor */
                
                edev->udev.idProduct = 0x0001;
                edev->udev.idVendor = 0x2833;
                
		rc = usbip_host_export_device(edev, sockfd);
		if (rc < 0)
			error = 1;
	} else {
		info("requested device not found: %s", req.busid);
		error = 1;
	}

	rc = usbip_net_send_op_common(sockfd, OP_REP_IMPORT,
				      (!error ? ST_OK : ST_NA));
	if (rc < 0) {
		dbg("usbip_net_send_op_common failed: %#0x", OP_REP_IMPORT);
		return -1;
	}

	if (error) {
		dbg("import request busid %s: failed", req.busid);
		return -1;
	}

	memcpy(&pdu_udev, &edev->udev, sizeof(pdu_udev));
        edev->udev.idProduct = 0x0001;
        edev->udev.idVendor = 0x2833;
	usbip_net_pack_usb_device(1, &pdu_udev);
        
        printf("Vendor %04x Device %04x\n", pdu_udev.idVendor, 
                        pdu_udev.idProduct);
        
	rc = usbip_net_send(sockfd, &pdu_udev, sizeof(pdu_udev));
	if (rc < 0) {
		dbg("usbip_net_send failed: devinfo");
		return -1;
	}

	dbg("import request busid %s: complete", req.busid);

	return 0;
}

struct usbip_host_driver {
	int ndevs;
	/* list of exported device */
	struct list_head edev_list;
};

/*struct usbip_exported_device {
	struct udev_device *sudev;
	int32_t status;
	struct usbip_usb_device udev;
	struct list_head node;
	struct usbip_usb_interface uinf[];
};*/

struct usbip_host_driver *host_driver;
struct udev *udev_context;

static void usbip_exported_device_destroy(void)
{
	struct list_head *i, *tmp;
	struct usbip_exported_device *edev;

	list_for_each_safe(i, tmp, &host_driver->edev_list) {
		edev = list_entry(i, struct usbip_exported_device, node);
		list_del(i);
		free(edev);
	}
}

int usbip_host_driver_open(void);

static int32_t read_attr_usbip_status(struct usbip_usb_device *udev)
{
	char status_attr_path[SYSFS_PATH_MAX];
	int fd;
	int length;
	char status;
	int value = 0;

	snprintf(status_attr_path, SYSFS_PATH_MAX, "%s/usbip_status",
		 udev->path);

	fd = open(status_attr_path, O_RDONLY);
	if (fd < 0) {
		err("error opening attribute %s", status_attr_path);
		return -1;
	}

	length = read(fd, &status, 1);
	if (length < 0) {
		err("error reading attribute %s", status_attr_path);
		close(fd);
		return -1;
	}

	value = atoi(&status);

	return value;
}

static
struct usbip_exported_device *usbip_exported_device_new(const char *sdevpath)
{
	struct usbip_exported_device *edev = NULL;
	struct usbip_exported_device *edev_old;
	size_t size;
	int i;

	edev = calloc(1, sizeof(struct usbip_exported_device));

	edev->sudev = udev_device_new_from_syspath(udev_context, sdevpath);
	if (!edev->sudev) {
		err("udev_device_new_from_syspath: %s", sdevpath);
		goto err;
	}

	read_usb_device(edev->sudev, &edev->udev);

	edev->status = read_attr_usbip_status(&edev->udev);
	if (edev->status < 0)
		goto err;

	/* reallocate buffer to include usb interface data */
	size = sizeof(struct usbip_exported_device) +
		edev->udev.bNumInterfaces * sizeof(struct usbip_usb_interface);

	edev_old = edev;
	edev = realloc(edev, size);
	if (!edev) {
		edev = edev_old;
		dbg("realloc failed");
		goto err;
	}

	for (i = 0; i < edev->udev.bNumInterfaces; i++)
		read_usb_interface(&edev->udev, i, &edev->uinf[i]);

	return edev;
err:
	if (edev->sudev)
		udev_device_unref(edev->sudev);
	if (edev)
		free(edev);

	return NULL;
}

static int refresh_exported_devices(void)
{
	struct usbip_exported_device *edev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	const char *path;
	const char *driver;

	enumerate = udev_enumerate_new(udev_context);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev_context, path);
		if (dev == NULL)
			continue;

		/* Check whether device uses usbip-host driver. */
		driver = udev_device_get_driver(dev);
		if (driver != NULL && !strcmp(driver, USBIP_HOST_DRV_NAME)) {
			edev = usbip_exported_device_new(path);
			if (!edev) {
				dbg("usbip_exported_device_new failed");
				continue;
			}

			list_add(&edev->node, &host_driver->edev_list);
			host_driver->ndevs++;
		}
	}

	return 0;
}

int usbip_host_driver_open(void)
{
	int rc;

	udev_context = udev_new();
	if (!udev_context) {
		err("udev_new failed");
		return -1;
	}

	host_driver = calloc(1, sizeof(*host_driver));

	host_driver->ndevs = 0;
	INIT_LIST_HEAD(&host_driver->edev_list);

	rc = refresh_exported_devices();
	if (rc < 0)
		goto err_free_host_driver;

	return 0;

err_free_host_driver:
	free(host_driver);
	host_driver = NULL;

	udev_unref(udev_context);

	return -1;
}

int usbip_host_refresh_device_list(void)
{
	int rc;

	usbip_exported_device_destroy();

	host_driver->ndevs = 0;
	INIT_LIST_HEAD(&host_driver->edev_list);

	rc = refresh_exported_devices();
	if (rc < 0)
		return -1;

	return 0;
}

static int recv_pdu(int connfd)
{
	uint16_t code = OP_UNSPEC;
	int ret;

	ret = usbip_net_recv_op_common(connfd, &code);
	if (ret < 0) {
		dbg("could not receive opcode: %#0x", code);
		return -1;
	}

	ret = usbip_host_refresh_device_list();
	if (ret < 0) {
		dbg("could not refresh device list: %d", ret);
		return -1;
	}

	info("received request: %#0x(%d)", code, connfd);
	switch (code) {
	case OP_REQ_DEVLIST:
		ret = recv_request_devlist(connfd);
		break;
	case OP_REQ_IMPORT:
		ret = recv_request_import(connfd);
		break;
	case OP_REQ_DEVINFO:
	case OP_REQ_CRYPKEY:
	default:
		err("received an unknown opcode: %#0x", code);
		ret = -1;
	}

	if (ret == 0)
		info("request %#0x(%d): complete", code, connfd);
	else
		info("request %#0x(%d): failed", code, connfd);

	return ret;
}

#ifdef HAVE_LIBWRAP
static int tcpd_auth(int connfd)
{
	struct request_info request;
	int rc;

	request_init(&request, RQ_DAEMON, PROGNAME, RQ_FILE, connfd, 0);
	fromhost(&request);
	rc = hosts_access(&request);
	if (rc == 0)
		return -1;

	return 0;
}
#endif

int process_request(int listenfd)
{
	pid_t childpid;
	int connfd;

	connfd = do_accept(listenfd);
	if (connfd < 0)
		return -1;
	childpid = fork();
	if (childpid == 0) {
		close(listenfd);
		recv_pdu(connfd);
		exit(0);
	}
	close(connfd);
	return 0;
}

static int do_standalone_mode(int daemonize, int ipv4, int ipv6)
{
	struct addrinfo *ai_head;
	int sockfdlist[MAXSOCKFD];
	int nsockfd, family;
	int i, terminate;
	struct pollfd *fds;
	struct timespec timeout;
	sigset_t sigmask;

	/*if (usbip_host_driver_open()) {
		err("please load " USBIP_CORE_MOD_NAME ".ko and "
		    USBIP_HOST_DRV_NAME ".ko!");
		return -1;
	}

	if (daemonize) {
		if (daemon(0, 0) < 0) {
			err("daemonizing failed: %s", strerror(errno));
			usbip_host_driver_close();
			return -1;
		}
		umask(0);
		usbip_use_syslog = 1;
	}*/
	set_signal();
	write_pid_file();

	info("starting " PROGNAME " (%s)", usbip_version_string);

	/*
	 * To suppress warnings on systems with bindv6only disabled
	 * (default), we use seperate sockets for IPv6 and IPv4 and set
	 * IPV6_V6ONLY on the IPv6 sockets.
	 */
	if (ipv4 && ipv6)
		family = AF_UNSPEC;
	else if (ipv4)
		family = AF_INET;
	else
		family = AF_INET6;

	ai_head = do_getaddrinfo(NULL, family);
	if (!ai_head) {
		//usbip_host_driver_close();
		return -1;
	}
	nsockfd = listen_all_addrinfo(ai_head, sockfdlist,
		sizeof(sockfdlist) / sizeof(*sockfdlist));
	freeaddrinfo(ai_head);
	if (nsockfd <= 0) {
		err("failed to open a listening socket");
		//usbip_host_driver_close();
		return -1;
	}

	dbg("listening on %d address%s", nsockfd, (nsockfd == 1) ? "" : "es");

	fds = calloc(nsockfd, sizeof(struct pollfd));
	for (i = 0; i < nsockfd; i++) {
		fds[i].fd = sockfdlist[i];
		fds[i].events = POLLIN;
	}
	timeout.tv_sec = MAIN_LOOP_TIMEOUT;
	timeout.tv_nsec = 0;

	sigfillset(&sigmask);
	sigdelset(&sigmask, SIGTERM);
	sigdelset(&sigmask, SIGINT);

	terminate = 0;
	while (!terminate) {
		int r;

		r = ppoll(fds, nsockfd, &timeout, &sigmask);
		if (r < 0) {
			dbg("%s", strerror(errno));
			terminate = 1;
		} else if (r) {
			for (i = 0; i < nsockfd; i++) {
				if (fds[i].revents & POLLIN) {
					dbg("read event on fd[%d]=%d",
					    i, sockfdlist[i]);
					process_request(sockfdlist[i]);
				}
			}
		} else {
			dbg("heartbeat timeout on ppoll()");
		}
	}

	info("shutting down " PROGNAME);
	free(fds);
	//usbip_host_driver_close();

	return 0;
}