#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "jsmethod.h"

typedef struct callback_ctx_s {
	int should_throw;
	const uint16_t *text;
	size_t len;
} callback_ctx_t;

static int
callback_to_string(void *opaque, jsstr16_t *out, jsmethod_error_t *error)
{
	callback_ctx_t *ctx = (callback_ctx_t *)opaque;

	if (ctx->should_throw) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "callback threw";
		return -1;
	}
	if (jsstr16_set_from_utf16(out, ctx->text, ctx->len) != ctx->len) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

static void
expect_utf16(jsstr16_t *s, const uint16_t *expected, size_t expected_len)
{
	size_t i;

	assert(s->len == expected_len);
	for (i = 0; i < expected_len; i++) {
		assert(s->codeunits[i] == expected[i]);
	}
}

static void
test_value_to_string_primitives(void)
{
	uint16_t storage[32];
	jsstr16_t out;
	jsmethod_error_t error;
	static const uint16_t expected_number[] = {'4', '2'};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	assert(jsmethod_value_to_string(&out, jsmethod_value_bool(1), 0, &error) == 0);
	expect_utf16(&out, (const uint16_t[]){'t','r','u','e'}, 4);

	out.len = 0;
	assert(jsmethod_value_to_string(&out, jsmethod_value_number(42), 0, &error) == 0);
	expect_utf16(&out, expected_number, sizeof(expected_number) / sizeof(expected_number[0]));

	out.len = 0;
	assert(jsmethod_value_to_string(&out, jsmethod_value_null(), 0, &error) == 0);
	expect_utf16(&out, (const uint16_t[]){'n','u','l','l'}, 4);
}

static void
test_value_to_string_errors(void)
{
	uint16_t storage[32];
	jsstr16_t out;
	jsmethod_error_t error;
	callback_ctx_t throw_ctx = {1, NULL, 0};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));

	assert(jsmethod_value_to_string(&out, jsmethod_value_undefined(), 1, &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_value_to_string(&out, jsmethod_value_symbol(), 0, &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_value_to_string(&out,
			jsmethod_value_coercible(&throw_ctx, callback_to_string),
			0, &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);
}

static void
test_string_normalize_method(void)
{
	static const uint8_t receiver_utf8[] = "A\xCC\x8A";
	static const uint16_t expected_nfc[] = {0x00C5};
	static const uint16_t expected_nfd[] = {0x0041, 0x030A};
	static const uint16_t nfd_form[] = {'N', 'F', 'D'};
	static const uint16_t receiver_u16[] = {0x1E9B, 0x0323};
	uint16_t storage[32];
	uint16_t form_storage[8];
	uint32_t workspace[5];
	jsstr16_t out;
	jsmethod_error_t error;
	callback_ctx_t form_ctx = {0, nfd_form, sizeof(nfd_form) / sizeof(nfd_form[0])};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	assert(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			0, jsmethod_value_undefined(),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == 0);
	expect_utf16(&out, expected_nfc, sizeof(expected_nfc) / sizeof(expected_nfc[0]));

	out.len = 0;
	assert(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf16(receiver_u16,
				sizeof(receiver_u16) / sizeof(receiver_u16[0])),
			1, jsmethod_value_coercible(&form_ctx, callback_to_string),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == 0);
	expect_utf16(&out, (const uint16_t[]){0x017F, 0x0323, 0x0307}, 3);

	out.len = 0;
	assert(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			1, jsmethod_value_string_utf16(expected_nfd,
				sizeof(expected_nfd) / sizeof(expected_nfd[0])),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_RANGE);
}

static void
test_string_normalize_measure(void)
{
	static const uint8_t receiver_utf8[] = "A\xCC\x8A";
	static const uint16_t receiver_u16[] = {0x1E9B, 0x0323};
	static const uint16_t nfd_form[] = {'N', 'F', 'D'};
	uint16_t out_storage[3];
	jsstr16_t out;
	jsmethod_error_t error;
	jsmethod_string_normalize_sizes_t sizes;
	callback_ctx_t form_ctx = {0, nfd_form, sizeof(nfd_form) / sizeof(nfd_form[0])};

	assert(jsmethod_string_normalize_measure(
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			0, jsmethod_value_undefined(), &sizes, &error) == 0);
	assert(sizes.this_storage_len == 2);
	assert(sizes.form_storage_len == 0);
	assert(sizes.workspace_len == 4);
	assert(sizes.result_len == 1);

	jsstr16_init_from_buf(&out, (const char *)out_storage,
			sizes.result_len * sizeof(out_storage[0]));
	{
		uint16_t this_storage[sizes.this_storage_len];
		uint32_t workspace[sizes.workspace_len];

		assert(jsmethod_string_normalize_into(&out,
				jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
				this_storage, sizes.this_storage_len,
				0, jsmethod_value_undefined(),
				NULL, 0,
				workspace, sizes.workspace_len,
				&error) == 0);
	}
	expect_utf16(&out, (const uint16_t[]){0x00C5}, 1);

	assert(jsmethod_string_normalize_measure(
			jsmethod_value_string_utf16(receiver_u16,
				sizeof(receiver_u16) / sizeof(receiver_u16[0])),
			1, jsmethod_value_string_utf16(nfd_form,
				sizeof(nfd_form) / sizeof(nfd_form[0])),
			&sizes, &error) == 0);
	assert(sizes.this_storage_len == 2);
	assert(sizes.form_storage_len == 3);
	assert(sizes.workspace_len == 5);
	assert(sizes.result_len == 3);

	errno = 0;
	assert(jsmethod_string_normalize_measure(
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			1, jsmethod_value_coercible(&form_ctx, callback_to_string),
			&sizes, &error) == -1);
	assert(errno == ENOTSUP);
	assert(error.kind == JSMETHOD_ERROR_NONE);
}

static void
test_string_normalize_errors(void)
{
	static const uint8_t receiver_utf8[] = "foo";
	static const uint16_t nfc_form[] = {'N', 'F', 'C'};
	uint16_t storage[32];
	uint16_t form_storage[8];
	uint32_t workspace[64];
	jsstr16_t out;
	jsmethod_error_t error;
	callback_ctx_t throw_ctx = {1, NULL, 0};
	callback_ctx_t ok_form_ctx = {0, nfc_form, sizeof(nfc_form) / sizeof(nfc_form[0])};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));

	assert(jsmethod_string_normalize(&out, jsmethod_value_null(), 0,
			jsmethod_value_undefined(), form_storage,
			sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_normalize(&out, jsmethod_value_symbol(), 0,
			jsmethod_value_undefined(), form_storage,
			sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			1, jsmethod_value_symbol(), form_storage,
			sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_normalize(&out,
			jsmethod_value_coercible(&throw_ctx, callback_to_string),
			1, jsmethod_value_coercible(&ok_form_ctx, callback_to_string),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);

	assert(jsmethod_string_normalize(&out,
			jsmethod_value_string_utf8(receiver_utf8, sizeof(receiver_utf8) - 1),
			1, jsmethod_value_coercible(&throw_ctx, callback_to_string),
			form_storage, sizeof(form_storage) / sizeof(form_storage[0]),
			workspace, sizeof(workspace) / sizeof(workspace[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);
}

static void
test_string_case_methods(void)
{
	static const uint8_t mixed_ascii[] = "Hello, WoRlD!";
	static const uint8_t turkish_i[] = "i";
	static const uint8_t turkish_capital_i[] = "I";
	static const uint16_t turkish_locale[] = {'t', 'r'};
	uint16_t storage[64];
	uint16_t locale_storage[8];
	jsstr16_t out;
	jsmethod_error_t error;
	callback_ctx_t locale_ctx = {
		0,
		turkish_locale,
		sizeof(turkish_locale) / sizeof(turkish_locale[0]),
	};
	callback_ctx_t throw_ctx = {1, NULL, 0};

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));

	assert(jsmethod_string_to_upper_case(&out,
			jsmethod_value_string_utf8(mixed_ascii, sizeof(mixed_ascii) - 1),
			&error) == 0);
	expect_utf16(&out, (const uint16_t[]){
		'H', 'E', 'L', 'L', 'O', ',', ' ', 'W', 'O', 'R', 'L', 'D', '!'
	}, 13);

	out.len = 0;
	assert(jsmethod_string_to_lower_case(&out,
			jsmethod_value_bool(1), &error) == 0);
	expect_utf16(&out, (const uint16_t[]){'t', 'r', 'u', 'e'}, 4);

	out.len = 0;
	assert(jsmethod_string_to_locale_upper_case(&out,
			jsmethod_value_string_utf8(turkish_i, sizeof(turkish_i) - 1),
			1, jsmethod_value_coercible(&locale_ctx, callback_to_string),
			locale_storage, sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) == 0);
	expect_utf16(&out, (const uint16_t[]){0x0130}, 1);

	out.len = 0;
	assert(jsmethod_string_to_locale_lower_case(&out,
			jsmethod_value_string_utf8(turkish_capital_i,
				sizeof(turkish_capital_i) - 1),
			1, jsmethod_value_string_utf8(
				(const uint8_t *)"tr", 2),
			locale_storage, sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) == 0);
	expect_utf16(&out, (const uint16_t[]){0x0131}, 1);

	assert(jsmethod_string_to_upper_case(&out,
			jsmethod_value_symbol(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_to_locale_upper_case(&out,
			jsmethod_value_string_utf8(turkish_i, sizeof(turkish_i) - 1),
			1, jsmethod_value_symbol(), locale_storage,
			sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_to_locale_lower_case(&out,
			jsmethod_value_string_utf8(turkish_capital_i,
				sizeof(turkish_capital_i) - 1),
			1, jsmethod_value_coercible(&throw_ctx, callback_to_string),
			locale_storage, sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);
}

static void
test_well_formed_methods(void)
{
	static const uint16_t lone_leading[] = {'a', 0xD83D, 'b'};
	static const uint16_t lone_expected[] = {'a', 0xFFFD, 'b'};
	static const uint16_t pair[] = {0xD83D, 0xDCA9};
	uint16_t storage[16];
	uint16_t check_storage[16];
	jsstr16_t out;
	jsmethod_error_t error;
	int is_well_formed;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));

	assert(jsmethod_string_to_well_formed(&out,
			jsmethod_value_string_utf16(lone_leading,
				sizeof(lone_leading) / sizeof(lone_leading[0])),
			&error) == 0);
	expect_utf16(&out, lone_expected,
			sizeof(lone_expected) / sizeof(lone_expected[0]));

	assert(jsmethod_string_is_well_formed(&is_well_formed,
			jsmethod_value_string_utf16(pair,
				sizeof(pair) / sizeof(pair[0])),
			check_storage, sizeof(check_storage) / sizeof(check_storage[0]),
			&error) == 0);
	assert(is_well_formed == 1);

	assert(jsmethod_string_is_well_formed(&is_well_formed,
			jsmethod_value_string_utf16(lone_leading,
				sizeof(lone_leading) / sizeof(lone_leading[0])),
			check_storage, sizeof(check_storage) / sizeof(check_storage[0]),
			&error) == 0);
	assert(is_well_formed == 0);

	assert(jsmethod_string_to_well_formed(&out,
			jsmethod_value_null(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
}

static void
test_string_search_methods(void)
{
	static const uint8_t bananas_utf8[] = "bananas";
	static const uint8_t needle_utf8[] = "na";
	static const uint8_t missing_utf8[] = "zz";
	uint16_t storage[16];
	jsstr16_t probe;
	jsmethod_error_t error;
	ssize_t index;
	int result;
	callback_ctx_t throw_ctx = {1, NULL, 0};

	jsstr16_init_from_buf(&probe, (const char *)storage, sizeof(storage));

	assert(jsmethod_string_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == 0);
	assert(index == 2);

	assert(jsmethod_string_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			1, jsmethod_value_number(3.0), &error) == 0);
	assert(index == 4);

	assert(jsmethod_string_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(missing_utf8,
				sizeof(missing_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == 0);
	assert(index == -1);

	assert(jsmethod_string_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			1, jsmethod_value_number(99.0), &error) == 0);
	assert(index == 7);

	assert(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == 0);
	assert(index == 4);

	assert(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			1, jsmethod_value_number(3.0), &error) == 0);
	assert(index == 2);

	assert(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			1, jsmethod_value_number(NAN), &error) == 0);
	assert(index == 4);

	assert(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			1, jsmethod_value_number(99.0), &error) == 0);
	assert(index == 7);

	assert(jsmethod_string_includes(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			1, jsmethod_value_number(3.0), &error) == 0);
	assert(result == 1);

	assert(jsmethod_string_includes(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(missing_utf8,
				sizeof(missing_utf8) - 1),
			1, jsmethod_value_number(0.0), &error) == 0);
	assert(result == 0);

	assert(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			1, jsmethod_value_number(2.0), &error) == 0);
	assert(result == 1);

	assert(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			1, jsmethod_value_number(99.0), &error) == 0);
	assert(result == 1);

	assert(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"na", 2),
			1, jsmethod_value_number(4.0), &error) == 0);
	assert(result == 1);

	assert(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"ba", 2),
			1, jsmethod_value_number(NAN), &error) == 0);
	assert(result == 0);

	assert(jsmethod_string_includes(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_symbol(),
			0, jsmethod_value_undefined(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_index_of(&index, jsmethod_value_null(),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsmethod_string_includes(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_coercible(&throw_ctx, callback_to_string),
			0, jsmethod_value_undefined(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);

	assert(jsmethod_string_starts_with(&result,
			jsmethod_value_coercible(&throw_ctx, callback_to_string),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);

	(void)probe;
}

int
main(void)
{
	test_value_to_string_primitives();
	test_value_to_string_errors();
	test_string_normalize_method();
	test_string_normalize_measure();
	test_string_normalize_errors();
	test_string_case_methods();
	test_well_formed_methods();
	test_string_search_methods();
	return 0;
}
