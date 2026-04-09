#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "objects"
#define CASE_NAME "jsmx/object-symbol-key-parity"

static int
expect_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_copy_json(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to measure emitted JSON");
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected %zu JSON bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_copy_json(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to copy emitted JSON");
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"emitted JSON did not match expected output");
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t description;
	jsval_t symbol_a;
	jsval_t symbol_b;
	jsval_t src;
	jsval_t clone;
	jsval_t copied;
	jsval_t value;
	jsval_t key;
	int deleted = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"token", 5, &description) == 0,
			SUITE, CASE_NAME, "failed to allocate symbol description");
	GENERATED_TEST_ASSERT(jsval_symbol_new(&region, 1, description,
			&symbol_a) == 0 && jsval_symbol_new(&region, 1, description,
			&symbol_b) == 0, SUITE, CASE_NAME,
			"failed to allocate symbol keys");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &src) == 0,
			SUITE, CASE_NAME, "failed to allocate source object");
	GENERATED_TEST_ASSERT(jsval_object_set_key(&region, src, symbol_a,
			jsval_number(7.0)) == 0 && jsval_object_set_utf8(&region, src,
			(const uint8_t *)"plain", 5, jsval_bool(1)) == 0
			&& jsval_object_set_key(&region, src, symbol_b,
			jsval_number(9.0)) == 0, SUITE, CASE_NAME,
			"failed to initialize symbol-keyed source object");
	GENERATED_TEST_ASSERT(jsval_object_clone_own(&region, src, 3, &clone) == 0,
			SUITE, CASE_NAME, "failed to clone symbol-keyed source object");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 3, &copied) == 0
			&& jsval_object_copy_own(&region, copied, src) == 0,
			SUITE, CASE_NAME, "failed to copy symbol-keyed source object");

	GENERATED_TEST_ASSERT(jsval_object_get_key(&region, clone, symbol_a,
			&value) == 0 && value.kind == JSVAL_KIND_NUMBER
			&& value.as.number == 7.0, SUITE, CASE_NAME,
			"expected clone to preserve first symbol-keyed property");
	GENERATED_TEST_ASSERT(jsval_object_get_key(&region, copied, symbol_b,
			&value) == 0 && value.kind == JSVAL_KIND_NUMBER
			&& value.as.number == 9.0, SUITE, CASE_NAME,
			"expected copy to preserve second symbol-keyed property");
	GENERATED_TEST_ASSERT(expect_json(&region, src, "{\"plain\":true}")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected JSON emission to omit symbol-keyed properties");

	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, src, 0, &key) == 0
			&& jsval_strict_eq(&region, key, symbol_a) == 1,
			SUITE, CASE_NAME,
			"expected first own key to be the first inserted symbol");
	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, src, 1, &key) == 0
			&& key.kind == JSVAL_KIND_STRING, SUITE, CASE_NAME,
			"expected middle own key to stay the string key");
	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, src, 2, &key) == 0
			&& jsval_strict_eq(&region, key, symbol_b) == 1,
			SUITE, CASE_NAME,
			"expected third own key to be the second inserted symbol");

	GENERATED_TEST_ASSERT(jsval_object_delete_key(&region, src, symbol_a,
			&deleted) == 0 && deleted == 1, SUITE, CASE_NAME,
			"failed to delete first symbol-keyed property");
	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, src, 0, &key) == 0
			&& key.kind == JSVAL_KIND_STRING, SUITE, CASE_NAME,
			"expected string key to compact into the first slot after delete");
	GENERATED_TEST_ASSERT(jsval_object_key_at(&region, src, 1, &key) == 0
			&& jsval_strict_eq(&region, key, symbol_b) == 1,
			SUITE, CASE_NAME,
			"expected remaining symbol key to compact left after delete");

	return generated_test_pass(SUITE, CASE_NAME);
}
