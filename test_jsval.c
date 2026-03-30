#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jsval.h"

static void assert_string(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(value.kind == JSVAL_KIND_STRING);
	assert(jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) == 0);
	assert(actual_len == expected_len);
	assert(jsval_string_copy_utf8(region, value, buf, actual_len, NULL) == 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void assert_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(jsval_copy_json(region, value, NULL, 0, &actual_len) == 0);
	assert(actual_len == expected_len);
	assert(jsval_copy_json(region, value, buf, actual_len, NULL) == 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void assert_positive_zero(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == 0.0);
	assert(signbit(value.as.number) == 0);
}

static void assert_negative_zero(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == 0.0);
	assert(signbit(value.as.number) != 0);
}

static void assert_nan_value(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(isnan(value.as.number));
}

static void assert_number_value(jsval_t value, double expected)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == expected);
}

static jsval_t logical_and(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? right : left;
}

static jsval_t logical_or(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? left : right;
}

static void test_native_storage()
{
	uint8_t storage[16384];
	jsval_region_t region;
	const jsval_pages_t *pages;
	jsval_t object;
	jsval_t array;
	jsval_t string;
	jsval_t same_string;
	jsval_t got;
	jsval_t sum;
	jsval_t cycle_object;

	jsval_region_init(&region, storage, sizeof(storage));
	pages = jsval_region_pages(&region);
	assert(pages != NULL);
	assert(pages->magic == JSVAL_PAGES_MAGIC);
	assert(pages->version == JSVAL_PAGES_VERSION);
	assert(pages->header_size == sizeof(jsval_pages_t));
	assert(region.used == jsval_pages_head_size());

	assert(jsval_object_new(&region, 4, &object) == 0);
	assert(jsval_region_set_root(&region, object) == 0);
	assert(jsval_array_new(&region, 4, &array) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5, &string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5, &same_string) == 0);
	assert(jsval_region_root(&region, &got) == 0);
	assert(got.kind == JSVAL_KIND_OBJECT);
	assert(got.repr == JSVAL_REPR_NATIVE);
	assert(got.off == object.off);

	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"msg", 3, string) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"items", 5, array) == 0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"msg", 3, &got) == 0);
	assert_string(&region, got, "hello");
	assert(jsval_strict_eq(&region, string, same_string) == 1);

	assert(jsval_array_set(&region, array, 2, jsval_number(7.0)) == 0);
	assert(jsval_array_length(&region, array) == 3);
	assert(jsval_array_get(&region, array, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 7.0);

	assert(jsval_array_set(&region, array, 0, jsval_null()) == 0);
	assert(jsval_array_set(&region, array, 1, jsval_bool(1)) == 0);

	assert(jsval_truthy(&region, object) == 1);
	assert(jsval_truthy(&region, array) == 1);
	assert(jsval_truthy(&region, string) == 1);
	assert(jsval_truthy(&region, jsval_undefined()) == 0);
	assert(jsval_truthy(&region, jsval_null()) == 0);
	assert(jsval_truthy(&region, jsval_number(0.0)) == 0);

	assert(jsval_add(&region, string, jsval_number(7.0), &sum) == 0);
	assert_string(&region, sum, "hello7");

	assert(jsval_add(&region, jsval_number(2.5), jsval_bool(1), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 3.5);

	assert_json(&region, object, "{\"msg\":\"hello\",\"items\":[null,true,7]}");
	assert(jsval_copy_json(&region, jsval_undefined(), NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_copy_json(&region, jsval_number(NAN), NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_object_new(&region, 1, &cycle_object) == 0);
	assert(jsval_object_set_utf8(&region, cycle_object, (const uint8_t *)"self", 4, cycle_object) == 0);
	assert(jsval_copy_json(&region, cycle_object, NULL, 0, NULL) < 0);
	assert(errno == ELOOP);
}

static void test_json_storage()
{
	static const char json[] = "{\"message\":\"hi\",\"items\":[1,true,null],\"nested\":{\"a\":\"x\"}}";
	uint8_t storage[32768];
	uint8_t moved_storage[32768];
	jsval_region_t region;
	jsval_region_t moved;
	jsval_t root;
	jsval_t native_root;
	jsval_t message;
	jsval_t items;
	jsval_t nested;
	jsval_t got;
	jsval_t sum;
	size_t used;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_region_root(&region, &got) == 0);
	assert(got.repr == JSVAL_REPR_JSON);
	assert(got.off == root.off);
	assert(got.as.index == root.as.index);
	assert(root.kind == JSVAL_KIND_OBJECT);
	assert(root.repr == JSVAL_REPR_JSON);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"message", 7, &message) == 0);
	assert(message.kind == JSVAL_KIND_STRING);
	assert(message.repr == JSVAL_REPR_JSON);
	assert_string(&region, message, "hi");

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);
	assert(items.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, items) == 3);
	assert(jsval_array_get(&region, items, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_BOOL);
	assert(jsval_truthy(&region, got) == 1);
	assert(jsval_array_get(&region, items, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_NULL);
	assert(jsval_truthy(&region, got) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nested", 6, &nested) == 0);
	assert(jsval_object_get_utf8(&region, nested, (const uint8_t *)"a", 1, &got) == 0);
	assert_string(&region, got, "x");

	assert_json(&region, root, json);
	assert_json(&region, message, "\"hi\"");

	assert(jsval_add(&region, message, jsval_number(3.0), &sum) == 0);
	assert_string(&region, sum, "hi3");

	assert(jsval_promote(&region, root, &native_root) == 0);
	assert(native_root.kind == JSVAL_KIND_OBJECT);
	assert(native_root.repr == JSVAL_REPR_NATIVE);
	assert(jsval_region_set_root(&region, native_root) == 0);
	assert(jsval_object_get_utf8(&region, native_root, (const uint8_t *)"message", 7, &got) == 0);
	assert_string(&region, got, "hi");
	assert(jsval_strict_eq(&region, got, message) == 1);
	assert_json(&region, native_root, "{\"message\":\"hi\",\"items\":[1,true,null],\"nested\":{\"a\":\"x\"}}");

	used = region.used;
	memcpy(moved_storage, storage, used);
	moved = region;
	jsval_region_rebase(&moved, moved_storage, sizeof(moved_storage));
	assert(jsval_region_root(&moved, &got) == 0);
	assert(got.kind == JSVAL_KIND_OBJECT);
	assert(got.repr == JSVAL_REPR_NATIVE);
	assert(jsval_object_get_utf8(&moved, got, (const uint8_t *)"message", 7, &nested) == 0);
	assert_string(&moved, nested, "hi");
	assert(jsval_object_get_utf8(&moved, got, (const uint8_t *)"nested", 6, &nested) == 0);
	assert(jsval_object_get_utf8(&moved, nested, (const uint8_t *)"a", 1, &got) == 0);
	assert_string(&moved, got, "x");
}

static void test_json_mutation_requires_promotion(void)
{
	static const char json[] = "{\n  \"message\": \"hi\",\n  \"items\": [1, true, null]\n}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t promoted;
	jsval_t replacement;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);

	errno = 0;
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"message", 7, jsval_null()) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_promote(&region, root, &promoted) == 0);
	assert(promoted.repr == JSVAL_REPR_NATIVE);
	assert(jsval_region_set_root(&region, promoted) == 0);
	assert(jsval_object_get_utf8(&region, promoted, (const uint8_t *)"items", 5, &items) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"line\n\"quoted\"", 13, &replacement) == 0);
	assert(jsval_object_set_utf8(&region, promoted, (const uint8_t *)"message", 7, replacement) == 0);
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) == 0);
	assert_json(&region, promoted, "{\"message\":\"line\\n\\\"quoted\\\"\",\"items\":[7,true,null]}");
}

static void test_json_root_rebase()
{
	static const char json[] = "{\"ok\":true}";
	uint8_t storage[4096];
	uint8_t moved_storage[4096];
	jsval_region_t region;
	jsval_region_t moved;
	jsval_t root;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 8, &root) == 0);
	memcpy(moved_storage, storage, region.used);
	jsval_region_rebase(&moved, moved_storage, sizeof(moved_storage));
	assert(jsval_region_root(&moved, &root) == 0);
	assert(root.repr == JSVAL_REPR_JSON);
	assert(jsval_object_get_utf8(&moved, root, (const uint8_t *)"ok", 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_BOOL);
	assert(jsval_truthy(&moved, got) == 1);
}

static void test_native_container_helpers(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t object;
	jsval_t array;
	jsval_t got;
	int has;
	int deleted;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_object_new(&region, 4, &object) == 0);
	assert(jsval_array_new(&region, 5, &array) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"drop", 4,
			jsval_number(7.0)) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"items", 5,
			array) == 0);

	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"keep", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);

	assert(jsval_object_delete_utf8(&region, object, (const uint8_t *)"missing", 7,
			&deleted) == 0);
	assert(deleted == 0);
	assert(jsval_object_delete_utf8(&region, object, (const uint8_t *)"drop", 4,
			&deleted) == 0);
	assert(deleted == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"drop", 4,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_object_size(&region, object) == 2);

	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_length(&region, array) == 2);
	assert(jsval_array_set_length(&region, array, 5) == 0);
	assert(jsval_array_length(&region, array) == 5);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, array, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set_length(&region, array, 2) == 0);
	assert(jsval_array_length(&region, array) == 2);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_length(&region, array) == 3);
	errno = 0;
	assert(jsval_array_set_length(&region, array, 6) < 0);
	assert(errno == ENOBUFS);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(5.0)) == 0);
	errno = 0;
	assert(jsval_array_push(&region, array, jsval_number(6.0)) < 0);
	assert(errno == ENOBUFS);

	assert_json(&region, object, "{\"keep\":true,\"items\":[1,2,3,4,5]}");
}

static void test_json_container_helpers(void)
{
	static const char json[] = "{\"drop\":1,\"keep\":true,\"items\":[1,2]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	int has;
	int deleted;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);

	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);

	errno = 0;
	assert(jsval_object_delete_utf8(&region, root, (const uint8_t *)"drop", 4,
			&deleted) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 3) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_region_promote_root(&region, &root) == 0);
	assert(jsval_object_delete_utf8(&region, root, (const uint8_t *)"drop", 4,
			&deleted) == 0);
	assert(deleted == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 0);
	assert_json(&region, root, "{\"keep\":true,\"items\":[1,2]}");

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOBUFS);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 3) < 0);
	assert(errno == ENOBUFS);
}

static void test_policy_layer()
{
	static const char json[] = "{\"message\":\"hi\",\"items\":[1,true,null]}";
	uint8_t storage[32768];
	uint8_t slot_storage[16384];
	jsval_region_t region;
	jsval_region_t slot_region;
	jsval_t root;
	jsval_t root_again;
	jsval_t items;
	jsval_t message;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_is_json_backed(root) == 1);
	assert(jsval_is_native(root) == 0);
	assert(jsval_is_native(jsval_number(1.0)) == 0);
	assert(jsval_is_json_backed(jsval_number(1.0)) == 0);

	assert(jsval_region_promote_root(&region, &root) == 0);
	assert(jsval_is_native(root) == 1);
	assert(jsval_is_json_backed(root) == 0);
	assert(jsval_region_root(&region, &root_again) == 0);
	assert(root_again.kind == JSVAL_KIND_OBJECT);
	assert(root_again.repr == JSVAL_REPR_NATIVE);
	assert(root_again.off == root.off);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);
	assert(jsval_is_native(items) == 1);
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) == 0);
	assert_json(&region, root, "{\"message\":\"hi\",\"items\":[7,true,null]}");

	jsval_region_init(&slot_region, slot_storage, sizeof(slot_storage));
	assert(jsval_json_parse(&slot_region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_object_get_utf8(&slot_region, root, (const uint8_t *)"message", 7, &message) == 0);
	assert(jsval_is_json_backed(message) == 1);
	assert(jsval_promote_in_place(&slot_region, &message) == 0);
	assert(jsval_is_native(message) == 1);
	assert_string(&slot_region, message, "hi");
	assert(jsval_region_root(&slot_region, &got) == 0);
	assert(jsval_is_json_backed(got) == 1);
}

static void test_method_bridge()
{
	static const char json[] = "{\"city\":\"istanbul\"}";
	static const uint16_t broken_units[] = {0xD83D};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t city;
	jsval_t broken;
	jsval_t mixed;
	jsval_t locale;
	jsval_t upper;
	jsval_t lower;
	jsval_t repaired;
	jsval_t well_formed;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"city", 4, &city) == 0);
	assert(jsval_string_new_utf16(&region, broken_units,
			sizeof(broken_units) / sizeof(broken_units[0]), &broken) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"Hello, WoRlD!", 13, &mixed) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"tr", 2, &locale) == 0);

	assert(jsval_method_string_to_locale_upper_case(&region, city, 1, locale,
			&upper, &error) == 0);
	assert(jsval_is_native(upper) == 1);
	assert_string(&region, upper, "\xC4\xB0STANBUL");

	assert(jsval_method_string_to_lower_case(&region, mixed, &lower,
			&error) == 0);
	assert(jsval_is_native(lower) == 1);
	assert_string(&region, lower, "hello, world!");

	assert(jsval_method_string_is_well_formed(&region, broken, &well_formed,
			&error) == 0);
	assert(well_formed.kind == JSVAL_KIND_BOOL);
	assert(well_formed.as.boolean == 0);

	assert(jsval_method_string_to_well_formed(&region, broken, &repaired,
			&error) == 0);
	assert_string(&region, repaired, "\xEF\xBF\xBD");

	errno = 0;
	assert(jsval_method_string_to_upper_case(&region, root, &upper, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_normalize_bridge(void)
{
	static const char json[] = "{\"name\":\"A\xCC\x8A\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t name;
	jsval_t normalized;
	jsmethod_error_t error;
	jsmethod_string_normalize_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"name", 4, &name) == 0);

	assert(jsval_method_string_normalize_measure(&region, name, 0,
			jsval_undefined(), &sizes, &error) == 0);
	assert(sizes.this_storage_len == 2);
	assert(sizes.form_storage_len == 0);
	assert(sizes.workspace_len == 4);
	assert(sizes.result_len == 1);

	{
		uint16_t this_storage[sizes.this_storage_len];
		uint32_t workspace[sizes.workspace_len];

		assert(jsval_method_string_normalize(&region, name, 0, jsval_undefined(),
				this_storage, sizes.this_storage_len,
				NULL, 0,
				workspace, sizes.workspace_len,
				&normalized, &error) == 0);
	}
	assert(jsval_is_native(normalized) == 1);
	assert_string(&region, normalized, "\xC3\x85");

	errno = 0;
	assert(jsval_method_string_normalize_measure(&region, root, 0,
			jsval_undefined(), &sizes, &error) == -1);
	assert(errno == ENOTSUP);
}

static void test_method_search_bridge(void)
{
	static const char json[] = "{\"text\":\"bananas\",\"needle\":\"na\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t needle;
	jsval_t pos_three;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"needle", 6,
			&needle) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"3", 1,
			&pos_three) == 0);

	assert(jsval_method_string_index_of(&region, text, needle, 0,
			jsval_undefined(), &result, &error) == 0);
	assert_number_value(result, 2.0);

	assert(jsval_method_string_last_index_of(&region, text, needle, 1,
			pos_three, &result, &error) == 0);
	assert_number_value(result, 2.0);

	assert(jsval_method_string_includes(&region, text, needle, 1, pos_three,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	assert(jsval_method_string_starts_with(&region, text, needle, 1,
			jsval_number(2.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	assert(jsval_method_string_ends_with(&region, text, needle, 1,
			jsval_number(4.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	errno = 0;
	assert(jsval_method_string_index_of(&region, root, needle, 0,
			jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_accessor_bridge(void)
{
	static const char json[] = "{\"ascii\":\"abc\",\"astral\":\"\\ud83d\\ude00\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t ascii;
	jsval_t astral;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"ascii", 5,
			&ascii) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"astral", 6,
			&astral) == 0);

	assert(jsval_method_string_char_at(&region, ascii, 1, jsval_number(1.9),
			&result, &error) == 0);
	assert_string(&region, result, "b");

	assert(jsval_method_string_at(&region, ascii, 1, jsval_number(-1.0),
			&result, &error) == 0);
	assert_string(&region, result, "c");

	assert(jsval_method_string_at(&region, ascii, 1, jsval_number(99.0),
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(2.0), &result, &error) == 0);
	assert_number_value(result, 99.0);

	assert(jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(INFINITY), &result, &error) == 0);
	assert_nan_value(result);

	assert(jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(0.0), &result, &error) == 0);
	assert_number_value(result, 0x1F600);

	assert(jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(99.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	errno = 0;
	assert(jsval_method_string_char_at(&region, root, 1, jsval_number(0.0),
			&result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_value_semantics(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t null_word;
	jsval_t one_string;
	jsval_t same_a;
	jsval_t same_b;
	jsval_t sum;
	jsval_t undefined_word;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"null", 4, &null_word) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_b) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"undefined", 9,
			&undefined_word) == 0);

	assert(jsval_truthy(&region, jsval_number(1.0)) == 1);
	assert(jsval_truthy(&region, jsval_bool(1)) == 1);
	assert(jsval_truthy(&region, one_string) == 1);
	assert(jsval_truthy(&region, jsval_number(0.0)) == 0);
	assert(jsval_truthy(&region, jsval_bool(0)) == 0);
	assert(jsval_truthy(&region, jsval_null()) == 0);
	assert(jsval_truthy(&region, jsval_undefined()) == 0);
	assert(jsval_truthy(&region, empty_string) == 0);
	assert(jsval_truthy(&region, jsval_number(NAN)) == 0);

	assert(jsval_strict_eq(&region, jsval_undefined(), jsval_undefined()) == 1);
	assert(jsval_strict_eq(&region, jsval_null(), jsval_null()) == 1);
	assert(jsval_strict_eq(&region, jsval_number(INFINITY), jsval_number(INFINITY)) == 1);
	assert(jsval_strict_eq(&region, jsval_number(-INFINITY), jsval_number(-INFINITY)) == 1);
	assert(jsval_strict_eq(&region, jsval_number(+0.0), jsval_number(-0.0)) == 1);
	assert(jsval_strict_eq(&region, same_a, same_b) == 1);
	assert(jsval_strict_eq(&region, jsval_number(1.0), jsval_number(0.999999999999)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(NAN), jsval_bool(1)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(NAN), jsval_number(NAN)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0);
	assert(jsval_strict_eq(&region, jsval_bool(1), jsval_number(1.0)) == 0);
	assert(jsval_strict_eq(&region, jsval_null(), jsval_undefined()) == 0);
	assert(jsval_strict_eq(&region, jsval_null(), null_word) == 0);
	assert(jsval_strict_eq(&region, jsval_undefined(), undefined_word) == 0);
	assert((jsval_strict_eq(&region, jsval_null(), jsval_undefined()) == 0) == 1);
	assert((jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0) == 1);

	assert(jsval_add(&region, jsval_number(1.0), jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 2.0);
	assert(jsval_add(&region, one_string, jsval_number(1.0), &sum) == 0);
	assert_string(&region, sum, "11");
	assert(jsval_add(&region, jsval_number(1.0), one_string, &sum) == 0);
	assert_string(&region, sum, "11");
}

static void test_logical_not_semantics(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t x;
	int c = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	x = jsval_bool(1);

	assert((!jsval_truthy(&region, jsval_bool(1))) == 0);
	assert((!(!jsval_truthy(&region, jsval_bool(1)))) == 1);
	assert((!jsval_truthy(&region, x)) == 0);
	assert((!(!jsval_truthy(&region, x))) == 1);
	assert((!jsval_truthy(&region, jsval_bool(0))) == 1);
	assert((!jsval_truthy(&region, jsval_null())) == 1);
	assert((!jsval_truthy(&region, jsval_undefined())) == 1);
	assert((!jsval_truthy(&region, jsval_number(+0.0))) == 1);
	assert((!jsval_truthy(&region, jsval_number(-0.0))) == 1);
	assert((!jsval_truthy(&region, jsval_number(NAN))) == 1);
	assert((!jsval_truthy(&region, jsval_number(INFINITY))) == 0);
	assert((!jsval_truthy(&region, jsval_number(-13.0))) == 0);
	assert((!jsval_truthy(&region, empty_string)) == 1);
	assert((!jsval_truthy(&region, one_string)) == 0);

	if (!jsval_truthy(&region, jsval_number(1.0))) {
		assert(0);
	} else {
		c++;
	}
	if (!jsval_truthy(&region, jsval_bool(1))) {
		assert(0);
	} else {
		c++;
	}
	if (!jsval_truthy(&region, one_string)) {
		assert(0);
	} else {
		c++;
	}
	assert(c == 3);
}

static void test_logical_and_or_semantics(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t minus_one_string;
	jsval_t x_string;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"-1", 2,
			&minus_one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &x_string) == 0);

	result = logical_and(&region, jsval_bool(0), jsval_bool(1));
	assert(jsval_strict_eq(&region, result, jsval_bool(0)) == 1);
	result = logical_and(&region, jsval_bool(1), jsval_bool(0));
	assert(jsval_strict_eq(&region, result, jsval_bool(0)) == 1);

	result = logical_and(&region, jsval_number(-0.0), jsval_number(-1.0));
	assert_negative_zero(result);
	result = logical_and(&region, jsval_number(0.0), jsval_number(-1.0));
	assert_positive_zero(result);
	result = logical_and(&region, jsval_number(NAN), jsval_number(1.0));
	assert_nan_value(result);

	result = logical_and(&region, empty_string, one_string);
	assert_string(&region, result, "");
	result = logical_and(&region, one_string, x_string);
	assert_string(&region, result, "x");
	result = logical_and(&region, minus_one_string, x_string);
	assert_string(&region, result, "x");

	result = logical_or(&region, jsval_bool(0), jsval_bool(1));
	assert(jsval_strict_eq(&region, result, jsval_bool(1)) == 1);
	result = logical_or(&region, jsval_number(0.0), jsval_number(-0.0));
	assert_negative_zero(result);
	result = logical_or(&region, jsval_number(-0.0), jsval_number(0.0));
	assert_positive_zero(result);
	result = logical_or(&region, empty_string, one_string);
	assert_string(&region, result, "1");

	result = logical_or(&region, jsval_number(-1.0), jsval_number(1.0));
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	result = logical_or(&region, jsval_number(-1.0), jsval_number(NAN));
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	result = logical_or(&region, minus_one_string, one_string);
	assert_string(&region, result, "-1");
	result = logical_or(&region, minus_one_string, x_string);
	assert_string(&region, result, "-1");
}

static void test_numeric_coercion_and_arithmetic(void)
{
	static const char json[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"spaces\":\"   \",\"bad\":\"x\",\"inf\":\"Infinity\",\"upper\":\"INFINITY\",\"num\":2}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t spaces;
	jsval_t bad;
	jsval_t inf;
	jsval_t upper;
	jsval_t num;
	jsval_t result;
	double number;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_to_number(&region, jsval_undefined(), &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, jsval_null(), &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, jsval_bool(0), &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, jsval_bool(1), &number) == 0);
	assert(number == 1.0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"   ", 3, &spaces) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"Infinity", 8, &inf) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"INFINITY", 8, &upper) == 0);

	assert(jsval_to_number(&region, one, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, spaces, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, bad, &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, inf, &number) == 0);
	assert(isinf(number) && number > 0);
	assert(jsval_to_number(&region, upper, &number) == 0);
	assert(isnan(number));

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"spaces", 6,
			&spaces) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"inf", 3,
			&inf) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"upper", 5,
			&upper) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);

	assert(jsval_to_number(&region, truth, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, nothing, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, one, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, spaces, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, bad, &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, inf, &number) == 0);
	assert(isinf(number) && number > 0);
	assert(jsval_to_number(&region, upper, &number) == 0);
	assert(isnan(number));

	assert(jsval_unary_plus(&region, jsval_undefined(), &result) == 0);
	assert_nan_value(result);
	assert(jsval_unary_plus(&region, jsval_null(), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_unary_plus(&region, truth, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_unary_plus(&region, one, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_unary_plus(&region, inf, &result) == 0);
	assert(isinf(result.as.number) && result.as.number > 0);
	assert(jsval_unary_plus(&region, upper, &result) == 0);
	assert_nan_value(result);

	assert(jsval_unary_minus(&region, jsval_bool(0), &result) == 0);
	assert_negative_zero(result);
	assert(jsval_unary_minus(&region, jsval_bool(1), &result) == 0);
	assert_number_value(result, -1.0);
	assert(jsval_unary_minus(&region, jsval_number(0.0), &result) == 0);
	assert_negative_zero(result);
	assert(jsval_unary_minus(&region, jsval_number(-0.0), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_unary_minus(&region, one, &result) == 0);
	assert_number_value(result, -1.0);
	assert(jsval_unary_minus(&region, bad, &result) == 0);
	assert_nan_value(result);

	assert(jsval_subtract(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_subtract(&region, jsval_bool(1), jsval_number(1.0), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_subtract(&region, jsval_number(1.0), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);

	assert(jsval_multiply(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_divide(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_remainder(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);

	errno = 0;
	assert(jsval_to_number(&region, root, &number) < 0);
	assert(errno == ENOTSUP);
}

static void test_integer_coercion_and_bitwise(void)
{
	static const char json[] =
		"{\"truth\":true,\"zero\":\"0\",\"one\":\"1\",\"bad\":\"x\",\"nothing\":null,\"num\":5,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t zero;
	jsval_t one;
	jsval_t bad;
	jsval_t nothing;
	jsval_t num;
	jsval_t obj;
	jsval_t arr;
	jsval_t result;
	int32_t i32;
	uint32_t u32;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_to_int32(&region, jsval_bool(1), &i32) == 0);
	assert(i32 == 1);
	assert(jsval_to_int32(&region, jsval_undefined(), &i32) == 0);
	assert(i32 == 0);
	assert(jsval_to_uint32(&region, jsval_number(4294967295.0), &u32) == 0);
	assert(u32 == 4294967295u);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_to_int32(&region, one, &i32) == 0);
	assert(i32 == 1);
	assert(jsval_to_int32(&region, bad, &i32) == 0);
	assert(i32 == 0);

	assert(jsval_bitwise_not(&region, jsval_bool(0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_not(&region, one, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-2.0)) == 1);
	assert(jsval_bitwise_not(&region, jsval_null(), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_not(&region, bad, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);

	assert(jsval_bitwise_and(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_or(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_xor(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
			&zero) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_to_int32(&region, zero, &i32) == 0);
	assert(i32 == 0);
	assert(jsval_to_int32(&region, one, &i32) == 0);
	assert(i32 == 1);
	assert(jsval_bitwise_not(&region, nothing, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_and(&region, truth, num, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_or(&region, one, jsval_bool(0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_xor(&region, bad, jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);

	errno = 0;
	assert(jsval_bitwise_not(&region, obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_bitwise_and(&region, arr, jsval_bool(1), &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_shift_semantics(void)
{
	static const char json[] =
		"{\"truth\":true,\"count\":\"33\",\"neg\":-1,\"num\":5,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t count;
	jsval_t neg;
	jsval_t num;
	jsval_t obj;
	jsval_t arr;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_shift_left(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, jsval_bool(1), jsval_bool(1),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);

	assert(jsval_shift_left(&region, jsval_number(1.0), jsval_number(33.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_number(5.0), jsval_number(33.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_number(-1.0), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, jsval_number(-1.0),
			jsval_number(1.0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2147483647.0)) == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 20,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"count", 5,
			&count) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"neg", 3,
			&neg) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_shift_left(&region, truth, count, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, num, count, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, neg, truth, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2147483647.0)) == 1);

	errno = 0;
	assert(jsval_shift_left(&region, obj, jsval_bool(1), &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_shift_right_unsigned(&region, jsval_bool(1), arr, &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_relational_semantics(void)
{
	static const char json[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"bad\":\"x\",\"num\":2,\"left\":\"1\",\"right\":\"2\"}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t bad;
	jsval_t num;
	jsval_t left_string;
	jsval_t right_string;
	jsval_t ten_string;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_less_than(&region, jsval_bool(1), jsval_number(1.0), &result)
			== 0);
	assert(result == 0);
	assert(jsval_less_than(&region, jsval_number(1.0), jsval_bool(1), &result)
			== 0);
	assert(result == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&left_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&right_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"10", 2,
			&ten_string) == 0);

	assert(jsval_less_than(&region, jsval_bool(1), one, &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, one, jsval_bool(1), &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, jsval_null(), jsval_bool(1), &result) == 0);
	assert(result == 1);

	assert(jsval_less_equal(&region, jsval_bool(1), jsval_number(1.0), &result)
			== 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, jsval_number(1.0), jsval_bool(1), &result)
			== 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_than(&region, jsval_number(1.0), jsval_bool(1),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, jsval_number(1.0), jsval_bool(1),
			&result) == 0);
	assert(result == 1);

	assert(jsval_less_than(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_less_equal(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_than(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);

	assert(jsval_less_than(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, right_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, right_string, left_string, &result)
			== 0);
	assert(result == 1);
	assert(jsval_less_than(&region, left_string, ten_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, ten_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"left", 4,
			&left_string) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"right", 5,
			&right_string) == 0);

	assert(jsval_greater_than(&region, num, truth, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, nothing, truth, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, one, truth, &result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, truth, one, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, one, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, right_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);
}

static void test_abstract_equality_semantics(void)
{
	static const char json[] =
		"{\"num\":1,\"flag\":true,\"text\":\"1\",\"empty\":\"\",\"nothing\":null,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t one;
	jsval_t zero;
	jsval_t bad;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t text;
	jsval_t empty;
	jsval_t nothing;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_obj;
	jsval_t native_arr;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"0", 1, &zero) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);

	assert(jsval_abstract_eq(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_number(0.0), jsval_bool(0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, one, jsval_bool(1), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_bool(0), zero, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, one, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 0);
	assert(jsval_abstract_eq(&region, jsval_null(), jsval_undefined(),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_null(), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_eq(&region, jsval_number(NAN), jsval_bool(1),
			&result) == 0);
	assert(result == 0);

	assert(jsval_abstract_ne(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_ne(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_ne(&region, jsval_null(), jsval_undefined(),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_ne(&region, jsval_number(NAN), jsval_number(NAN),
			&result) == 0);
	assert(result == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
			&empty) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_abstract_eq(&region, num, one, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, flag, one, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, text, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, empty, jsval_number(0.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, nothing, jsval_undefined(), &result)
			== 0);
	assert(result == 1);
	assert(jsval_abstract_ne(&region, nothing, jsval_number(0.0), &result)
			== 0);
	assert(result == 1);

	assert(jsval_object_new(&region, 1, &native_obj) == 0);
	assert(jsval_array_new(&region, 1, &native_arr) == 0);

	errno = 0;
	assert(jsval_abstract_eq(&region, obj, obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_ne(&region, arr, arr, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_eq(&region, native_obj, native_obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_ne(&region, native_arr, native_arr, &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_json_backed_value_parity(void)
{
	static const char json[] =
		"{\"num\":1,\"flag\":true,\"off\":false,\"text\":\"2\",\"nothing\":null,\"obj\":{},\"obj2\":{},\"arr\":[],\"arr2\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t off;
	jsval_t text;
	jsval_t nothing;
	jsval_t obj;
	jsval_t obj_again;
	jsval_t obj2;
	jsval_t arr;
	jsval_t arr_again;
	jsval_t arr2;
	jsval_t native_text;
	jsval_t native_obj;
	jsval_t native_obj_other;
	jsval_t native_arr;
	jsval_t native_arr_other;
	jsval_t sum;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 48,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"off", 3,
			&off) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj_again) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj2", 4,
			&obj2) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr_again) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr2", 4,
			&arr2) == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&native_text) == 0);
	assert(jsval_object_new(&region, 0, &native_obj) == 0);
	assert(jsval_object_new(&region, 0, &native_obj_other) == 0);
	assert(jsval_array_new(&region, 0, &native_arr) == 0);
	assert(jsval_array_new(&region, 0, &native_arr_other) == 0);

	assert(jsval_truthy(&region, num) == jsval_truthy(&region, jsval_number(1.0)));
	assert(jsval_truthy(&region, flag) == jsval_truthy(&region, jsval_bool(1)));
	assert(jsval_truthy(&region, off) == jsval_truthy(&region, jsval_bool(0)));
	assert(jsval_truthy(&region, text) == jsval_truthy(&region, native_text));
	assert(jsval_truthy(&region, nothing) == jsval_truthy(&region, jsval_null()));

	assert(jsval_strict_eq(&region, num, jsval_number(1.0)) == 1);
	assert(jsval_strict_eq(&region, flag, jsval_bool(1)) == 1);
	assert(jsval_strict_eq(&region, off, jsval_bool(0)) == 1);
	assert(jsval_strict_eq(&region, text, native_text) == 1);
	assert(jsval_strict_eq(&region, nothing, jsval_null()) == 1);

	assert(jsval_add(&region, text, jsval_number(1.0), &sum) == 0);
	assert_string(&region, sum, "21");
	assert(jsval_add(&region, flag, jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 2.0);
	assert(jsval_add(&region, nothing, jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 1.0);

	assert(jsval_truthy(&region, obj) == 1);
	assert(jsval_truthy(&region, arr) == 1);
	assert(jsval_truthy(&region, native_obj) == 1);
	assert(jsval_truthy(&region, native_arr) == 1);

	assert(jsval_strict_eq(&region, obj, obj_again) == 1);
	assert(jsval_strict_eq(&region, obj, obj2) == 0);
	assert(jsval_strict_eq(&region, arr, arr_again) == 1);
	assert(jsval_strict_eq(&region, arr, arr2) == 0);
	assert(jsval_strict_eq(&region, native_obj, native_obj) == 1);
	assert(jsval_strict_eq(&region, native_obj, native_obj_other) == 0);
	assert(jsval_strict_eq(&region, native_arr, native_arr) == 1);
	assert(jsval_strict_eq(&region, native_arr, native_arr_other) == 0);
}

static void test_shallow_planned_promotion(void)
{
	static const char json[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2],\"status\":\"ok\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t status;
	jsval_t scores;
	jsval_t same_root;
	jsval_t same_scores;
	jsval_t got;
	size_t bytes;
	size_t before_used;
	int has;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) == 0);
	assert(jsval_is_json_backed(profile) == 1);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6,
			&scores) == 0);
	assert(jsval_is_json_backed(scores) == 1);

	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 2, &bytes) < 0);
	assert(errno == ENOBUFS);

	before_used = region.used;
	assert(jsval_promote_object_shallow_measure(&region, root, 4, &bytes) == 0);
	assert(bytes > 0);
	assert(jsval_promote_object_shallow_in_place(&region, &root, 4) == 0);
	assert(jsval_is_native(root) == 1);
	assert(region.used == before_used + bytes);
	assert(jsval_region_set_root(&region, root) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) == 0);
	assert(jsval_is_json_backed(profile) == 1);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"status", 6,
			&status) == 0);
	assert(jsval_is_json_backed(status) == 1);
	assert_string(&region, status, "ok");
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"ready", 5,
			jsval_bool(1)) == 0);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"ready", 5,
			&has) == 0);
	assert(has == 1);

	assert(jsval_promote_object_shallow_measure(&region, root, 4, &bytes) == 0);
	assert(bytes == 0);
	assert(jsval_promote_object_shallow(&region, root, 4, &same_root) == 0);
	assert(same_root.off == root.off);
	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 5, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6,
			&scores) == 0);
	assert(jsval_is_json_backed(scores) == 1);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, scores, 1, &bytes) < 0);
	assert(errno == ENOBUFS);

	before_used = region.used;
	assert(jsval_promote_array_shallow_measure(&region, scores, 4, &bytes) == 0);
	assert(bytes > 0);
	assert(jsval_promote_array_shallow_in_place(&region, &scores, 4) == 0);
	assert(jsval_is_native(scores) == 1);
	assert(region.used == before_used + bytes);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"scores", 6,
			scores) == 0);

	assert(jsval_array_push(&region, scores, jsval_number(3.0)) == 0);
	assert(jsval_array_set_length(&region, scores, 4) == 0);
	assert(jsval_array_get(&region, scores, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set(&region, scores, 3, jsval_number(4.0)) == 0);

	assert(jsval_promote_array_shallow_measure(&region, scores, 4, &bytes) == 0);
	assert(bytes == 0);
	assert(jsval_promote_array_shallow(&region, scores, 4, &same_scores) == 0);
	assert(same_scores.off == scores.off);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, scores, 5, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert_json(&region, root,
			"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2,3,4],\"status\":\"ok\",\"ready\":true}");
}

static void test_lookup_and_capacity_contracts(void)
{
	static const char json[] =
		"{\"keep\":7,\"items\":[1,2],\"nested\":{\"flag\":true}}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	size_t size_before;
	size_t len_before;
	size_t bytes;
	int has;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"missing", 7,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"keep", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	assert(jsval_array_get(&region, items, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	errno = 0;
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 2, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert(jsval_promote_object_shallow_in_place(&region, &root, 3) == 0);
	assert(jsval_region_set_root(&region, root) == 0);
	size_before = jsval_object_size(&region, root);
	assert(size_before == 3);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) == 0);
	assert(jsval_object_size(&region, root) == size_before);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"missing", 7,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	assert(jsval_is_json_backed(items) == 1);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, items, 1, &bytes) < 0);
	assert(errno == ENOBUFS);
	assert(jsval_promote_array_shallow_in_place(&region, &items, 3) == 0);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"items", 5,
			items) == 0);
	len_before = jsval_array_length(&region, items);
	assert(len_before == 2);
	assert(jsval_array_set(&region, items, 1, jsval_number(8.0)) == 0);
	assert(jsval_array_length(&region, items) == len_before);
	assert(jsval_array_get(&region, items, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_push(&region, items, jsval_number(3.0)) == 0);
	assert(jsval_array_length(&region, items) == 3);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(4.0)) < 0);
	assert(errno == ENOBUFS);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 4) < 0);
	assert(errno == ENOBUFS);

	assert_json(&region, root,
			"{\"keep\":9,\"items\":[1,8,3],\"nested\":{\"flag\":true}}");
}

static void test_dense_array_observable_behavior(void)
{
	static const char json[] = "[1,2]";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t native_array;
	jsval_t parsed_array;
	jsval_t got;
	size_t len_before;
	size_t bytes;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_new(&region, 4, &native_array) == 0);
	assert(jsval_array_set(&region, native_array, 0, jsval_number(1.0)) == 0);
	assert(jsval_array_set(&region, native_array, 1, jsval_number(2.0)) == 0);
	len_before = jsval_array_length(&region, native_array);
	assert(len_before == 2);
	assert(jsval_array_set(&region, native_array, 1, jsval_number(9.0)) == 0);
	assert(jsval_array_length(&region, native_array) == len_before);
	assert(jsval_array_get(&region, native_array, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 9.0);

	assert(jsval_array_set_length(&region, native_array, 4) == 0);
	assert(jsval_array_get(&region, native_array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, native_array, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set(&region, native_array, 2, jsval_number(7.0)) == 0);
	assert(jsval_array_set(&region, native_array, 3, jsval_number(8.0)) == 0);
	assert_json(&region, native_array, "[1,9,7,8]");
	assert(jsval_array_set_length(&region, native_array, 2) == 0);
	assert(jsval_array_get(&region, native_array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert_json(&region, native_array, "[1,9]");

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 8,
			&parsed_array) == 0);
	assert(jsval_is_json_backed(parsed_array) == 1);
	assert(jsval_array_get(&region, parsed_array, 1, &got) == 0);
	assert(jsval_strict_eq(&region, got, jsval_number(2.0)) == 1);
	assert(jsval_promote_array_shallow_measure(&region, parsed_array, 3, &bytes)
			== 0);
	assert(bytes > 0);
	assert(jsval_promote_array_shallow_in_place(&region, &parsed_array, 3) == 0);
	assert(jsval_is_native(parsed_array) == 1);
	assert(jsval_region_set_root(&region, parsed_array) == 0);
	assert(jsval_array_set(&region, parsed_array, 0, jsval_number(7.0)) == 0);
	assert(jsval_array_push(&region, parsed_array, jsval_number(3.0)) == 0);
	assert_json(&region, parsed_array, "[7,2,3]");
}

int main(void)
{
	test_native_storage();
	test_value_semantics();
	test_logical_not_semantics();
	test_logical_and_or_semantics();
	test_numeric_coercion_and_arithmetic();
	test_integer_coercion_and_bitwise();
	test_shift_semantics();
	test_relational_semantics();
	test_abstract_equality_semantics();
	test_json_backed_value_parity();
	test_json_storage();
	test_json_root_rebase();
	test_json_mutation_requires_promotion();
	test_native_container_helpers();
	test_json_container_helpers();
	test_policy_layer();
	test_method_bridge();
	test_method_normalize_bridge();
	test_method_search_bridge();
	test_method_accessor_bridge();
	test_shallow_planned_promotion();
	test_lookup_and_capacity_contracts();
	test_dense_array_observable_behavior();
	puts("test_jsval: ok");
	return 0;
}
