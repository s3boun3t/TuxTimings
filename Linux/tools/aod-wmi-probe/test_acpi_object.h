/* SPDX-License-Identifier: GPL-2.0 */
#ifndef TUXTIMINGS_TEST_ACPI_OBJECT_H
#define TUXTIMINGS_TEST_ACPI_OBJECT_H
/* Test double of the public ACPI object fields consumed by the decoder.
 * Never included in a kernel build; no firmware or sysfs access in tests.
 */
#define ACPI_TYPE_INTEGER 1
#define ACPI_TYPE_STRING 2
#define ACPI_TYPE_BUFFER 3
#define ACPI_TYPE_PACKAGE 4
union acpi_object {
	unsigned int type;
	struct { unsigned int type; unsigned long long value; } integer;
	struct { unsigned int type, length; char *pointer; } string;
	struct { unsigned int type, length; unsigned char *pointer; } buffer;
	struct { unsigned int type, count; union acpi_object *elements; } package;
};
#endif
