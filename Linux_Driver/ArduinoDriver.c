#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <asm/uaccess.h>

// Define el ID del fabricante y del producto de Arduino
#define ARDUINO_VENDOR_ID    0x2341
#define ARDUINO_PRODUCT_ID   0x0043

// Tabla con los dispositivos que pueden trabajar con este driver
static struct usb_device_id tableOfDevices [] = {
    {USB_DEVICE(ARDUINO_VENDOR_ID, ARDUINO_PRODUCT_ID)},
    {}
};

// Definir la tabla de dispositivos
MODULE_DEVICE_TABLE(usb, tableOfDevices);

// Obtener un rango menor para los dispositivos del mantenedor USB
#define USB_SKEL_MINOR_BASE  192

// Estructura para mantener toda la información específica del dispositivo
struct usb_skel {
    struct usb_device *udev;              // El dispositivo USB para este dispositivo
    struct usb_interface *interface;      // La interfaz para este dispositivo
    unsigned char *bulk_in_buffer;        // El buffer para recibir datos
    size_t bulk_in_size;                  // El tamaño del buffer de recepción
    __u8 bulk_in_endpointAddr;            // La dirección del endpoint de entrada para recibir información del dispositivo al host.
    __u8 bulk_out_endpointAddr;           // La dirección del endpoint de salida para enviar información del host al dispositivo.
    struct kref kref;
};

#define to_skel_dev(d) container_of(d, struct usb_skel, kref)

// Definir la estructura del driver USB que se va a usar
static struct usb_driver driver;

/**
 * usb_delete - Eliminar la referencia al dispositivo USB y liberar memoria
 * @kref: Referencia al contador del dispositivo USB
 *
 * Esta función decrementa el contador de referencia y libera la memoria asociada
 * con el dispositivo USB cuando el contador llega a cero.
 */
static void usb_delete(struct kref *kref)
{
    struct usb_skel *dev = to_skel_dev(kref);

    usb_put_dev(dev->udev);
    kfree(dev->bulk_in_buffer);
    kfree(dev);
}

/**
 * usb_open - Abrir el driver del dispositivo cuando va a ser usado
 * @inode: Nodo de índice
 * @file: Estructura de archivo
 * 
 * Esta función abre el dispositivo USB y lo prepara para operaciones de lectura/escritura.
 * 
 * Retorna 0 en caso de éxito o un código de error en caso de fallo.
 */
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
    
    // Incrementar el contador de uso para el dispositivo
    kref_get(&dev->kref);

    // Guardar nuestro objeto en la estructura privada del archivo
    file->private_data = dev;

exit:
    return retval;
}

/**
 * usb_read - Leer información del dispositivo USB
 * @file: Estructura de archivo
 * @buffer: Buffer del usuario para recibir datos
 * @count: Número de bytes a leer
 * @ppos: Posición del archivo
 * 
 * Esta función lee datos desde el dispositivo USB y los copia al espacio de usuario.
 * 
 * Retorna el número de bytes leídos o un código de error en caso de fallo.
 */
static ssize_t usb_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    struct usb_skel *dev;
    int retval = 0;
    int actual_length;

    dev = (struct usb_skel *)file->private_data;
    
    // Realizar una lectura bloqueante para obtener datos del dispositivo 
    retval = usb_bulk_msg(dev->udev,
                          usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                          dev->bulk_in_buffer,
                          min(dev->bulk_in_size, count),
                          &actual_length, HZ*10);

    // Si la lectura fue exitosa, copiar los datos al espacio de usuario.
    if (!retval) {
        if (copy_to_user(buffer, dev->bulk_in_buffer, actual_length))
            retval = -EFAULT;
        else
            retval = actual_length;
    }

    return retval;
}

/**
 * usb_write_bulk_callback - Callback para escritura masiva USB
 * @urb: URB (USB Request Block)
 * 
 * Esta función es llamada cuando una operación de escritura USB se completa.
 */ 
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

    // Liberar buffer asignado
    usb_free_coherent(urb->dev, urb->transfer_buffer_length,
                      urb->transfer_buffer, urb->transfer_dma);
}

/**
 * usb_write - Escribir datos en el dispositivo USB
 * @file: Estructura de archivo
 * @user_buffer: Buffer del usuario con los datos a escribir
 * @count: Número de bytes a escribir
 * @ppos: Posición del archivo
 * 
 * Esta función escribe datos en el dispositivo USB enviando instrucciones.
 * 
 * Retorna el número de bytes escritos o un código de error en caso de fallo.
 */
static ssize_t usb_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    printk("Writing message in the file created by the driver!\n"); // printing that we are writing information to the device in the file
    struct usb_skel *dev;
    int retval = 0;
    struct urb *urb = NULL;
    char *buf = NULL;

    dev = (struct usb_skel *)file->private_data;

    // Si no hay nada que escribir
    if (count == 0)
        goto exit;

    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb) {
        retval = -ENOMEM;
        goto error;
    }

    // Buffer para los datos
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

    // Los datos pueden ser enviados al núcleo USB para ser transmitidos al dispositivo
    retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval) {
        pr_err("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
        goto error;
    }

    // Liberar nuestra referencia a este URB, el núcleo USB eventualmente lo liberará completamente
    usb_free_urb(urb);

exit:
    return count;

error:
    usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
    usb_free_urb(urb);
    kfree(buf);
    return retval;
}

// Operaciones de archivo para controlar el driver
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = usb_read,
    .write = usb_write,
    .open = usb_open,
};

// Se va a crear un archivo llamado ArduinoDriver y un número, este archivo será usado para la comunicación con el driver USB
static struct usb_class_driver skel_class = {
    .name = "ArduinoDriver%d",
    .fops = &fops,
    .minor_base = USB_SKEL_MINOR_BASE,
};

/**
 * usb_probe - Función llamada cuando el USB se conecta a la computadora
 * @interface: Interfaz del dispositivo USB
 * @id: ID del dispositivo USB
 * 
 * Esta función inicializa el dispositivo USB y lo prepara para su uso.
 * 
 * Retorna 0 en caso de éxito o un código de error en caso de fallo.
 */
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    printk("ArduinoDriver - Hice probe al Driver \n");
    struct usb_skel *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
    int i;
    int retval = -ENOMEM;

    // Asignar memoria para el estado del dispositivo e inicializarlo
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

    // Guardar puntero de datos en este dispositivo de interfaz
    usb_set_intfdata(interface, dev);

    // Registrar el dispositivo ahora que está listo
    retval = usb_register_dev(interface, &skel_class);
    if (retval) {
        // Algo nos impidió registrar este driver
        pr_err("Not able to get a minor for this device.");
        usb_set_intfdata(interface, NULL);
        goto error;
    }

    // Informar al usuario a qué nodo está conectado este dispositivo
    dev_info(&interface->dev, "USB Skeleton device now attached to USBSkel-%d", interface->minor);
    return 0;

error:
    if (dev)
        kref_put(&dev->kref, usb_delete);
    return retval;
}

/**
 * usb_disconnect - Función llamada cuando el driver ya no debe controlar el dispositivo
 * @interface: Interfaz del dispositivo USB
 * 
 * Esta función limpia los recursos asociados con el dispositivo USB cuando se desconecta.
 */
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

// Esta estructura debe ser completada por el driver USB 
// Y consiste en varias funciones callback y variables que describen el driver USB al núcleo USB
static struct usb_driver driver = {
    .name = "ArduinoDriver",        // Nombre del driver
    .id_table = tableOfDevices,     // Lista de todos los diferentes tipos de dispositivos USB que este driver puede aceptar
    .probe = usb_probe,
    .disconnect = usb_disconnect,   // This function is called by the USB core when the driver is being unloaded from the USB core.
};

/**
 * usb_init - Función encargada de registrar el driver USB
 * 
 * Esta función registra el driver USB con el subsistema USB.
 * 
 * Retorna 0 en caso de éxito o un código de error en caso de fallo.
 */
static int __init usb_init(void)
{
    printk(KERN_INFO KBUILD_MODNAME " Me pusieron el Driver \n");
    int result;

    // Registrar este driver con el subsistema USB
    result = usb_register(&driver);
    if (result)
        pr_err("usb_register failed. Error number %d", result);

    return result;
}

/**
 * usb_exit - Función encargada de anular el registro del driver USB
 * 
 * Esta función quita el registro del driver USB del subsistema USB.
 */
static void __exit usb_exit(void)
{
    printk(KERN_INFO KBUILD_MODNAME " Me quitaron el Driver \n");
    usb_deregister(&driver);
}

module_init(usb_init); // Cuando el módulo se carga, llama a la función usb_init
module_exit(usb_exit); // Cuando el módulo se descarga, llama a la función usb_exit

MODULE_LICENSE("GPL");
