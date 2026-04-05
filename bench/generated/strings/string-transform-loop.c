#include <stdint.h>

#include "bench/generated/bench_util.h"

int
main(void)
{
	static const uint8_t base[] = "alpha beta gamma";
	static const uint8_t search[] = "a";
	static const uint8_t replacement[] = "ab";
	static const uint8_t suffix_xy[] = "-xy";
	static const uint8_t suffix_zz[] = "-zz";
	uint8_t storage[4096];
	uint64_t checksum = 0;
	size_t i;

	for (i = 0; i < 30000u; i++) {
		jsval_region_t region;
		jsval_t base_value;
		jsval_t search_value;
		jsval_t replacement_value;
		jsval_t suffix_value;
		jsval_t replaced;
		jsval_t sliced;
		jsval_t concat_args[1];
		jsval_t value;
		jsval_t code;
		jsmethod_error_t error = {0};
		size_t len = 0;

		jsval_region_init(&region, storage, sizeof(storage));
		if (jsval_string_new_utf8(&region, base, sizeof(base) - 1,
				&base_value) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(base)");
		}
		if (jsval_string_new_utf8(&region, search, sizeof(search) - 1,
				&search_value) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(search)");
		}
		if (jsval_string_new_utf8(&region, replacement,
				sizeof(replacement) - 1, &replacement_value) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(replacement)");
		}
		if (jsval_string_new_utf8(&region,
				(i & 1u) == 0 ? suffix_xy : suffix_zz, 3, &suffix_value) < 0) {
			return bench_fail_errno("jsval_string_new_utf8(suffix)");
		}
		if (jsval_method_string_replace_all(&region, base_value, search_value,
				replacement_value, &replaced, &error) < 0) {
			return bench_fail("jsval_method_string_replace_all");
		}
		if (jsval_method_string_slice(&region, replaced, 1,
				jsval_number(1.0), 1, jsval_number(10.0), &sliced,
				&error) < 0) {
			return bench_fail("jsval_method_string_slice");
		}
		concat_args[0] = suffix_value;
		if (jsval_method_string_concat(&region, sliced, 1, concat_args, &value,
				&error) < 0) {
			return bench_fail("jsval_method_string_concat");
		}
		if (jsval_string_copy_utf8(&region, value, NULL, 0, &len) < 0) {
			return bench_fail_errno("jsval_string_copy_utf8");
		}
		if (jsval_method_string_char_code_at(&region, value, 1,
				jsval_number(0.0), &code, &error) < 0) {
			return bench_fail("jsval_method_string_char_code_at(first)");
		}
		checksum += len;
		checksum += (uint64_t)code.as.number;
		if (jsval_method_string_char_code_at(&region, value, 1,
				jsval_number((double)(len - 1)), &code, &error) < 0) {
			return bench_fail("jsval_method_string_char_code_at(last)");
		}
		checksum += (uint64_t)code.as.number;
	}

	return bench_print_u64(checksum);
}
