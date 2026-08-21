#ifdef CONFIG_PM_DEVFREQ
#include "hal.h"
#include "inno_input_event.h"
#include "inno_devfreq_gov.h"
#include "innopower.h"

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>

#define idle_fmt(fmt) KBUILD_MODNAME ": " fmt

static int key_report(unsigned int type, unsigned int code, int value)
{
	struct devfreq_info *pinfo = inno_get_devfreq_info();

	if (fh2m_get_pwr_debug_lvl() == PWRD_DBG_INPUT) {
		innopwr_notice("Key changed and code[%u] value[%d] input handle bind[%d]\n", code, value, pinfo->inited);
	}

	if (pinfo->inited) {
		inno_input_kick(pinfo->devfreq);
	}
	return 0;
}

static int mouse_report(unsigned int type, unsigned int code, int value)
{

	struct devfreq_info *pinfo = inno_get_devfreq_info();
	if (fh2m_get_pwr_debug_lvl() == PWRD_DBG_INPUT) {
		innopwr_notice("Mouse changed and code[%u] value[%d] input handle bind[%d]\n", code, value, pinfo->inited);
	}

	if (pinfo->inited) {
		inno_input_kick(pinfo->devfreq);
	}

	return 0;
}

static void inno_input_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	if (type == EV_KEY) {
		key_report(type, code, value);
	} else if (type == EV_REL) {
		mouse_report(type, code, value);
	} else {
		//do nothing
	}
}

static int inno_input_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "inno_input";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	innopwr_info(idle_fmt("Connected device: %s (%s at %s)\n"),
	dev_name(&dev->dev), dev->name ? dev->name : "unknown", dev->phys ? dev->phys : "unknown");

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);

	return error;
}

static void inno_input_disconnect(struct input_handle *handle)
{
	innopwr_info(idle_fmt("Disconnected device: %s\n"), dev_name(&handle->dev->dev));

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id inno_input_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

static struct input_handler inno_input_handler = {
	.event =	inno_input_event,
	.connect =	inno_input_connect,
	.disconnect =	inno_input_disconnect,
	.name =		"inno_input",
	.id_table =	inno_input_ids,
};

int inno_input_init(void)
{
	return input_register_handler(&inno_input_handler);
}

void inno_input_exit(void)
{
	input_unregister_handler(&inno_input_handler);
}

int inno_input_suspend(void)
{
	struct devfreq_info *pinfo = inno_get_devfreq_info();
	bool inited = pinfo->inited;

	if (pinfo->inited) {
		pinfo->inited = false;
	}

	innopwr_dbg("inited[%d %d] suspend done!!!\n", inited, pinfo->inited);

	return 0;
}

int inno_input_resume(void)
{
	struct devfreq_info *pinfo = inno_get_devfreq_info();
	bool inited = pinfo->inited;

	if (!pinfo->inited) {
		pinfo->inited = true;
	}

	innopwr_dbg("inited[%d %d] resume done!!!\n", inited, pinfo->inited);

	return 0;
}
#endif
