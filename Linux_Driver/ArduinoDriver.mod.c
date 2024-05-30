#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2ec9681e, "usb_alloc_urb" },
	{ 0xb8d6ea64, "usb_alloc_coherent" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x84cd9e22, "usb_submit_urb" },
	{ 0x9fe19262, "usb_free_urb" },
	{ 0x37a0cba, "kfree" },
	{ 0x3dad4972, "usb_bulk_msg" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x74bc459a, "usb_deregister" },
	{ 0xe72db8a, "usb_find_interface" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0x2df40867, "usb_deregister_dev" },
	{ 0xb940ca68, "_dev_info" },
	{ 0xc6aeb0a7, "usb_put_dev" },
	{ 0xb88db70c, "kmalloc_caches" },
	{ 0x4454730e, "kmalloc_trace" },
	{ 0xa37e37e4, "usb_get_dev" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x90c20ca2, "usb_register_dev" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x122c3a7e, "_printk" },
	{ 0x4523577f, "usb_register_driver" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x2421d970, "usb_free_coherent" },
	{ 0x5b5bb31b, "__dynamic_dev_dbg" },
	{ 0x2fa5cadd, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v2341p0043d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "A2E12FE29B01904EF85384B");
