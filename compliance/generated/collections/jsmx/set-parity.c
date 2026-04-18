#include <math.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "collections"
#define CASE_NAME "jsmx/set-parity"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t set;
	jsval_t key_a;
	size_t size = 0;
	int has = 0;
	int removed = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_set_new(&region, 4, &set) == 0 &&
			set.kind == JSVAL_KIND_SET,
			SUITE, CASE_NAME, "set_new failed");
	GENERATED_TEST_ASSERT(
			jsval_set_size(&region, set, &size) == 0 && size == 0,
			SUITE, CASE_NAME, "initial size != 0");

	/* Add a string key; presence + size. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&key_a) == 0,
			SUITE, CASE_NAME, "key_a setup");
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, key_a) == 0,
			SUITE, CASE_NAME, "add 'a'");
	GENERATED_TEST_ASSERT(
			jsval_set_has(&region, set, key_a, &has) == 0 && has == 1,
			SUITE, CASE_NAME, "has 'a' false");
	GENERATED_TEST_ASSERT(
			jsval_set_size(&region, set, &size) == 0 && size == 1,
			SUITE, CASE_NAME, "size != 1 after add");

	/* Duplicate add is a no-op. */
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, key_a) == 0 &&
			jsval_set_size(&region, set, &size) == 0 && size == 1,
			SUITE, CASE_NAME, "duplicate add changed size");

	/* NaN dedupe: all NaN bit patterns are treated as a single
	 * key. */
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, jsval_number(NAN)) == 0,
			SUITE, CASE_NAME, "add NaN");
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, jsval_number(NAN)) == 0 &&
			jsval_set_size(&region, set, &size) == 0 && size == 2,
			SUITE, CASE_NAME, "NaN not deduped (size should be 2)");
	GENERATED_TEST_ASSERT(
			jsval_set_has(&region, set, jsval_number(NAN), &has) == 0 &&
			has == 1,
			SUITE, CASE_NAME, "has(NaN) failed");

	/* -0 and +0 are the same key (sameValueZero). */
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, jsval_number(-0.0)) == 0 &&
			jsval_set_has(&region, set, jsval_number(0.0), &has) == 0 &&
			has == 1,
			SUITE, CASE_NAME, "has(+0) after add(-0) should be true");

	/* Delete returns 1 on success, 0 if absent. */
	GENERATED_TEST_ASSERT(
			jsval_set_delete(&region, set, key_a, &removed) == 0 &&
			removed == 1,
			SUITE, CASE_NAME, "delete 'a' should return 1");
	GENERATED_TEST_ASSERT(
			jsval_set_has(&region, set, key_a, &has) == 0 && has == 0,
			SUITE, CASE_NAME, "has 'a' after delete");
	GENERATED_TEST_ASSERT(
			jsval_set_delete(&region, set, key_a, &removed) == 0 &&
			removed == 0,
			SUITE, CASE_NAME, "second delete 'a' should return 0");

	/* Clear empties the set. */
	GENERATED_TEST_ASSERT(
			jsval_set_clear(&region, set) == 0 &&
			jsval_set_size(&region, set, &size) == 0 && size == 0,
			SUITE, CASE_NAME, "clear failed");

	return generated_test_pass(SUITE, CASE_NAME);
}
