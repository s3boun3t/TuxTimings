/* SPDX-License-Identifier: GPL-2.0 */
#ifndef TUXTIMINGS_AOD_PROBE_PROTOCOL_H
#define TUXTIMINGS_AOD_PROBE_PROTOCOL_H

#ifdef __KERNEL__
#include <linux/acpi.h>
#include <linux/errno.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "test_acpi_object.h"
#endif

#define AOD_PROBE_GUID "ABBC0F6A-8EA1-11D1-00A0-C90629100000"
#define AOD_PROBE_VERSION 5U
#define AOD_PROBE_REPLY_SIZE 200U
#define AOD_PROBE_OIDS 64U

struct aod_probe_reply {
	unsigned int bytes;
	unsigned int smi_command;
	unsigned int status;
	unsigned int version;
	unsigned int command_mask;
	unsigned int vdio_raw;
	unsigned int vmtt_raw;
	unsigned int vmpp_raw;
};

struct aod_probe_snapshot {
	unsigned int version;
	int version_ret;
	int capabilities_ret;
	bool query_requested;
	bool query_attempted;
	int query_ret;
	struct aod_probe_reply reply;
};

/* The transport returns one allocated ACPI object on success, freed by release. */
struct aod_probe_transport {
	int (*call)(void *context, unsigned int method, const unsigned char *args,
		    size_t length, union acpi_object **result);
	void (*release)(union acpi_object *result);
	void *context;
};

static inline unsigned int aod_probe_le16(const unsigned char *p)
{
	return p[0] | ((unsigned int)p[1] << 8);
}

static inline unsigned int aod_probe_le32(const unsigned char *p)
{
	return aod_probe_le16(p) | (aod_probe_le16(p + 2) << 16);
}

static inline int aod_probe_version(const union acpi_object *obj,
				  unsigned int *version)
{
	if (!obj || obj->type != ACPI_TYPE_INTEGER ||
	    obj->integer.value > 0xffffffffULL)
		return -EPROTO;
	*version = obj->integer.value;
	return 0;
}

/* Match the advertised ID AND name; never derive a command from a name alone. */
static inline int aod_probe_memory_getter(const union acpi_object *obj)
{
	static const char name[] = "Get memory voltages";
	const union acpi_object *e;
	unsigned int count, i, matches = 0;

	if (!obj || obj->type != ACPI_TYPE_PACKAGE ||
	    obj->package.count != 1 + 3 * AOD_PROBE_OIDS ||
	    !obj->package.elements)
		return -EPROTO;
	e = obj->package.elements;
	if (e[0].type != ACPI_TYPE_INTEGER ||
	    e[0].integer.value > AOD_PROBE_OIDS)
		return -EPROTO;
	count = e[0].integer.value;
	for (i = 0; i < count; i++) {
		const union acpi_object *id = &e[1 + i];
		const union acpi_object *property = &e[1 + AOD_PROBE_OIDS + i];
		const union acpi_object *label = &e[1 + 2 * AOD_PROBE_OIDS + i];

		if (id->type != ACPI_TYPE_INTEGER || id->integer.value > 0xffffffffULL ||
		    property->type != ACPI_TYPE_INTEGER ||
		    property->integer.value > 0xffffffffULL ||
		    label->type != ACPI_TYPE_STRING || !label->string.pointer)
			return -EPROTO;
		if (id->integer.value != 0x00010002)
			continue;
		if (++matches > 1 || label->string.length != sizeof(name) - 1 ||
		    memcmp(label->string.pointer, name, sizeof(name) - 1))
			return -EPROTO;
	}
	return matches == 1 ? 0 : -EOPNOTSUPP;
}

/* Raw firmware fields only. Neither units nor sensor provenance are inferred. */
static inline int aod_probe_decode_reply(const union acpi_object *obj,
					struct aod_probe_reply *reply)
{
	const unsigned char *p;

	memset(reply, 0, sizeof(*reply));
	if (!obj || obj->type != ACPI_TYPE_BUFFER ||
	    obj->buffer.length != AOD_PROBE_REPLY_SIZE || !obj->buffer.pointer)
		return -EPROTO;
	p = obj->buffer.pointer;
	reply->bytes = obj->buffer.length;
	reply->smi_command = aod_probe_le16(p);
	reply->status = aod_probe_le32(p + 2);
	reply->version = aod_probe_le32(p + 0x0c);
	reply->command_mask = aod_probe_le32(p + 0x10);
	if (reply->status)
		return -EREMOTEIO;
	if (reply->version != AOD_PROBE_VERSION)
		return -EPROTO;
	reply->vdio_raw = aod_probe_le16(p + 0x1b);
	reply->vmtt_raw = aod_probe_le16(p + 0x1d);
	reply->vmpp_raw = aod_probe_le16(p + 0x1f);
	return 0;
}

/* A single snapshot; no Init, setter, retry, polling, or caller-supplied command. */
static inline void aod_probe_collect(const struct aod_probe_transport *transport,
				     bool request, struct aod_probe_snapshot *s)
{
	const unsigned char dummy[4] = { 0 };
	const unsigned char get_memory[8] = { 0x02, 0x00, 0x01, 0x00, 0, 0, 0, 0 };
	union acpi_object *obj = NULL;

	memset(s, 0, sizeof(*s));
	s->query_requested = request;
	s->capabilities_ret = -ENODATA;
	s->query_ret = -ENODATA;
	s->version_ret = transport->call(transport->context, 1, dummy, sizeof(dummy), &obj);
	if (!s->version_ret)
		s->version_ret = aod_probe_version(obj, &s->version);
	transport->release(obj);
	if (s->version_ret || s->version != AOD_PROBE_VERSION)
		return;

	obj = NULL;
	s->capabilities_ret = transport->call(transport->context, 3, dummy, sizeof(dummy), &obj);
	if (!s->capabilities_ret)
		s->capabilities_ret = aod_probe_memory_getter(obj);
	transport->release(obj);
	if (s->capabilities_ret || !request)
		return;

	obj = NULL;
	s->query_attempted = true;
	s->query_ret = transport->call(transport->context, 5, get_memory,
				       sizeof(get_memory), &obj);
	if (!s->query_ret)
		s->query_ret = aod_probe_decode_reply(obj, &s->reply);
	transport->release(obj);
}
#endif
