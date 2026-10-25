// SPDX-License-Identifier: GPL-2.0
/* Optional, manually loaded diagnostic. Does not feed application sensors. */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/wmi.h>

#include "aod_probe_protocol.h"

static bool read_voltages;
module_param(read_voltages, bool, 0400);
MODULE_PARM_DESC(read_voltages, "Request one advertised memory-voltage getter at bind (default false)");

static int aod_call(void *context, unsigned int method, const unsigned char *args,
		    size_t length, union acpi_object **result)
{
	struct acpi_buffer in = { length, (void *)args };
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;

	/* Native ACPI objects preserve package types on kernels including 6.8. */
	status = wmidev_evaluate_method(context, 0, method, &in, &out);
	if (ACPI_FAILURE(status)) {
		kfree(out.pointer);
		return -EIO;
	}
	*result = out.pointer;
	return 0;
}

static void aod_release(union acpi_object *obj)
{
	kfree(obj);
}

static ssize_t report_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct aod_probe_snapshot *s = dev_get_drvdata(dev);
	ssize_t n;

	n = sysfs_emit(buf,
		"guid=" AOD_PROBE_GUID "\n"
		"version_ret=%d\nversion=%u\ncapabilities_ret=%d\n"
		"query_requested=%u\nquery_attempted=%u\nquery_ret=%d\n",
		s->version_ret, s->version, s->capabilities_ret,
		s->query_requested, s->query_attempted, s->query_ret);
	if (s->reply.bytes)
		n += sysfs_emit_at(buf, n,
			"reply_bytes=%u\nsmi_command_raw=0x%04x\n"
			"firmware_status_raw=0x%08x\nexchange_version=%u\n"
			"command_mask_raw=0x%08x\n",
			s->reply.bytes, s->reply.smi_command, s->reply.status,
			s->reply.version, s->reply.command_mask);
	if (s->query_attempted && !s->query_ret)
		n += sysfs_emit_at(buf, n,
			"vdio_raw=%u\nvmtt_raw=%u\nvmpp_raw=%u\n"
			"units=unverified\nsource=firmware_snapshot\n",
			s->reply.vdio_raw, s->reply.vmtt_raw, s->reply.vmpp_raw);
	return n;
}
static DEVICE_ATTR(report, 0400, report_show, NULL);

static int aod_probe(struct wmi_device *wdev, const void *context)
{
	struct aod_probe_snapshot *s;
	const struct aod_probe_transport transport = {
		.call = aod_call,
		.release = aod_release,
		.context = wdev,
	};

	if (wmidev_instance_count(wdev) != 1)
		return -EOPNOTSUPP;
	s = devm_kzalloc(&wdev->dev, sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	aod_probe_collect(&transport, read_voltages, s);
	dev_set_drvdata(&wdev->dev, s);
	return device_create_file(&wdev->dev, &dev_attr_report);
}

static void aod_remove(struct wmi_device *wdev)
{
	device_remove_file(&wdev->dev, &dev_attr_report);
}

static const struct wmi_device_id aod_ids[] = {
	{ AOD_PROBE_GUID, NULL },
	{ }
};
MODULE_DEVICE_TABLE(wmi, aod_ids);

static struct wmi_driver aod_driver = {
	.driver = { .name = "tuxtimings_aod_probe" },
	.id_table = aod_ids,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
	.no_singleton = true,
#endif
	.probe = aod_probe,
	.remove = aod_remove,
};
module_wmi_driver(aod_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TuxTimings legacy AOD WMI snapshot diagnostic");
MODULE_VERSION("0.1");
