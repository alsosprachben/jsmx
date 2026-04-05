#ifndef JSMX_BENCH_UTIL_H
#define JSMX_BENCH_UTIL_H

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "jsval.h"

static int
bench_fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int
bench_fail_errno(const char *context)
{
	fprintf(stderr, "%s: %s\n", context, strerror(errno));
	return 1;
}

static int
bench_append_dense_array(jsval_region_t *region, jsval_t dst, jsval_t src)
{
	size_t len;
	size_t i;

	if (region == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_array_length(region, src);
	for (i = 0; i < len; i++) {
		jsval_t value;

		if (jsval_array_get(region, src, i, &value) < 0) {
			return -1;
		}
		if (jsval_array_push(region, dst, value) < 0) {
			return -1;
		}
	}
	return 0;
}

static int
bench_print_i64(int64_t value)
{
	return printf("%" PRId64 "\n", value) < 0 ? 1 : 0;
}

static int
bench_print_u64(uint64_t value)
{
	return printf("%" PRIu64 "\n", value) < 0 ? 1 : 0;
}

#endif
