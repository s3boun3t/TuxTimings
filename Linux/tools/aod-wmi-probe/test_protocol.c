// SPDX-License-Identifier: GPL-2.0
#include <assert.h>
#include <stdio.h>

#include "aod_probe_protocol.h"

struct fixture {
	union acpi_object version, ids, reply;
	union acpi_object entries[193];
	unsigned char bytes[200];
	unsigned int calls[3], used, releases;
	unsigned int failing_method;
};

static void init_fixture(struct fixture *f)
{
	unsigned int i;

	memset(f, 0, sizeof(*f));
	f->version.type = ACPI_TYPE_INTEGER;
	f->version.integer.value = 5;
	f->ids.type = ACPI_TYPE_PACKAGE;
	f->ids.package.count = 193;
	f->ids.package.elements = f->entries;
	for (i = 0; i < 129; i++)
		f->entries[i].type = ACPI_TYPE_INTEGER;
	for (i = 129; i < 193; i++) {
		f->entries[i].type = ACPI_TYPE_STRING;
		f->entries[i].string.pointer = "";
	}
	f->entries[0].integer.value = 2;
	f->entries[1].integer.value = 0x10001;
	f->entries[2].integer.value = 0x10002;
	f->entries[130].string.pointer = "Get memory voltages";
	f->entries[130].string.length = 19;
	f->reply.type = ACPI_TYPE_BUFFER;
	f->reply.buffer.length = sizeof(f->bytes);
	f->reply.buffer.pointer = f->bytes;
	f->bytes[0] = 0x9e;
	f->bytes[0x0c] = 5;
	/* Adjacent packed 16-bit fields, deliberately unsuitable for u32 reads. */
	f->bytes[0x1b] = 0x46; f->bytes[0x1c] = 0x05; /* 1350 */
	f->bytes[0x1d] = 0xa3; f->bytes[0x1e] = 0x02; /* 675 */
	f->bytes[0x1f] = 0xc4; f->bytes[0x20] = 0x09; /* 2500 */
}

static struct fixture *active;

static int mock_call(void *context, unsigned int method, const unsigned char *args,
		     size_t length, union acpi_object **result)
{
	struct fixture *f = context;
	const unsigned char expected[8] = { 2, 0, 1, 0, 0, 0, 0, 0 };
	const unsigned char zero[4] = { 0 };

	assert(f->used < 3);
	f->calls[f->used++] = method;
	/* There is no test route that permits Init, setters or arbitrary arguments. */
	if (method == 1 || method == 3) {
		assert(length == sizeof(zero) && !memcmp(args, zero, sizeof(zero)));
	} else {
		assert(method == 5 && length == sizeof(expected));
		assert(!memcmp(args, expected, sizeof(expected)));
	}
	if (method == f->failing_method)
		return -EIO;
	*result = method == 1 ? &f->version : method == 3 ? &f->ids : &f->reply;
	return 0;
}

static void mock_release(union acpi_object *obj)
{
	if (obj) {
		assert(obj == &active->version || obj == &active->ids || obj == &active->reply);
		active->releases++;
	}
}

static struct aod_probe_snapshot collect(struct fixture *f, bool request)
{
	const struct aod_probe_transport transport = {
		.call = mock_call, .release = mock_release, .context = f,
	};
	struct aod_probe_snapshot s;

	active = f;
	aod_probe_collect(&transport, request, &s);
	return s;
}

static void test_capability_validation(void)
{
	struct fixture f;
	unsigned int v;

	init_fixture(&f);
	assert(!aod_probe_version(&f.version, &v) && v == 5);
	assert(aod_probe_version(NULL, &v) == -EPROTO);
	f.version.integer.value = 0x100000005ULL;
	assert(aod_probe_version(&f.version, &v) == -EPROTO);
	assert(aod_probe_version(&f.reply, &v) == -EPROTO);
	assert(!aod_probe_memory_getter(&f.ids));
	assert(aod_probe_memory_getter(NULL) == -EPROTO);
	f.ids.package.count = 192;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	f.ids.package.count = 194;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	f.ids.package.count = 193;
	f.entries[0].integer.value = 65;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	f.entries[0].integer.value = 0;
	assert(aod_probe_memory_getter(&f.ids) == -EOPNOTSUPP);
	f.entries[0].integer.value = 2;
	f.entries[2].integer.value = 0x30001; /* Setter must never substitute for getter. */
	assert(aod_probe_memory_getter(&f.ids) == -EOPNOTSUPP);
	f.entries[2].integer.value = 0x10002;
	f.entries[130].string.pointer = "Set memory voltages";
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	f.entries[130].string.pointer = "Get memory voltages";
	f.entries[130].string.length = 18;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	f.entries[130].string.length = 19;
	f.entries[130].string.pointer = NULL;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	init_fixture(&f);
	f.entries[1] = f.entries[2];
	f.entries[129] = f.entries[130];
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO); /* Ambiguous duplicate. */
	init_fixture(&f);
	f.entries[66].type = ACPI_TYPE_BUFFER;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
	init_fixture(&f);
	f.entries[2].integer.value = 0x100010002ULL;
	assert(aod_probe_memory_getter(&f.ids) == -EPROTO);
}

static void test_reply_validation(void)
{
	struct fixture f;
	struct aod_probe_reply r;
	unsigned int i;

	init_fixture(&f);
	assert(!aod_probe_decode_reply(&f.reply, &r));
	assert(r.vdio_raw == 1350 && r.vmtt_raw == 675 && r.vmpp_raw == 2500);
	for (i = 0; i < 200; i++) {
		f.reply.buffer.length = i;
		assert(aod_probe_decode_reply(&f.reply, &r) == -EPROTO);
		assert(!r.bytes && !r.vdio_raw);
	}
	f.reply.buffer.length = 201; /* Unknown layout: diagnostic rejects extensions. */
	assert(aod_probe_decode_reply(&f.reply, &r) == -EPROTO);
	f.reply.buffer.length = 200;
	f.bytes[2] = 1;
	assert(aod_probe_decode_reply(&f.reply, &r) == -EREMOTEIO);
	assert(r.status == 1 && !r.vdio_raw);
	f.bytes[2] = 0; f.bytes[5] = 0x80;
	assert(aod_probe_decode_reply(&f.reply, &r) == -EREMOTEIO);
	assert(r.status == 0x80000000U);
	f.bytes[5] = 0; f.bytes[0x0c] = 4;
	assert(aod_probe_decode_reply(&f.reply, &r) == -EPROTO);
	assert(!r.vdio_raw);
	memset(f.bytes, 0, sizeof(f.bytes));
	assert(aod_probe_decode_reply(&f.reply, &r) == -EPROTO);
	f.bytes[0x0c] = 5;
	assert(!aod_probe_decode_reply(&f.reply, &r));
	assert(!r.vdio_raw && !r.vmtt_raw && !r.vmpp_raw); /* Empty stays raw zero. */
	f.reply.buffer.pointer = NULL;
	assert(aod_probe_decode_reply(&f.reply, &r) == -EPROTO);
}

static void test_dispatch_guards(void)
{
	struct fixture f;
	struct aod_probe_snapshot s;
	unsigned int i;

	init_fixture(&f);
	s = collect(&f, false);
	assert(f.used == 2 && f.calls[0] == 1 && f.calls[1] == 3);
	assert(!s.query_attempted && s.query_ret == -ENODATA && f.releases == 2);
	init_fixture(&f);
	s = collect(&f, true);
	assert(f.used == 3 && f.calls[2] == 5 && f.releases == 3);
	assert(s.query_attempted && !s.query_ret && s.reply.vdio_raw == 1350);
	for (i = 0; i < 8; i++) {
		if (i == 5)
			continue;
		init_fixture(&f);
		f.version.integer.value = i;
		s = collect(&f, true);
		assert(f.used == 1 && !s.query_attempted && f.releases == 1);
	}
	init_fixture(&f);
	f.entries[2].integer.value = 0x30001;
	s = collect(&f, true);
	assert(f.used == 2 && !s.query_attempted && s.capabilities_ret == -EOPNOTSUPP);
	init_fixture(&f);
	f.entries[130].string.pointer = "Set memory voltages";
	s = collect(&f, true);
	assert(f.used == 2 && !s.query_attempted && s.capabilities_ret == -EPROTO);
	for (i = 1; i <= 5; i += 2) {
		init_fixture(&f);
		f.failing_method = i;
		s = collect(&f, true);
		assert(f.used == (i + 1) / 2 && f.releases == (i - 1) / 2);
		assert(i == 5 ? s.query_ret == -EIO : !s.query_attempted);
	}
	init_fixture(&f);
	f.bytes[2] = 1;
	s = collect(&f, true);
	assert(f.used == 3 && s.query_ret == -EREMOTEIO && !s.reply.vdio_raw);
}

int main(void)
{
	test_capability_validation();
	test_reply_validation();
	test_dispatch_guards();
	puts("AOD WMI protocol tests passed; no hardware access");
	return 0;
}
