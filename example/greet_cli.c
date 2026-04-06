#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsval.h"

/*
 * Source JS: example/greet_cli.js
 * Behavior: Node-style CLI script using process.argv and stdout to produce a
 * greeting line after trim/replaceAll/startsWith processing.
 * Lowering: production_slow_path
 * Host assumptions: process.argv is bridged from argc/argv with the original
 * JS entrypoint baked in as process.argv[1].
 */

static int
fail_errno(const char *context)
{
	fprintf(stderr, "%s: %s\n", context, strerror(errno));
	return 1;
}

static int
fail_jsmethod(const char *context, jsmethod_error_t *error)
{
	fprintf(stderr, "%s: jsmethod error kind=%d errno=%d\n",
			context, error != NULL ? (int)error->kind : -1, errno);
	return 1;
}

static int
build_process_argv(jsval_region_t *region, int argc, char **argv,
		const char *script_path, jsval_t *value_ptr)
{
	jsval_t array;
	jsval_t entry;
	int i;

	if (jsval_array_new(region, (size_t)argc + 1u, &array) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)argv[0],
			strlen(argv[0]), &entry) < 0) {
		return -1;
	}
	if (jsval_array_push(region, array, entry) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)script_path,
			strlen(script_path), &entry) < 0) {
		return -1;
	}
	if (jsval_array_push(region, array, entry) < 0) {
		return -1;
	}
	for (i = 1; i < argc; i++) {
		if (jsval_string_new_utf8(region, (const uint8_t *)argv[i],
				strlen(argv[i]), &entry) < 0) {
			return -1;
		}
		if (jsval_array_push(region, array, entry) < 0) {
			return -1;
		}
	}
	*value_ptr = array;
	return 0;
}

static int
write_js_string(FILE *stream, jsval_region_t *region, jsval_t value)
{
	uint8_t *buf = NULL;
	size_t len = 0;
	int rc = 1;

	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return -1;
	}
	if (len > 0) {
		buf = (uint8_t *)malloc(len);
		if (buf == NULL) {
			errno = ENOMEM;
			return -1;
		}
		if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0) {
			goto done;
		}
		if (fwrite(buf, 1, len, stream) != len) {
			errno = EIO;
			goto done;
		}
	}
	rc = 0;

done:
	free(buf);
	return rc;
}

int
main(int argc, char **argv)
{
	static const char script_path[] = "example/greet_cli.js";
	static const uint8_t default_raw_text[] = "  world  ";
	static const uint8_t default_mode_text[] = "plain";
	static const uint8_t hyphen_text[] = "-";
	static const uint8_t space_text[] = " ";
	static const uint8_t prefix_probe_text[] = "jsmx";
	static const uint8_t runtime_text[] = "runtime";
	static const uint8_t hello_text[] = "hello";
	static const uint8_t excited_text[] = "excited";
	static const uint8_t bang_text[] = "!";
	static const uint8_t period_text[] = ".";
	static const uint8_t comma_space_text[] = ", ";
	static const uint8_t newline_text[] = "\n";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t process_argv;
	jsval_t raw_value;
	jsval_t mode_value;
	jsval_t default_raw;
	jsval_t default_mode;
	jsval_t hyphen;
	jsval_t space;
	jsval_t prefix_probe;
	jsval_t runtime_prefix;
	jsval_t hello_prefix;
	jsval_t excited_mode;
	jsval_t bang;
	jsval_t period;
	jsval_t comma_space;
	jsval_t newline;
	jsval_t trimmed;
	jsval_t phrase;
	jsval_t starts_with_value;
	jsval_t prefix_value;
	jsval_t suffix_value;
	jsval_t concat_args[3];
	jsval_t line;
	jsval_t output;
	jsmethod_error_t error = {0};

	jsval_region_init(&region, storage, sizeof(storage));
	if (build_process_argv(&region, argc, argv, script_path, &process_argv) < 0) {
		return fail_errno("build_process_argv");
	}
	if (jsval_array_get(&region, process_argv, 2, &raw_value) < 0) {
		return fail_errno("jsval_array_get(raw)");
	}
	if (jsval_array_get(&region, process_argv, 3, &mode_value) < 0) {
		return fail_errno("jsval_array_get(mode)");
	}
	if (jsval_string_new_utf8(&region, default_raw_text,
			sizeof(default_raw_text) - 1, &default_raw) < 0) {
		return fail_errno("jsval_string_new_utf8(default_raw)");
	}
	if (jsval_string_new_utf8(&region, default_mode_text,
			sizeof(default_mode_text) - 1, &default_mode) < 0) {
		return fail_errno("jsval_string_new_utf8(default_mode)");
	}
	if (jsval_is_nullish(raw_value)) {
		raw_value = default_raw;
	}
	if (jsval_is_nullish(mode_value)) {
		mode_value = default_mode;
	}
	if (jsval_string_new_utf8(&region, hyphen_text, sizeof(hyphen_text) - 1,
			&hyphen) < 0) {
		return fail_errno("jsval_string_new_utf8(hyphen)");
	}
	if (jsval_string_new_utf8(&region, space_text, sizeof(space_text) - 1,
			&space) < 0) {
		return fail_errno("jsval_string_new_utf8(space)");
	}
	if (jsval_string_new_utf8(&region, prefix_probe_text,
			sizeof(prefix_probe_text) - 1, &prefix_probe) < 0) {
		return fail_errno("jsval_string_new_utf8(prefix_probe)");
	}
	if (jsval_string_new_utf8(&region, runtime_text,
			sizeof(runtime_text) - 1, &runtime_prefix) < 0) {
		return fail_errno("jsval_string_new_utf8(runtime)");
	}
	if (jsval_string_new_utf8(&region, hello_text, sizeof(hello_text) - 1,
			&hello_prefix) < 0) {
		return fail_errno("jsval_string_new_utf8(hello)");
	}
	if (jsval_string_new_utf8(&region, excited_text,
			sizeof(excited_text) - 1, &excited_mode) < 0) {
		return fail_errno("jsval_string_new_utf8(excited)");
	}
	if (jsval_string_new_utf8(&region, bang_text, sizeof(bang_text) - 1,
			&bang) < 0) {
		return fail_errno("jsval_string_new_utf8(bang)");
	}
	if (jsval_string_new_utf8(&region, period_text, sizeof(period_text) - 1,
			&period) < 0) {
		return fail_errno("jsval_string_new_utf8(period)");
	}
	if (jsval_string_new_utf8(&region, comma_space_text,
			sizeof(comma_space_text) - 1, &comma_space) < 0) {
		return fail_errno("jsval_string_new_utf8(comma_space)");
	}
	if (jsval_string_new_utf8(&region, newline_text,
			sizeof(newline_text) - 1, &newline) < 0) {
		return fail_errno("jsval_string_new_utf8(newline)");
	}
	if (jsval_method_string_trim(&region, raw_value, &trimmed, &error) < 0) {
		return fail_jsmethod("jsval_method_string_trim", &error);
	}
	if (jsval_method_string_replace_all(&region, trimmed, hyphen, space, &phrase,
			&error) < 0) {
		return fail_jsmethod("jsval_method_string_replace_all", &error);
	}
	if (jsval_method_string_starts_with(&region, phrase, prefix_probe, 0,
			jsval_undefined(), &starts_with_value, &error) < 0) {
		return fail_jsmethod("jsval_method_string_starts_with", &error);
	}
	prefix_value = jsval_truthy(&region, starts_with_value)
			? runtime_prefix : hello_prefix;
	suffix_value = jsval_strict_eq(&region, mode_value, excited_mode)
			? bang : period;

	concat_args[0] = comma_space;
	concat_args[1] = phrase;
	concat_args[2] = suffix_value;
	if (jsval_method_string_concat(&region, prefix_value, 3, concat_args, &line,
			&error) < 0) {
		return fail_jsmethod("jsval_method_string_concat(line)", &error);
	}
	if (jsval_method_string_concat(&region, line, 1, &newline, &output,
			&error) < 0) {
		return fail_jsmethod("jsval_method_string_concat(output)", &error);
	}
	if (write_js_string(stdout, &region, output) < 0) {
		return fail_errno("write_js_string");
	}

	return 0;
}
