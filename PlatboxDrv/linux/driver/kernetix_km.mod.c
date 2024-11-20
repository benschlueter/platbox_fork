#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

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



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x6228c21f, "smp_call_function_single" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xbda73460, "apic" },
	{ 0xcedda90a, "class_destroy" },
	{ 0x8f47f9da, "remap_pfn_range" },
	{ 0x37a0cba, "kfree" },
	{ 0xeadcf760, "pcpu_hot" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x122c3a7e, "_printk" },
	{ 0x8a7d1c31, "high_memory" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x43278c02, "const_pcpu_hot" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0x7f08621, "cdev_add" },
	{ 0x98378a1d, "cc_mkdec" },
	{ 0xfddeb056, "efi" },
	{ 0x57bc19d2, "down_write" },
	{ 0xce807a25, "up_write" },
	{ 0xfe3eb422, "device_create" },
	{ 0xf65e496c, "class_create" },
	{ 0x4c9d28b0, "phys_base" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xb8e7ce2c, "__put_user_8" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xf5ffedcb, "pv_ops" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x8b6d2fef, "device_destroy" },
	{ 0xa60fc0d3, "__kmalloc_cache_noprof" },
	{ 0xd4ec10e6, "BUG_func" },
	{ 0x83fb7223, "cdev_init" },
	{ 0xf4729770, "kmalloc_caches" },
	{ 0xd6b1b98d, "cdev_del" },
	{ 0x26742f3f, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "EA1E1003851E612F33F73BC");
