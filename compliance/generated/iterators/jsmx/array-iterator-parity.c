#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "iterators"
#define CASE_NAME "jsmx/array-iterator-parity"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t array;
	jsval_t empty;
	jsval_t iterator;
	jsval_t key;
	jsval_t value;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Build [10, 20, 30]. */
	GENERATED_TEST_ASSERT(
			jsval_array_new(&region, 3, &array) == 0 &&
			jsval_array_set(&region, array, 0, jsval_number(10.0)) == 0 &&
			jsval_array_set(&region, array, 1, jsval_number(20.0)) == 0 &&
			jsval_array_set(&region, array, 2, jsval_number(30.0)) == 0,
			SUITE, CASE_NAME, "build array");

	/* DEFAULT selector: yields values. */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, array,
				JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "get DEFAULT iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 &&
			value.kind == JSVAL_KIND_NUMBER && value.as.number == 10.0,
			SUITE, CASE_NAME, "DEFAULT step 0 != 10");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 20.0,
			SUITE, CASE_NAME, "DEFAULT step 1 != 20");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 30.0,
			SUITE, CASE_NAME, "DEFAULT step 2 != 30");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "DEFAULT not done after 3 steps");

	/* KEYS selector: yields indices. */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, array,
				JSVAL_ITERATOR_SELECTOR_KEYS, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "get KEYS iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 0.0,
			SUITE, CASE_NAME, "KEYS step 0 != 0");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 1.0,
			SUITE, CASE_NAME, "KEYS step 1 != 1");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 2.0,
			SUITE, CASE_NAME, "KEYS step 2 != 2");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "KEYS not done after 3 steps");

	/* VALUES selector: yields values (same as DEFAULT for arrays). */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, array,
				JSVAL_ITERATOR_SELECTOR_VALUES, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "get VALUES iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 0 && value.as.number == 10.0,
			SUITE, CASE_NAME, "VALUES step 0 != 10");

	/* ENTRIES selector: yields (index, value). Generic _next
	 * returns -1; must use _next_entry. */
	GENERATED_TEST_ASSERT(
			jsval_get_iterator(&region, array,
				JSVAL_ITERATOR_SELECTOR_ENTRIES, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "get ENTRIES iterator");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) < 0,
			SUITE, CASE_NAME,
			"expected _next on ENTRIES iterator to fail");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 0 &&
			key.as.number == 0.0 && value.as.number == 10.0,
			SUITE, CASE_NAME, "ENTRIES step 0 != (0, 10)");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 0 &&
			key.as.number == 1.0 && value.as.number == 20.0,
			SUITE, CASE_NAME, "ENTRIES step 1 != (1, 20)");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 0 &&
			key.as.number == 2.0 && value.as.number == 30.0,
			SUITE, CASE_NAME, "ENTRIES step 2 != (2, 30)");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) == 0 && done == 1,
			SUITE, CASE_NAME, "ENTRIES not done after 3 steps");

	/* Empty array: done immediately. */
	GENERATED_TEST_ASSERT(
			jsval_array_new(&region, 0, &empty) == 0 &&
			jsval_get_iterator(&region, empty,
				JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator,
				&error) == 0,
			SUITE, CASE_NAME, "empty iterator setup");
	GENERATED_TEST_ASSERT(
			jsval_iterator_next(&region, iterator, &done, &value,
				&error) == 0 && done == 1,
			SUITE, CASE_NAME, "empty iterator not done immediately");

	return generated_test_pass(SUITE, CASE_NAME);
}
