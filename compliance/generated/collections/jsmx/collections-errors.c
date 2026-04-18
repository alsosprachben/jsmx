#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "collections"
#define CASE_NAME "jsmx/collections-errors"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t set;
	jsval_t grown_set;
	jsval_t map;
	jsval_t grown_map;
	jsval_t k[3];
	size_t size = 0;
	int has = 0;
	jsval_t out;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Set: fill cap=2, third add returns -1 ENOBUFS. */
	GENERATED_TEST_ASSERT(
			jsval_set_new(&region, 2, &set) == 0,
			SUITE, CASE_NAME, "set_new(cap=2)");
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
				&k[0]) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
				&k[1]) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"z", 1,
				&k[2]) == 0,
			SUITE, CASE_NAME, "key setup");
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, k[0]) == 0 &&
			jsval_set_add(&region, set, k[1]) == 0,
			SUITE, CASE_NAME, "fill set to cap");
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, set, k[2]) < 0 &&
			errno == ENOBUFS,
			SUITE, CASE_NAME, "over-cap add should be ENOBUFS");

	/* Clone to a larger capacity; the third add succeeds there. */
	GENERATED_TEST_ASSERT(
			jsval_set_clone(&region, set, 4, &grown_set) == 0 &&
			jsval_set_size(&region, grown_set, &size) == 0 && size == 2,
			SUITE, CASE_NAME, "clone preserves existing entries");
	GENERATED_TEST_ASSERT(
			jsval_set_add(&region, grown_set, k[2]) == 0 &&
			jsval_set_size(&region, grown_set, &size) == 0 && size == 3,
			SUITE, CASE_NAME, "add into grown set");

	/* Map: same capacity shape. */
	GENERATED_TEST_ASSERT(
			jsval_map_new(&region, 2, &map) == 0,
			SUITE, CASE_NAME, "map_new(cap=2)");
	GENERATED_TEST_ASSERT(
			jsval_map_set(&region, map, k[0], jsval_number(1.0)) == 0 &&
			jsval_map_set(&region, map, k[1], jsval_number(2.0)) == 0,
			SUITE, CASE_NAME, "fill map to cap");
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_map_set(&region, map, k[2], jsval_number(3.0)) < 0 &&
			errno == ENOBUFS,
			SUITE, CASE_NAME, "over-cap map_set should be ENOBUFS");

	GENERATED_TEST_ASSERT(
			jsval_map_clone(&region, map, 4, &grown_map) == 0 &&
			jsval_map_set(&region, grown_map, k[2],
				jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "map_clone grows capacity");

	/* Accessors on a non-Set / non-Map receiver fail. */
	GENERATED_TEST_ASSERT(
			jsval_set_has(&region, jsval_number(0.0), k[0], &has) < 0,
			SUITE, CASE_NAME, "set_has on number should fail");
	GENERATED_TEST_ASSERT(
			jsval_map_get(&region, jsval_undefined(), k[0], &out) < 0,
			SUITE, CASE_NAME, "map_get on undefined should fail");

	return generated_test_pass(SUITE, CASE_NAME);
}
