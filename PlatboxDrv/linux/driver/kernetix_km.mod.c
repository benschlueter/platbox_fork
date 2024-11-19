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

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x6228c21f, "smp_call_function_single" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xfab8dad1, "class_destroy" },
	{ 0x22dc2d38, "remap_pfn_range" },
	{ 0x37a0cba, "kfree" },
	{ 0x4861f2bd, "pcpu_hot" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x122c3a7e, "_printk" },
	{ 0x8a7d1c31, "high_memory" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x800473f, "__cond_resched" },
	{ 0xf6bcd8f, "cdev_add" },
	{ 0x98378a1d, "cc_mkdec" },
	{ 0xfddeb056, "efi" },
	{ 0x57bc19d2, "down_write" },
	{ 0xce807a25, "up_write" },
	{ 0xe5a9866f, "device_create" },
	{ 0x9864ae7f, "class_create" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xb8e7ce2c, "__put_user_8" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xbdd8dbc, "device_destroy" },
	{ 0x6bb1092b, "kmalloc_trace" },
	{ 0x14c624de, "cdev_init" },
	{ 0x7c0091f3, "kmalloc_caches" },
	{ 0x5a36124c, "cdev_del" },
	{ 0xe1fecaff, "smp_call_function_single_async" },
	{ 0xdc12662d, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "1932627A33A645FF8684C02");
