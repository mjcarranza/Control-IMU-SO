#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <asm/uaccess.h>


// define the arduino vendor and product ID
#define ARDUINO_VENDOR_ID    0x2341
#define ARDUINO_PRODUCT_ID   0x0043

// table with the devices that can work with this driver
static struct usb_device_id tableOfDevices [] = {
    {USB_DEVICE(ARDUINO_VENDOR_ID, ARDUINO_PRODUCT_ID)},
    {}
};

// define the device table
MODULE_DEVICE_TABLE(usb, tableOfDevices);

// Get a minor range for your devices from the usb maintainer 
#define USB_SKEL_MINOR_BASE  192

// Structure to hold all of our device specific stuff 
struct usb_skel {
    struct usb_device *udev;              // the usb device for this device
    struct usb_interface *interface;      // the interface for this device
    unsigned char *bulk_in_buffer;        // the buffer to receive data
    size_t bulk_in_size;                  // the size of the receive buffer
    __u8 bulk_in_endpointAddr;            // the address of the bulk in endpoint
    __u8 bulk_out_endpointAddr;           // the address of the bulk out endpoint
    struct kref kref;
};

#define to_skel_dev(d) container_of(d, struct usb_skel, kref)

// it is define the struct to the usb driver that we are going to use
static struct usb_driver driver;

// delete the reference to the usb device with the kfree
static void usb_delete(struct kref *kref)
{
    struct usb_skel *dev = to_skel_dev(kref);

    usb_put_dev(dev->udev);
    kfree(dev->bulk_in_buffer);
    kfree(dev);
}

// Open the device driver, when it is going to be used
static int usb_open(struct inode *inode, struct file *file)
{
    struct usb_skel *dev;
    struct usb_interface *interface;
    int subminor;
    int retval = 0;

    subminor = iminor(inode);

    interface = usb_find_interface(&driver, subminor);
    if (!interface) {
        pr_err("%s - error, can't find device for minor %d",
             __FUNCTION__, subminor);
        retval = -ENODEV;
        goto exit;
    }

    dev = usb_get_intfdata(interface);
    if (!dev) {
        retval = -ENODEV;
        goto exit;
    }
    
    // increment our usage count for the device 
    kref_get(&dev->kref);

    // save our object in the file's private structure
    file->private_data = dev;

exit:
    return retval;
}

// As the name of the function says, this method is used to read the information that gives the driver
// to be used in this case to warn the driver program that the instruction is ready.
static ssize_t usb_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    struct usb_skel *dev;
    int retval = 0;
    int actual_length;

    dev = (struct usb_skel *)file->private_data;
    
    // do a blocking bulk read to get data from the device 
    retval = usb_bulk_msg(dev->udev,
                          usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                          dev->bulk_in_buffer,
                          min(dev->bulk_in_size, count),
                          &actual_length, HZ*10);

    // if the read was successful, copy the data to userspace 
    if (!retval) {
        if (copy_to_user(buffer, dev->bulk_in_buffer, actual_length))
            retval = -EFAULT;
        else
            retval = actual_length;
    }

    return retval;
}

// This function is used with the write usb function 
static void usb_write_bulk_callback(struct urb *urb)
{
    struct usb_skel *dev = urb->context;

    // sync/async unlink faults aren't errors 
    if (urb->status && 
        !(urb->status == -ENOENT || 
          urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN)) {
        dev_dbg(&dev->interface->dev,
                "%s - nonzero write bulk status received: %d",
                __FUNCTION__, urb->status);
    }

    // free up our allocated buffer 
    usb_free_coherent(urb->dev, urb->transfer_buffer_length,
                      urb->transfer_buffer, urb->transfer_dma);
}

// the main function in the method is to write the data to the device, in this case we are going to send 
// instructions that the device can understand and do something with the instructions
static ssize_t usb_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    printk("Writing message in the file created by the driver!\n"); // printing that we are writing information to the device in the file
    struct usb_skel *dev;
    int retval = 0;
    struct urb *urb = NULL;
    char *buf = NULL;

    dev = (struct usb_skel *)file->private_data;

    // if there is something to write
    if (count == 0)
        goto exit;

    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb) {
        retval = -ENOMEM;
        goto error;
    }

    // buffer for the data 
    buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
    if (!buf) {
        retval = -ENOMEM;
        goto error;
    }
    if (copy_from_user(buf, user_buffer, count)) {
        retval = -EFAULT;
        goto error;
    }
    usb_fill_bulk_urb(urb, dev->udev,
                      usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
                      buf, count, usb_write_bulk_callback, dev);
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    // The data can be submitted to the USB core to be transmitted to the device 
    retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval) {
        pr_err("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
        goto error;
    }

    // release our reference to this urb, the USB core will eventually free it entirely 
    usb_free_urb(urb);

exit:
    return count;

error:
    usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
    usb_free_urb(urb);
    kfree(buf);
    return retval;
}

// these are the operations to the file to control the driver
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = usb_read,
    .write = usb_write,
    .open = usb_open,
};

// it is going to be created a file called RoboticFinger and a number, and this file is going to be used 
// for the communication to the usb driver
static struct usb_class_driver skel_class = {
    .name = "usb/RoboticFinger%d",
    .fops = &fops,
    .minor_base = USB_SKEL_MINOR_BASE,
};

// this is the function called when the USB is plugged in the computer
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    printk("ArduinoDriver - Hice probe al Driver \n");
    struct usb_skel *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
    int i;
    int retval = -ENOMEM;

	

    // allocate memory for our device state and initialize it
    dev = kzalloc(sizeof(struct usb_skel), GFP_KERNEL);
    if (!dev) {
        pr_err("Out of memory");
        goto error;
    }
    kref_init(&dev->kref);

    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;

    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        if (!dev->bulk_in_endpointAddr &&
            (endpoint->bEndpointAddress & USB_DIR_IN) &&
            ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                      == USB_ENDPOINT_XFER_BULK)) {
            // we found a bulk in endpoint to manage the size of the memory the finger would need
            buffer_size = endpoint->wMaxPacketSize;
            dev->bulk_in_size = buffer_size;
            dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
            if (!dev->bulk_in_buffer) {
                pr_err("Could not allocate bulk_in_buffer");
                goto error;
            }
        }

        if (!dev->bulk_out_endpointAddr &&
            !(endpoint->bEndpointAddress & USB_DIR_IN) &&
            ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                      == USB_ENDPOINT_XFER_BULK)) {
            // we found a bulk out endpoint 
            dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
        }
    }
    if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
        pr_err("Could not find both bulk-in and bulk-out endpoints");
        goto error;
    }

    // save our data pointer in this interface device 
    usb_set_intfdata(interface, dev);

    // we can register the device now, as it is ready 
    retval = usb_register_dev(interface, &skel_class);
    if (retval) {
        // something prevented us from registering this driver 
        pr_err("Not able to get a minor for this device.");
        usb_set_intfdata(interface, NULL);
        goto error;
    }

    // let the user know what node this device is now attached to 
    dev_info(&interface->dev, "USB Skeleton device now attached to USBSkel-%d", interface->minor);
    return 0;

error:
    if (dev)
        kref_put(&dev->kref, usb_delete);
    return retval;
}

// The disconnect function is called when the driver should no longer control the device for some
// reason and can do clean-up 
static void usb_disconnect(struct usb_interface *interface)
{
    struct usb_skel *dev;
    int minor = interface->minor;

    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    usb_deregister_dev(interface, &skel_class);

    kref_put(&dev->kref, usb_delete);

    dev_info(&interface->dev, "USB Skeleton #%d now disconnected", minor);

	printk(KERN_INFO KBUILD_MODNAME " Me desconecté del Driver \n");

}

// This structure must be filled out by the USB driver and consists of a number of function
// callbacks and variables that describe the USB driver to the USB core code  
static struct usb_driver driver = {
    .name = "ArduinoDriver", // Name of the driver
    .id_table = tableOfDevices,       // list of all of the different kinds of USB devices this driver can accept
    .probe = usb_probe,
    .disconnect = usb_disconnect,     // This function is called by the USB core when the driver is being unloaded from the USB core.
};

// This function is in charge of registering the usb_driver
static int __init usb_init(void)
{
	printk(KERN_INFO KBUILD_MODNAME " Me pusieron el Driver \n");
    int result;

    // register this driver with the USB subsystem 
    result = usb_register(&driver);
    if (result)
        pr_err("usb_register failed. Error number %d", result);

    return result;
}

// This function is in charge of unregistering the usb_driver
static void __exit usb_exit(void)
{
	printk(KERN_INFO KBUILD_MODNAME " Me quitaron el Driver \n");
    usb_deregister(&driver);
}

module_init(usb_init); // When the module is loaded, calls the function usb_init
module_exit(usb_exit); // When the module is unloaded, calls the function usb_exit

MODULE_LICENSE("GPL");