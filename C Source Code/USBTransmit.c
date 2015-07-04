#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <libusb-1.0/libusb.h>
#include <libudev.h>

int USBTransmit(void) {
    
    libusb_context *context = NULL;
    libusb_device **list = NULL;
    int rc = 0;
    ssize_t count = 0;

    rc = libusb_init(&context);
    assert(rc == 0);

    count = libusb_get_device_list(context, &list);
    assert(count > 0);

    for (size_t idx = 0; idx < count; ++idx) {
        libusb_device *device = list[idx];
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
        
        printf("Vendor %04x Device %04x %s %s\n", desc.idVendor, desc.idProduct, vendor, product);
        //desc.
    }
    return 0;
}