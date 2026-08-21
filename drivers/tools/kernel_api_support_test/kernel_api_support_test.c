/* This file is used to test the kernel api in cfg_detect.c which is defined in test_item.sh
 * The output file 'ko' will not be installed. */

#include <linux/kernel.h>
#include <linux/module.h>
#include "cfg_detect.c"

MODULE_LICENSE("GPL");

static int __init test_module_init(void) {
	return 0;
}

static void __exit test_module_exit(void) {
	return;
}

module_init(test_module_init);
module_exit(test_module_exit);