#include <stdint.h>

#include "bench/generated/bench_util.h"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t acc = jsval_number(0.0);
	jsval_t tmp;
	int32_t result = 0;
	uint32_t i;

	jsval_region_init(&region, storage, sizeof(storage));
	for (i = 0; i < 250000u; i++) {
		if (jsval_add(&region, acc, jsval_number((double)i), &tmp) < 0) {
			return bench_fail_errno("jsval_add");
		}
		if (jsval_multiply(&region, tmp, jsval_number(3.0), &acc) < 0) {
			return bench_fail_errno("jsval_multiply");
		}
		if (jsval_shift_right_unsigned(&region, jsval_number((double)i),
				jsval_number(3.0), &tmp) < 0) {
			return bench_fail_errno("jsval_shift_right_unsigned");
		}
		if (jsval_bitwise_xor(&region, acc, tmp, &acc) < 0) {
			return bench_fail_errno("jsval_bitwise_xor");
		}
	}
	if (jsval_to_int32(&region, acc, &result) < 0) {
		return bench_fail_errno("jsval_to_int32");
	}
	return bench_print_i64((int64_t)result);
}
