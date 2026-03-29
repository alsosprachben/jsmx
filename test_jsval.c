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

static void test_value_semantics(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t same_a;
	jsval_t same_b;
	jsval_t sum;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_b) == 0);

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
	assert(jsval_strict_eq(&region, jsval_number(+0.0), jsval_number(-0.0)) == 1);
	assert(jsval_strict_eq(&region, same_a, same_b) == 1);
	assert(jsval_strict_eq(&region, jsval_number(NAN), jsval_number(NAN)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0);
	assert(jsval_strict_eq(&region, jsval_bool(1), jsval_number(1.0)) == 0);
	assert(jsval_strict_eq(&region, jsval_null(), jsval_undefined()) == 0);

	assert(jsval_add(&region, jsval_number(1.0), jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 2.0);
	assert(jsval_add(&region, one_string, jsval_number(1.0), &sum) == 0);
	assert_string(&region, sum, "11");
	assert(jsval_add(&region, jsval_number(1.0), one_string, &sum) == 0);
	assert_string(&region, sum, "11");
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

int main(void)
{
	test_native_storage();
	test_value_semantics();
	test_json_backed_value_parity();
	test_json_storage();
	test_json_root_rebase();
	test_json_mutation_requires_promotion();
	test_native_container_helpers();
	test_json_container_helpers();
	test_policy_layer();
	test_method_bridge();
	test_method_normalize_bridge();
	test_shallow_planned_promotion();
	test_lookup_and_capacity_contracts();
	puts("test_jsval: ok");
	return 0;
}
