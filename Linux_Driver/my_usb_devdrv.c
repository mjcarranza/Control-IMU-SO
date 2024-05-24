#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>

/* Meta information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel");
MODULE_DESCRIPTION("A driver for my Arduino USB device");

#define VENDOR_ID 0x2341
#define PRODUCT_ID 0x0043

static struct usb_device_id my_usb_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {},
};
MODULE_DEVICE_TABLE(usb, my_usb_table);

static int my_usb_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    printk("my_usb_devdrv - Probe Function\n");
    return 0;
}

static void my_usb_disconnect(struct usb_interface *intf) {
    printk("my_usb_devdrv - Disconnect Function\n");
}

static struct usb_driver my_usb_driver = {
    .name = "my_usb_devdrv",
    .id_table = my_usb_table,
    .probe = my_usb_probe,
    .disconnect = my_usb_disconnect,
};

static int __init my_init(void) {
    int result;
    printk("my_usb_devdrv - Init Function 5\n");

    result = usb_register(&my_usb_driver);
    if (result) {
        printk("my_usb_devdrv - Error during register\n");
        return -result;
    }
    return 0;
}

static void __exit my_exit(void) {
    printk("my_usb_devdrv - Exit Function 5\n");
    usb_deregister(&my_usb_driver);
}

module_init(my_init);
module_exit(my_exit);

