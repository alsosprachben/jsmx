#include <stdint.h>

#include "bench/generated/bench_util.h"

int
main(void)
{
	static const uint8_t second_json[] = "[3,4]";
	uint8_t storage[4096];
	uint64_t checksum = 0;
	size_t i;

	for (i = 0; i < 40000u; i++) {
		jsval_region_t region;
		jsval_t first;
		jsval_t second;
		jsval_t out;
		jsval_t value;
		double number = 0.0;
		size_t j;

		jsval_region_init(&region, storage, sizeof(storage));
		if (jsval_array_new(&region, 2, &first) < 0) {
			return bench_fail_errno("jsval_array_new(first)");
		}
		if (jsval_array_push(&region, first, jsval_number((double)(i & 7u))) < 0
				|| jsval_array_push(&region, first, jsval_number(2.0)) < 0) {
			return bench_fail_errno("jsval_array_push(first)");
		}
		if (jsval_json_parse(&region, second_json, sizeof(second_json) - 1, 8,
				&second) < 0) {
			return bench_fail_errno("jsval_json_parse(second)");
		}
		if (jsval_array_new(&region, 6, &out) < 0) {
			return bench_fail_errno("jsval_array_new(out)");
		}
		if (jsval_array_push(&region, out, jsval_number(0.0)) < 0) {
			return bench_fail_errno("jsval_array_push(head)");
		}
		if (bench_append_dense_array(&region, out, first) < 0) {
			return bench_fail_errno("bench_append_dense_array(first)");
		}
		if (bench_append_dense_array(&region, out, second) < 0) {
			return bench_fail_errno("bench_append_dense_array(second)");
		}
		if (jsval_array_push(&region, out, jsval_number(9.0)) < 0) {
			return bench_fail_errno("jsval_array_push(tail)");
		}
		for (j = 0; j < jsval_array_length(&region, out); j++) {
			if (jsval_array_get(&region, out, j, &value) < 0) {
				return bench_fail_errno("jsval_array_get(out)");
			}
			if (jsval_to_number(&region, value, &number) < 0) {
				return bench_fail_errno("jsval_to_number(out)");
			}
			checksum += (uint64_t)number;
		}
	}

	return bench_print_u64(checksum);
}
