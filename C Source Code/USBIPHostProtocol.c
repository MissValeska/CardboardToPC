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
#include <pthread.h>

#include <getopt.h>
#include <signal.h>
#include <poll.h>
#include <assert.h>

#include <libusb-1.0/libusb.h>
#include <libudev.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "usbip_common.h"
#include "usbip_network.h"
#include "list.h"
#include "config.h"
#include "names.h"
#include "USBIPUSBTunnel.h"
#include "USBDataGenerator.h"

#undef  PROGNAME
#define PROGNAME "usbipd"
#define MAXSOCKFD 20

#define MAIN_LOOP_TIMEOUT 10

#define DEFAULT_PID_FILE "/var/run/" PROGNAME ".pid"

#define DBG_UDEV_INTEGER(name)\
	printf("%-20s = %x", to_string(name), (int) udev->name)

#define DBG_UINF_INTEGER(name)\
	printf("%-20s = %x", to_string(name), (int) uinf->name)

static const char usbip_version_string[] = PACKAGE_STRING;

static int do_standalone_mode(int daemonize, int ipv4, int ipv6);

int main(void) {
    do_standalone_mode(0, 0, 0);
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
		printf("getnameinfo: %s", gai_strerror(rc));

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
		printf("opening %s\n", ai_buf);
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			printf("socket: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			continue;
		}

		usbip_net_set_reuseaddr(sock);
		usbip_net_set_nodelay(sock);
		/* We use seperate sockets for IPv4 and IPv6
		 * (see do_standalone_mode()) */
		usbip_net_set_v6only(sock);

		if (sock >= FD_SETSIZE) {
			printf("FD_SETSIZE: %s: sock=%d, max=%d",
			    ai_buf, sock, FD_SETSIZE);
			close(sock);
			continue;
		}

		ret = bind(sock, ai->ai_addr, ai->ai_addrlen);
		if (ret < 0) {
			printf("bind: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			close(sock);
			continue;
		}

		ret = listen(sock, SOMAXCONN);
		if (ret < 0) {
			printf("listen: %s: %d (%s)",
			    ai_buf, errno, strerror(errno));
			close(sock);
			continue;
		}

		printf("listening on %s\n", ai_buf);
		sockfdlist[nsockfd++] = sock;
	}

	return nsockfd;
}

static void signal_handler(int i)
{
	printf("received '%s' signal", strsignal(i));
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
		printf("creating pid file %s", pid_file);
		FILE *fp;

		fp = fopen(pid_file, "w");
		if (!fp) {
			printf("pid_file: %s: %d (%s)",
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
		printf("removing pid file %s", pid_file);
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
		printf("failed to get a network address %s: %s", usbip_port_string,
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
		printf("failed to accept connection\n");
		return -1;
	}

	rc = getnameinfo((struct sockaddr *)&ss, len, host, sizeof(host),
			 port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	if (rc)
		printf("getnameinfo: %s\n", gai_strerror(rc));

#ifdef HAVE_LIBWRAP
	rc = tcpd_auth(connfd);
	if (rc < 0) {
		printf("denied access from %s", host);
		close(connfd);
		return -1;
	}
#endif
	printf("connection from %s:%s\n", host, port);

	return connfd;
}

struct usbip_exported_device {
	struct udev_device *sudev;
	int32_t status;
	struct usbip_usb_device udev;
	struct list_head node;
	struct usbip_usb_interface uinf[];
};

struct usbip_host_driver {
	int ndevs;
	/* list of exported device */
	struct list_head edev_list;
};

struct usbip_host_driver *host_driver;
struct udev *udev_context;

void build_usb_device(struct usbip_exported_device *edev) {
        libusb_context *context = NULL;
    libusb_device **list = NULL;
    int rc = 0;
    ssize_t count = 0;

    rc = libusb_init(&context);
    assert(rc == 0);

    count = libusb_get_device_list(context, &list);
    assert(count > 0);

        libusb_device *device = list[0];
        struct libusb_device_descriptor desc = {0};

        rc = libusb_get_device_descriptor(device, &desc);
        assert(rc == 0);
        
                char vendor[128], product[128];
        char modalias[64];
        char modalias1[64];
        
        sprintf(modalias, "usb:v%04Xp%04X*", desc.idVendor, desc.idProduct);
	sprintf(modalias1, "usb:v%04X*", desc.idVendor);
        
                struct udev_list_entry *entry;
        const char *cp;
        const char *cp1;
        
        struct udev *udev = NULL;
        struct udev_hwdb *hwdb = NULL;
        
        udev = udev_new();
        hwdb = udev_hwdb_new(udev);
        
        udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
            if(strcmp(udev_list_entry_get_name(entry), "ID_MODEL_FROM_DATABASE") == 0) {
            cp = udev_list_entry_get_value(entry);
            }
        
        udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias1, 0))
            if(strcmp(udev_list_entry_get_name(entry), "ID_VENDOR_FROM_DATABASE") == 0) {
            cp1 = udev_list_entry_get_value(entry);
            }
        
        snprintf(product, sizeof(product), "%s", cp);
        snprintf(vendor, sizeof(vendor), "%s", cp1);
        
        printf("Original Vendor %04x Product %04x Product Again: %s Vendor Again: %s\n", 
        desc.idVendor, desc.idProduct, product, vendor);
        
        /* Not all of the struct edev is filled, Which may cause problems in the future,
         * Figuring out what value should go there/generic hard coded value should go there
         * is required to complete this struct if problems arise without doing so.
         * Although, It may be good for this to be complete in the future regardless
         * of whether or not it causes any directly attributable problems/breaking issues. */
        
        edev->udev.bDeviceClass = desc.bDeviceClass;
        edev->udev.bDeviceSubClass = desc.bDeviceSubClass;
        edev->udev.bDeviceProtocol = desc.bDeviceProtocol;
        edev->udev.bNumConfigurations = desc.bNumConfigurations;
        edev->udev.bcdDevice = desc.bcdDevice;
        edev->udev.busnum = libusb_get_bus_number(device);
        edev->udev.speed = libusb_get_device_speed(device);
        edev->udev.bNumInterfaces = 0;   
        strcpy(edev->udev.busid, "1-1");
        edev->status = SDEV_ST_AVAILABLE;
        edev->udev.devnum = edev->udev.busnum;
        
        
        
        //edev->udev.idProduct = desc.idProduct;
        //edev->udev.idVendor = desc.idVendor;
        
        edev->udev.idProduct = 0x0001;
        edev->udev.idVendor = 0x2833;
        
        printf("Built Vendor %04x Product %04x Device %04x Names Product %s\n", edev->udev.idVendor, 
                        edev->udev.idProduct, edev->udev.bcdDevice, names_product(desc.idVendor, desc.idProduct));
}

static int send_reply_devlist(int connfd)
{
    
    	struct usbip_exported_device *edev;
	struct usbip_usb_device pdu_udev;
	struct usbip_usb_interface pdu_uinf;
	struct op_devlist_reply reply;
	struct list_head *j;
	int rc, i;

	reply.ndev = 1;
	printf("exportable devices: %d\n", reply.ndev);

	rc = usbip_net_send_op_common(connfd, OP_REP_DEVLIST, ST_OK);
	if (rc < 0) {
		printf("usbip_net_send_op_common failed: %#0x\n", OP_REP_DEVLIST);
		return -1;
	}
	PACK_OP_DEVLIST_REPLY(1, &reply);

	rc = usbip_net_send(connfd, &reply, sizeof(reply));
	if (rc < 0) {
		printf("usbip_net_send failed: %#0x\n", OP_REP_DEVLIST);
		return -1;
	}
		//edev = list_entry(j, struct usbip_exported_device, node);
                build_usb_device(edev);
		dump_usb_device(&edev->udev);
		memcpy(&pdu_udev, &edev->udev, sizeof(pdu_udev));
                printf("Packed Vendor %04x Product %04x Device %04x\n", pdu_udev.idVendor, 
                        pdu_udev.idProduct, pdu_udev.bcdDevice);
		usbip_net_pack_usb_device(1, &pdu_udev);

		rc = usbip_net_send(connfd, &pdu_udev, sizeof(pdu_udev));
		if (rc < 0) {
			printf("usbip_net_send failed: pdu_udev\n");
			return -1;
		}

		for (i = 0; i < edev->udev.bNumInterfaces; i++) {
			dump_usb_interface(&edev->uinf[i]);
			memcpy(&pdu_uinf, &edev->uinf[i], sizeof(pdu_uinf));
			usbip_net_pack_usb_interface(1, &pdu_uinf);

			rc = usbip_net_send(connfd, &pdu_uinf,
					sizeof(pdu_uinf));
			if (rc < 0) {
				printf("usbip_net_send failed: pdu_uinf\n");
				return -1;
			}
		}

	return 0;
    
}

static int recv_request_devlist(int connfd)
{
        printf("recv_request_devlist\n");
	struct op_devlist_request req;
	int rc;

	memset(&req, 0, sizeof(req));

        printf("net_recv\n");
	rc = usbip_net_recv(connfd, &req, sizeof(req));
	if (rc < 0) {
		printf("usbip_net_recv failed: devlist request\n");
		return -1;
	}

        printf("send_reply\n");
	rc = send_reply_devlist(connfd);
	if (rc < 0) {
		printf("send_reply_devlist failed\n");
		return -1;
	}

        printf("recv_done\n");
	return 0;
}

int write_sysfs_attribute(const char *attr_path, const char *new_value,
			  size_t len)
{
	int fd;
	int length;

	fd = open(attr_path, O_WRONLY);
	if (fd < 0) {
		printf("error opening attribute %s\n", attr_path);
		return -1;
	}

	length = write(fd, new_value, len);
	if (length < 0) {
		printf("error writing to attribute %s\n", attr_path);
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
		printf("device not available: %s\n", edev->udev.busid);
		switch (edev->status) {
		case SDEV_ST_ERROR:
			printf("status SDEV_ST_ERROR\n");
			break;
		case SDEV_ST_USED:
			printf("status SDEV_ST_USED\n");
			break;
		default:
			printf("status unknown: 0x%x", edev->status);
		}
		return -1;
	}

	/* only the first interface is true */
	snprintf(sockfd_attr_path, sizeof(sockfd_attr_path), "%s/%s",
		 edev->udev.path, attr_name);

	snprintf(sockfd_buff, sizeof(sockfd_buff), "%d\n", sockfd);

        /* Make this into a thread, Only run it once */
        
        get_headrot(sockfd);
        
	/*ret = write_sysfs_attribute(sockfd_attr_path, sockfd_buff,
				    strlen(sockfd_buff));
	if (ret < 0) {
		printf("write_sysfs_attribute failed: sockfd %s to %s\n",
		    sockfd_buff, sockfd_attr_path);
		return ret;
	}*/

	printf("connect: %s\n", edev->udev.busid);

	return ret;
}

static int recv_request_import(int sockfd)
{
        printf("recv_request\n");
    
	struct op_import_request req;
	struct usbip_exported_device *edev;
	struct usbip_usb_device pdu_udev;
	struct list_head *i;
	int found = 0;
	int error = 0;
	int rc;

	memset(&req, 0, sizeof(req));

        printf("recv_net_recv\n");
	rc = usbip_net_recv(sockfd, &req, sizeof(req));
	if (rc < 0) {
		printf("usbip_net_recv failed: import request\n");
		return -1;
	}
        printf("recv_pack\n");
	PACK_OP_IMPORT_REQUEST(0, &req);

        printf("recv_list_each\n");
        
	build_usb_device(edev);
	printf("found requested device: %s\n", req.busid);
	found = 1;
        
        printf("recv_list_end\n");

	if (found) {
		/* should set TCP_NODELAY for usbip */
		usbip_net_set_nodelay(sockfd);

		/* export device needs a TCP/IP socket descriptor */
                
		rc = usbip_host_export_device(edev, sockfd);
		if (rc < 0)
			error = 1;
	} else {
		printf("requested device not found: %s\n", req.busid);
		error = 1;
	}

	rc = usbip_net_send_op_common(sockfd, OP_REP_IMPORT,
				      (!error ? ST_OK : ST_NA));
	if (rc < 0) {
		printf("usbip_net_send_op_common failed: %#0x\n", OP_REP_IMPORT);
		return -1;
	}

	if (error) {
		printf("import request busid %s: failed\n", req.busid);
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
		printf("usbip_net_send failed: devinfo\n");
		return -1;
	}

	printf("import request busid %s: complete\n", req.busid);

	return 0;
}

static void usbip_exported_device_destroy(void)
{
        printf("usbip_destroyer\n");
	struct list_head *i, *tmp;
	struct usbip_exported_device *edev;

	list_for_each_safe(i, tmp, &host_driver->edev_list) {
		edev = list_entry(i, struct usbip_exported_device, node);
		list_del(i);
		free(edev);
	}
        printf("usbip_lister\n");
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
		printf("error opening attribute %s\n", status_attr_path);
		return -1;
	}

	length = read(fd, &status, 1);
	if (length < 0) {
		printf("error reading attribute %s\n", status_attr_path);
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
		printf("udev_device_new_from_syspath: %s\n", sdevpath);
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
		printf("realloc failed\n");
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
				printf("usbip_exported_device_new failed\n");
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
		printf("udev_new failed\n");
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
        printf("usbip_refresh\n");
	int rc;

	//usbip_exported_device_destroy();
        
        printf("usbip_destroy\n");
	host_driver->ndevs = 0;
	INIT_LIST_HEAD(&host_driver->edev_list);

        printf("host_driver\n");
	rc = refresh_exported_devices();
        printf("refresh exported\n");
	if (rc < 0)
		return -1;

        printf("usbip_refresh_end\n");
	return 0;
}

static int recv_pdu(int connfd)
{
        printf("recv_pdu\n");
	uint16_t code = OP_UNSPEC;
	int ret;

        printf("recv_pdu1\n");
	ret = usbip_net_recv_op_common(connfd, &code);
        printf("recv_pdu2\n");
	if (ret < 0) {
		printf("could not receive opcode: %#0x\n", code);
		return -1;
	}
        
        printf("recv_pdu3\n");
	//ret = usbip_host_refresh_device_list();
        printf("recv_pdu4\n");
	if (ret < 0) {
		printf("could not refresh device list: %d\n", ret);
		return -1;
	}

	printf("received request: %#0x(%d)\n", code, connfd);
        printf("received it ^_^\n");
	switch (code) {
	case OP_REQ_DEVLIST:
                printf("recv_devlist\n");
		ret = recv_request_devlist(connfd);
		break;
	case OP_REQ_IMPORT:
                printf("recv_import\n");
		ret = recv_request_import(connfd);
		break;
	case OP_REQ_DEVINFO:
	case OP_REQ_CRYPKEY:
	default:
		printf("received an unknown opcode: %#0x\n", code);
		ret = -1;
	}

	if (ret == 0)
		printf("request %#0x(%d): complete\n", code, connfd);
	else
		printf("request %#0x(%d): failed\n", code, connfd);

        printf("recv_pdu_done\n");
        
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
        printf("process\n");
	pid_t childpid;
	int connfd;

	connfd = do_accept(listenfd);
	if (connfd < 0)
		return -1;
	childpid = fork();
	if (childpid == 0) {
                printf("child\n");
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
		printf("please load " USBIP_CORE_MOD_NAME ".ko and "
		    USBIP_HOST_DRV_NAME ".ko!\n");
		return -1;
	}

	if (daemonize) {
		if (daemon(0, 0) < 0) {
			printf("daemonizing failed: %s", strerror(errno));
			usbip_host_driver_close();
			return -1;
		}
		umask(0);
		usbip_use_syslog = 1;
	}*/
	set_signal();
	write_pid_file();

	printf("starting " PROGNAME " (%s)\n", usbip_version_string);

	/*
	 * To suppress warnings on systems with bindv6only disabled
	 * (default), we use seperate sockets for IPv6 and IPv4 and set
	 * IPV6_V6ONLY on the IPv6 sockets.
	 */
	if (ipv4 == 0 && ipv6 == 0)
		family = AF_UNSPEC;
	else if (ipv4 == 0)
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
		printf("failed to open a listening socket\n");
		//usbip_host_driver_close();
		return -1;
	}

	printf("listening on %d address%s\n", nsockfd, (nsockfd == 1) ? "" : "es");

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
                printf("ppoll: %i\n", r);
		if (r < 0) {
			printf("%s", strerror(errno));
			terminate = 1;
		} else if (r) {
			for (i = 0; i < nsockfd; i++) {
				if (fds[i].revents & POLLIN) {
					printf("read event on fd[%d]=%d\n",
					    i, sockfdlist[i]);
					process_request(sockfdlist[i]);
				}
			}
		} else {
			printf("heartbeat timeout on ppoll()\n");
		}
	}

	printf("shutting down %s\n", PROGNAME);
	free(fds);
	//usbip_host_driver_close();

	return 0;
}