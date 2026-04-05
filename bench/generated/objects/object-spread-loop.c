#include <stdint.h>

#include "bench/generated/bench_util.h"

int
main(void)
{
	static const uint8_t second_json[] = "{\"drop\":9,\"tail\":10}";
	uint8_t storage[4096];
	uint64_t checksum = 0;
	size_t i;

	for (i = 0; i < 40000u; i++) {
		jsval_region_t region;
		jsval_t first;
		jsval_t second;
		jsval_t out;
		jsval_t items;
		jsval_t value;
		double number = 0.0;

		jsval_region_init(&region, storage, sizeof(storage));
		if (jsval_object_new(&region, 3, &first) < 0) {
			return bench_fail_errno("jsval_object_new(first)");
		}
		if (jsval_array_new(&region, 0, &items) < 0) {
			return bench_fail_errno("jsval_array_new(items)");
		}
		if (jsval_object_set_utf8(&region, first, (const uint8_t *)"keep", 4,
				jsval_bool(1)) < 0
				|| jsval_object_set_utf8(&region, first,
				(const uint8_t *)"drop", 4,
				jsval_number((double)(i & 7u))) < 0
				|| jsval_object_set_utf8(&region, first,
				(const uint8_t *)"items", 5, items) < 0) {
			return bench_fail_errno("jsval_object_set_utf8(first)");
		}
		if (jsval_json_parse(&region, second_json, sizeof(second_json) - 1, 8,
				&second) < 0) {
			return bench_fail_errno("jsval_json_parse(second)");
		}
		if (jsval_object_new(&region, 5, &out) < 0) {
			return bench_fail_errno("jsval_object_new(out)");
		}
		if (jsval_object_set_utf8(&region, out, (const uint8_t *)"head", 4,
				jsval_number((double)(i & 3u))) < 0) {
			return bench_fail_errno("jsval_object_set_utf8(head)");
		}
		if (jsval_object_copy_own(&region, out, first) < 0) {
			return bench_fail_errno("jsval_object_copy_own(first)");
		}
		if (jsval_object_copy_own(&region, out, second) < 0) {
			return bench_fail_errno("jsval_object_copy_own(second)");
		}
		if (jsval_object_set_utf8(&region, out, (const uint8_t *)"tail", 4,
				jsval_number((double)(i & 15u))) < 0) {
			return bench_fail_errno("jsval_object_set_utf8(tail)");
		}
		if (jsval_object_get_utf8(&region, out, (const uint8_t *)"head", 4,
				&value) < 0) {
			return bench_fail_errno("jsval_object_get_utf8(head)");
		}
		if (jsval_to_number(&region, value, &number) < 0) {
			return bench_fail_errno("jsval_to_number(head)");
		}
		checksum += (uint64_t)number;
		if (jsval_object_get_utf8(&region, out, (const uint8_t *)"drop", 4,
				&value) < 0) {
			return bench_fail_errno("jsval_object_get_utf8(drop)");
		}
		if (jsval_to_number(&region, value, &number) < 0) {
			return bench_fail_errno("jsval_to_number(drop)");
		}
		checksum += (uint64_t)number;
		if (jsval_object_get_utf8(&region, out, (const uint8_t *)"tail", 4,
				&value) < 0) {
			return bench_fail_errno("jsval_object_get_utf8(tail)");
		}
		if (jsval_to_number(&region, value, &number) < 0) {
			return bench_fail_errno("jsval_to_number(tail)");
		}
		checksum += (uint64_t)number;
		checksum += jsval_object_size(&region, out);
	}

	return bench_print_u64(checksum);
}
