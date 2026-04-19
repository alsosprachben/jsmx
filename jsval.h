#ifndef JSVAL_H
#define JSVAL_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"
#include "jsmethod.h"
#include "jsmn.h"

typedef uint32_t jsval_off_t;

#define JSVAL_PAGES_MAGIC 0x4a535650u
#define JSVAL_PAGES_VERSION 1u

typedef enum jsval_repr_e {
	JSVAL_REPR_INLINE = 0,
	JSVAL_REPR_NATIVE = 1,
	JSVAL_REPR_JSON = 2
} jsval_repr_t;

typedef enum jsval_kind_e {
	JSVAL_KIND_UNDEFINED = 0,
	JSVAL_KIND_NULL = 1,
	JSVAL_KIND_BOOL = 2,
	JSVAL_KIND_NUMBER = 3,
	JSVAL_KIND_STRING = 4,
	JSVAL_KIND_OBJECT = 5,
	JSVAL_KIND_ARRAY = 6,
	JSVAL_KIND_REGEXP = 7,
	JSVAL_KIND_MATCH_ITERATOR = 8,
	JSVAL_KIND_URL = 9,
	JSVAL_KIND_URL_SEARCH_PARAMS = 10,
	JSVAL_KIND_SET = 11,
	JSVAL_KIND_MAP = 12,
	JSVAL_KIND_ITERATOR = 13,
	JSVAL_KIND_SYMBOL = 14,
	JSVAL_KIND_BIGINT = 15,
	JSVAL_KIND_FUNCTION = 16,
	JSVAL_KIND_DATE = 17,
	JSVAL_KIND_ARRAY_BUFFER = 18,
	JSVAL_KIND_TYPED_ARRAY = 19,
	JSVAL_KIND_CRYPTO = 20,
	JSVAL_KIND_SUBTLE_CRYPTO = 21,
	JSVAL_KIND_CRYPTO_KEY = 22,
	JSVAL_KIND_DOM_EXCEPTION = 23,
	JSVAL_KIND_PROMISE = 24,
	JSVAL_KIND_HEADERS = 25,
	JSVAL_KIND_REQUEST = 26,
	JSVAL_KIND_RESPONSE = 27,
	JSVAL_KIND_READABLE_STREAM = 28,
	JSVAL_KIND_READABLE_STREAM_READER = 29,
	JSVAL_KIND_WRITABLE_STREAM = 30,
	JSVAL_KIND_WRITABLE_STREAM_DEFAULT_WRITER = 31,
	JSVAL_KIND_TRANSFORM_STREAM = 32
} jsval_kind_t;

typedef enum jsval_typed_array_kind_e {
	JSVAL_TYPED_ARRAY_INT8 = 0,
	JSVAL_TYPED_ARRAY_UINT8 = 1,
	JSVAL_TYPED_ARRAY_UINT8_CLAMPED = 2,
	JSVAL_TYPED_ARRAY_INT16 = 3,
	JSVAL_TYPED_ARRAY_UINT16 = 4,
	JSVAL_TYPED_ARRAY_INT32 = 5,
	JSVAL_TYPED_ARRAY_UINT32 = 6,
	JSVAL_TYPED_ARRAY_BIGINT64 = 7,
	JSVAL_TYPED_ARRAY_BIGUINT64 = 8,
	JSVAL_TYPED_ARRAY_FLOAT32 = 9,
	JSVAL_TYPED_ARRAY_FLOAT64 = 10
} jsval_typed_array_kind_t;

typedef enum jsval_crypto_key_type_e {
	JSVAL_CRYPTO_KEY_TYPE_SECRET = 0,
	JSVAL_CRYPTO_KEY_TYPE_PUBLIC = 1,
	JSVAL_CRYPTO_KEY_TYPE_PRIVATE = 2
} jsval_crypto_key_type_t;

typedef enum jsval_crypto_key_usage_e {
	JSVAL_CRYPTO_KEY_USAGE_SIGN = 1u << 0,
	JSVAL_CRYPTO_KEY_USAGE_VERIFY = 1u << 1,
	JSVAL_CRYPTO_KEY_USAGE_ENCRYPT = 1u << 2,
	JSVAL_CRYPTO_KEY_USAGE_DECRYPT = 1u << 3,
	JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS = 1u << 4,
	JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY = 1u << 5,
	JSVAL_CRYPTO_KEY_USAGE_WRAP_KEY = 1u << 6,
	JSVAL_CRYPTO_KEY_USAGE_UNWRAP_KEY = 1u << 7
} jsval_crypto_key_usage_t;

typedef enum jsval_iterator_selector_e {
	JSVAL_ITERATOR_SELECTOR_DEFAULT = 0,
	JSVAL_ITERATOR_SELECTOR_KEYS = 1,
	JSVAL_ITERATOR_SELECTOR_VALUES = 2,
	JSVAL_ITERATOR_SELECTOR_ENTRIES = 3
} jsval_iterator_selector_t;

typedef enum jsval_promise_state_e {
	JSVAL_PROMISE_STATE_PENDING = 0,
	JSVAL_PROMISE_STATE_FULFILLED = 1,
	JSVAL_PROMISE_STATE_REJECTED = 2
} jsval_promise_state_t;

typedef struct jsval_s {
	uint8_t kind;
	uint8_t repr;
	uint16_t reserved;
	jsval_off_t off;
	union {
		int boolean;
		double number;
		uint32_t index;
	} as;
} jsval_t;

typedef struct jsval_pages_s {
	uint32_t magic;
	uint16_t version;
	uint16_t header_size;
	uint32_t total_len;
	uint32_t used_len;
	jsval_t root;
} jsval_pages_t;

struct jsval_region_s;
typedef struct jsval_region_s jsval_region_t;

typedef void (*jsval_scheduler_notify_fn)(jsval_region_t *region, void *ctx);

typedef struct jsval_scheduler_s {
	void *ctx;
	jsval_scheduler_notify_fn on_enqueue;
	jsval_scheduler_notify_fn on_wake;
} jsval_scheduler_t;

/*
 * Outbound fetch transport seam.
 *
 * An embedder (today: mnvkd's vk_jsmx_fetch_transport) implements this
 * vtable and registers it on a region via jsval_region_set_fetch_transport.
 * When hosted JS calls fetch(), jsval_fetch delegates to the transport
 * instead of returning a rejected stub Promise.
 *
 *   begin:  Allocate opaque in-flight state for `request_value`. The
 *           transport copies any bytes it needs out of the Request before
 *           returning; `request_value` is not retained.
 *           Returns 0 on success (with *state_out set), -1 with errno set
 *           on failure.
 *
 *   poll:   Drive the in-flight request forward by one step. Three outcomes:
 *             PENDING: transport blocked on I/O. The microtask stays
 *                      parked; the drain re-polls on the next pump.
 *                      (Future: integrates with jsval_scheduler_s so the
 *                      embedder can yield its coroutine until wake.)
 *             READY  : *response_out holds a JSVAL_KIND_RESPONSE
 *                      built by the transport via jsval_response_new_*.
 *                      The state is freed by the transport internally.
 *             ERROR  : *reason_out holds a rejection reason (typically
 *                      a DOMException built via jsval_dom_exception_new_*).
 *                      The state is freed by the transport internally.
 *           Returns 0 on any of the three outcomes, -1 with errno on
 *           fatal failure (treated as a TypeError by the drain).
 *
 *   cancel: Free `state` early, e.g. on region tear-down or abort. May be
 *           NULL if the transport has no cancellation path.
 */

typedef enum jsval_fetch_transport_status_e {
	JSVAL_FETCH_TRANSPORT_STATUS_PENDING = 0,
	JSVAL_FETCH_TRANSPORT_STATUS_READY = 1,
	JSVAL_FETCH_TRANSPORT_STATUS_ERROR = 2
} jsval_fetch_transport_status_t;

typedef struct jsval_fetch_transport_s {
	int (*begin)(void *userdata, jsval_region_t *region,
			jsval_t request_value, void **state_out);
	int (*poll)(void *userdata, jsval_region_t *region, void *state,
			jsval_fetch_transport_status_t *status_out,
			jsval_t *response_out, jsval_t *reason_out);
	void (*cancel)(void *userdata, void *state);
} jsval_fetch_transport_t;

typedef struct jsval_region_s {
	uint8_t *base;
	size_t len;
	size_t used;
	jsval_pages_t *pages;
	jsval_off_t microtask_head;
	jsval_off_t microtask_tail;
	size_t microtask_count;
	uint8_t microtask_draining;
	uint8_t fetch_waitlist_enabled;
	uint8_t reserved[6];
	jsval_scheduler_t scheduler;
	const jsval_fetch_transport_t *fetch_transport;
	void *fetch_transport_userdata;
	jsval_off_t fetch_waitlist_head;
	jsval_off_t fetch_waitlist_tail;
	size_t fetch_waitlist_count;
	jsval_off_t promise_combinator_head;
} jsval_region_t;

typedef int (*jsval_native_function_fn)(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr,
		jsmethod_error_t *error);

#if JSMX_WITH_REGEX
typedef struct jsval_regexp_info_s {
	uint32_t flags;
	uint32_t capture_count;
	size_t last_index;
} jsval_regexp_info_t;
#endif

typedef struct jsval_replace_call_s {
	jsval_t match;
	size_t capture_count;
	const jsval_t *captures;
	size_t offset;
	jsval_t input;
	jsval_t groups;
} jsval_replace_call_t;

typedef int (*jsval_replace_callback_fn)(jsval_region_t *region, void *ctx,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error);

void jsval_region_init(jsval_region_t *region, void *buf, size_t len);
void jsval_region_rebase(jsval_region_t *region, void *buf, size_t len);
void jsval_region_set_scheduler(jsval_region_t *region,
		const jsval_scheduler_t *scheduler);
void jsval_region_get_scheduler(const jsval_region_t *region,
		jsval_scheduler_t *scheduler_ptr);
void jsval_region_set_fetch_transport(jsval_region_t *region,
		const jsval_fetch_transport_t *transport, void *userdata);

/*
 * External-driver fetch waitlist.
 *
 * An embedder whose outbound HTTP/1.1 client lives in a coroutine body
 * (notably mnvkd's vk_jsmx_fetch_driver) can't do I/O from inside a
 * jsval_fetch_transport_t::poll callback — mnvkd's vk_* I/O macros
 * only compile inside a coroutine's top-level switch. For those
 * embedders, enable the waitlist with jsval_region_set_fetch_waitlist_mode.
 *
 * When waitlist mode is enabled AND no transport is registered,
 * jsval_fetch() pushes each call onto a per-region FIFO of pending
 * (Request, Promise) pairs and returns the pending Promise. The
 * embedder's responder coroutine drives the queue by repeatedly:
 *
 *   1. draining non-fetch microtasks,
 *   2. calling jsval_fetch_waitlist_pop(...) to dequeue one pending
 *      entry,
 *   3. doing the HTTP/1.1 round trip with vk_* macros at the
 *      coroutine body level (e.g. via a spawned sub-coroutine),
 *   4. resolving the returned Promise via jsval_promise_resolve (or
 *      jsval_promise_reject on failure),
 *   5. looping back to the drain.
 *
 * Transport ABI and waitlist mode are mutually exclusive from the
 * point of view of jsval_fetch(): if a transport is registered it
 * wins; if not and waitlist mode is off the reject-stub path runs
 * (TypeError "network not implemented yet"), preserving the
 * pre-seam contract.
 */
void jsval_region_set_fetch_waitlist_mode(jsval_region_t *region,
		int enabled);
size_t jsval_fetch_waitlist_size(const jsval_region_t *region);
int jsval_fetch_waitlist_pop(jsval_region_t *region,
		jsval_t *request_out, jsval_t *promise_out);
size_t jsval_region_remaining(jsval_region_t *region);
int jsval_region_alloc(jsval_region_t *region, size_t len, size_t align,
		void **ptr_ptr);
int jsval_region_measure_alloc(const jsval_region_t *region, size_t *used_ptr,
		size_t len, size_t align);
size_t jsval_pages_head_size(void);
const jsval_pages_t *jsval_region_pages(const jsval_region_t *region);
int jsval_region_root(jsval_region_t *region, jsval_t *value_ptr);
int jsval_region_set_root(jsval_region_t *region, jsval_t value);

int jsval_is_native(jsval_t value);
int jsval_is_json_backed(jsval_t value);

jsval_t jsval_undefined(void);
jsval_t jsval_null(void);
jsval_t jsval_bool(int boolean);
jsval_t jsval_number(double number);

int jsval_string_new_utf8(jsval_region_t *region, const uint8_t *str, size_t len, jsval_t *value_ptr);
int jsval_string_new_utf16(jsval_region_t *region, const uint16_t *str, size_t len, jsval_t *value_ptr);
int jsval_symbol_new(jsval_region_t *region, int have_description,
		jsval_t description_value, jsval_t *value_ptr);
int jsval_symbol_description(jsval_region_t *region, jsval_t symbol,
		jsval_t *value_ptr);
int jsval_bigint_new_i64(jsval_region_t *region, int64_t value,
		jsval_t *value_ptr);
int jsval_bigint_new_u64(jsval_region_t *region, uint64_t value,
		jsval_t *value_ptr);
int jsval_bigint_new_utf8(jsval_region_t *region, const uint8_t *str,
		size_t len, jsval_t *value_ptr);
int jsval_bigint_copy_utf8(jsval_region_t *region, jsval_t value,
		uint8_t *buf, size_t cap, size_t *len_ptr);
int jsval_bigint_compare(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr);
int jsval_function_new(jsval_region_t *region, jsval_native_function_fn fn,
		size_t length, int have_name, jsval_t name_value,
		jsval_t *value_ptr);
int jsval_function_name(jsval_region_t *region, jsval_t function,
		jsval_t *value_ptr);
int jsval_function_length(jsval_region_t *region, jsval_t function,
		jsval_t *value_ptr);
int jsval_function_call(jsval_region_t *region, jsval_t function, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error);
int jsval_date_new_now(jsval_region_t *region, jsval_t *value_ptr);
int jsval_date_new_time(jsval_region_t *region, jsval_t time_value,
		jsval_t *value_ptr);
int jsval_date_new_iso(jsval_region_t *region, jsval_t input_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_new_local_fields(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_new_utc_fields(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_value_of(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_get_time(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_time(jsval_region_t *region, jsval_t date_value,
		jsval_t time_value, jsval_t *value_ptr);
int jsval_date_is_valid(jsval_region_t *region, jsval_t date_value,
		int *is_valid_ptr);
int jsval_date_get_utc_full_year(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_full_year(jsval_region_t *region, jsval_t date_value,
		jsval_t year_value, jsval_t *value_ptr);
int jsval_date_get_utc_month(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_month(jsval_region_t *region, jsval_t date_value,
		jsval_t month_value, jsval_t *value_ptr);
int jsval_date_get_utc_date(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_date(jsval_region_t *region, jsval_t date_value,
		jsval_t day_value, jsval_t *value_ptr);
int jsval_date_get_utc_day(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_get_utc_hours(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_hours(jsval_region_t *region, jsval_t date_value,
		jsval_t hours_value, jsval_t *value_ptr);
int jsval_date_get_utc_minutes(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_minutes(jsval_region_t *region, jsval_t date_value,
		jsval_t minutes_value, jsval_t *value_ptr);
int jsval_date_get_utc_seconds(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_seconds(jsval_region_t *region, jsval_t date_value,
		jsval_t seconds_value, jsval_t *value_ptr);
int jsval_date_get_utc_milliseconds(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_utc_milliseconds(jsval_region_t *region,
		jsval_t date_value, jsval_t milliseconds_value, jsval_t *value_ptr);
int jsval_date_get_full_year(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_full_year(jsval_region_t *region, jsval_t date_value,
		jsval_t year_value, jsval_t *value_ptr);
int jsval_date_get_month(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_month(jsval_region_t *region, jsval_t date_value,
		jsval_t month_value, jsval_t *value_ptr);
int jsval_date_get_date(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_date(jsval_region_t *region, jsval_t date_value,
		jsval_t day_value, jsval_t *value_ptr);
int jsval_date_get_day(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_get_hours(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_hours(jsval_region_t *region, jsval_t date_value,
		jsval_t hours_value, jsval_t *value_ptr);
int jsval_date_get_minutes(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_minutes(jsval_region_t *region, jsval_t date_value,
		jsval_t minutes_value, jsval_t *value_ptr);
int jsval_date_get_seconds(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_seconds(jsval_region_t *region, jsval_t date_value,
		jsval_t seconds_value, jsval_t *value_ptr);
int jsval_date_get_milliseconds(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_set_milliseconds(jsval_region_t *region, jsval_t date_value,
		jsval_t milliseconds_value, jsval_t *value_ptr);
int jsval_date_to_iso_string(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_to_utc_string(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_to_string(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr);
int jsval_date_to_json(jsval_region_t *region, jsval_t date_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_now(jsval_region_t *region, jsval_t *value_ptr);
int jsval_date_utc(jsval_region_t *region, size_t argc, const jsval_t *argv,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_date_parse_iso(jsval_region_t *region, jsval_t input_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_array_buffer_new(jsval_region_t *region, size_t byte_length,
		jsval_t *value_ptr);
int jsval_array_buffer_byte_length(jsval_region_t *region, jsval_t buffer,
		size_t *len_ptr);
int jsval_array_buffer_copy_bytes(jsval_region_t *region, jsval_t buffer,
		uint8_t *buf, size_t cap, size_t *len_ptr);
/*
 * Return a mutable pointer to the backing bytes of an ArrayBuffer,
 * along with its capacity. Intended for embedders that need to write
 * directly into a region-allocated buffer (e.g., an HTTP server
 * vk_read'ing a request body). The pointer is valid until any region
 * operation that might relocate the buffer. Returns 0 on success,
 * -1 with errno=EINVAL on kind mismatch.
 */
int jsval_array_buffer_bytes_mut(jsval_region_t *region, jsval_t buffer,
		uint8_t **bytes_out, size_t *len_out);
int jsval_typed_array_new(jsval_region_t *region, jsval_typed_array_kind_t kind,
		size_t length, jsval_t *value_ptr);
int jsval_typed_array_kind(jsval_region_t *region, jsval_t typed_array,
		jsval_typed_array_kind_t *kind_ptr);
size_t jsval_typed_array_length(jsval_region_t *region, jsval_t typed_array);
int jsval_typed_array_byte_length(jsval_region_t *region, jsval_t typed_array,
		size_t *len_ptr);
int jsval_typed_array_buffer(jsval_region_t *region, jsval_t typed_array,
		jsval_t *value_ptr);
int jsval_typed_array_copy_bytes(jsval_region_t *region, jsval_t typed_array,
		uint8_t *buf, size_t cap, size_t *len_ptr);
int jsval_typed_array_get_number(jsval_region_t *region, jsval_t typed_array,
		size_t index, jsval_t *value_ptr);
int jsval_typed_array_get_bigint(jsval_region_t *region, jsval_t typed_array,
		size_t index, jsval_t *value_ptr);
int jsval_dom_exception_new(jsval_region_t *region, jsval_t name_value,
		jsval_t message_value, jsval_t *value_ptr);
int jsval_dom_exception_name(jsval_region_t *region, jsval_t exception_value,
		jsval_t *value_ptr);
int jsval_dom_exception_message(jsval_region_t *region,
		jsval_t exception_value, jsval_t *value_ptr);
int jsval_dom_exception_errors(jsval_region_t *region,
		jsval_t exception_value, jsval_t *value_ptr);
int jsval_aggregate_error_new(jsval_region_t *region,
		jsval_t errors_array, jsval_t message_value, jsval_t *value_ptr);
int jsval_crypto_new(jsval_region_t *region, jsval_t *value_ptr);
int jsval_subtle_crypto_new(jsval_region_t *region, jsval_t *value_ptr);
int jsval_crypto_subtle(jsval_region_t *region, jsval_t crypto_value,
		jsval_t *value_ptr);
int jsval_subtle_crypto_digest(jsval_region_t *region, jsval_t subtle_value,
		jsval_t algorithm_value, jsval_t data_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_generate_key(jsval_region_t *region,
		jsval_t subtle_value, jsval_t algorithm_value, int extractable,
		jsval_t usages_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_import_key(jsval_region_t *region,
		jsval_t subtle_value, jsval_t format_value, jsval_t key_data_value,
		jsval_t algorithm_value, int extractable, jsval_t usages_value,
		jsval_t *promise_ptr);
int jsval_subtle_crypto_export_key(jsval_region_t *region,
		jsval_t subtle_value, jsval_t format_value, jsval_t key_value,
		jsval_t *promise_ptr);
int jsval_subtle_crypto_sign(jsval_region_t *region, jsval_t subtle_value,
		jsval_t algorithm_value, jsval_t key_value, jsval_t data_value,
		jsval_t *promise_ptr);
int jsval_subtle_crypto_verify(jsval_region_t *region, jsval_t subtle_value,
		jsval_t algorithm_value, jsval_t key_value, jsval_t signature_value,
		jsval_t data_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_encrypt(jsval_region_t *region, jsval_t subtle_value,
		jsval_t algorithm_value, jsval_t key_value, jsval_t data_value,
		jsval_t *promise_ptr);
int jsval_subtle_crypto_decrypt(jsval_region_t *region, jsval_t subtle_value,
		jsval_t algorithm_value, jsval_t key_value, jsval_t data_value,
		jsval_t *promise_ptr);
int jsval_subtle_crypto_derive_bits(jsval_region_t *region,
		jsval_t subtle_value, jsval_t algorithm_value, jsval_t base_key_value,
		jsval_t length_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_derive_key(jsval_region_t *region,
		jsval_t subtle_value, jsval_t algorithm_value, jsval_t base_key_value,
		jsval_t derived_key_algorithm_value, int extractable,
		jsval_t usages_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_wrap_key(jsval_region_t *region, jsval_t subtle_value,
		jsval_t format_value, jsval_t key_value, jsval_t wrapping_key_value,
		jsval_t wrap_algorithm_value, jsval_t *promise_ptr);
int jsval_subtle_crypto_unwrap_key(jsval_region_t *region,
		jsval_t subtle_value, jsval_t format_value, jsval_t wrapped_key_value,
		jsval_t unwrapping_key_value, jsval_t unwrap_algorithm_value,
		jsval_t unwrapped_key_algorithm_value, int extractable,
		jsval_t key_usages_value, jsval_t *promise_ptr);
int jsval_crypto_random_uuid(jsval_region_t *region, jsval_t crypto_value,
		jsval_t *value_ptr);
int jsval_crypto_get_random_values(jsval_region_t *region,
		jsval_t crypto_value, jsval_t typed_array, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_crypto_key_new(jsval_region_t *region, jsval_crypto_key_type_t type,
		int extractable, jsval_t algorithm_value, uint32_t usages_mask,
		jsval_t *value_ptr);
int jsval_crypto_key_type(jsval_region_t *region, jsval_t key_value,
		jsval_t *value_ptr);
int jsval_crypto_key_extractable(jsval_region_t *region, jsval_t key_value,
		int *extractable_ptr);
int jsval_crypto_key_algorithm(jsval_region_t *region, jsval_t key_value,
		jsval_t *value_ptr);
int jsval_crypto_key_usages(jsval_region_t *region, jsval_t key_value,
		uint32_t *usages_ptr);
int jsval_promise_new(jsval_region_t *region, jsval_t *value_ptr);
int jsval_promise_state(jsval_region_t *region, jsval_t promise_value,
		jsval_promise_state_t *state_ptr);
int jsval_promise_result(jsval_region_t *region, jsval_t promise_value,
		jsval_t *value_ptr);
int jsval_promise_resolve(jsval_region_t *region, jsval_t promise_value,
		jsval_t value);
int jsval_promise_reject(jsval_region_t *region, jsval_t promise_value,
		jsval_t reason);
int jsval_promise_then(jsval_region_t *region, jsval_t promise_value,
		jsval_t on_fulfilled, jsval_t on_rejected, jsval_t *value_ptr);
int jsval_promise_catch(jsval_region_t *region, jsval_t promise_value,
		jsval_t on_rejected, jsval_t *value_ptr);
int jsval_promise_finally(jsval_region_t *region, jsval_t promise_value,
		jsval_t on_finally, jsval_t *value_ptr);
int jsval_promise_all(jsval_region_t *region, const jsval_t *inputs,
		size_t n, jsval_t *out_promise);
int jsval_promise_race(jsval_region_t *region, const jsval_t *inputs,
		size_t n, jsval_t *out_promise);
int jsval_promise_all_settled(jsval_region_t *region, const jsval_t *inputs,
		size_t n, jsval_t *out_promise);
int jsval_promise_any(jsval_region_t *region, const jsval_t *inputs,
		size_t n, jsval_t *out_promise);
int jsval_microtask_enqueue(jsval_region_t *region, jsval_t function,
		size_t argc, const jsval_t *argv);
int jsval_queue_microtask(jsval_region_t *region, jsval_t callback);
size_t jsval_microtask_pending(jsval_region_t *region);
int jsval_microtask_drain(jsval_region_t *region, jsmethod_error_t *error);
int jsval_object_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_array_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);

int jsval_json_parse(jsval_region_t *region, const uint8_t *json, size_t len, unsigned int token_cap, jsval_t *value_ptr);
int jsval_promote(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_promote_in_place(jsval_region_t *region, jsval_t *value_ptr);
int jsval_region_promote_root(jsval_region_t *region, jsval_t *value_ptr);
int jsval_promote_object_shallow_measure(jsval_region_t *region, jsval_t object, size_t prop_cap, size_t *bytes_ptr);
int jsval_promote_object_shallow(jsval_region_t *region, jsval_t object, size_t prop_cap, jsval_t *value_ptr);
int jsval_promote_object_shallow_in_place(jsval_region_t *region, jsval_t *value_ptr, size_t prop_cap);
int jsval_promote_array_shallow_measure(jsval_region_t *region, jsval_t array, size_t elem_cap, size_t *bytes_ptr);
int jsval_promote_array_shallow(jsval_region_t *region, jsval_t array, size_t elem_cap, jsval_t *value_ptr);
int jsval_promote_array_shallow_in_place(jsval_region_t *region, jsval_t *value_ptr, size_t elem_cap);
int jsval_copy_json(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr);

int jsval_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr);
int jsval_string_to_cstr(jsval_region_t *region, jsval_t str_val, char *buf, size_t cap, size_t *out_len);
int jsval_string_concat_utf8(jsval_region_t *region, const uint8_t *left, size_t left_len, jsval_t right_str, jsval_t *out);
int jsval_text_encode_utf8(jsval_region_t *region, jsval_t string_value, jsval_t *uint8array_out);
int jsval_text_decode_utf8(jsval_region_t *region, jsval_t buffer_value, jsval_t *string_out);
int jsval_base64_encode(jsval_region_t *region, jsval_t input_value, jsval_t *string_out);
int jsval_base64_decode(jsval_region_t *region, jsval_t input_value, jsval_t *uint8array_out);
int jsval_base64url_encode(jsval_region_t *region, jsval_t input_value, jsval_t *string_out);
int jsval_base64url_decode(jsval_region_t *region, jsval_t input_value, jsval_t *uint8array_out);
size_t jsval_object_size(jsval_region_t *region, jsval_t object);
size_t jsval_array_length(jsval_region_t *region, jsval_t array);

int jsval_object_has_own_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, int *has_ptr);
int jsval_object_get_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t *value_ptr);
int jsval_object_set_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t value);
int jsval_object_delete_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, int *deleted_ptr);
int jsval_object_has_own_key(jsval_region_t *region, jsval_t object,
		jsval_t key, int *has_ptr);
int jsval_object_get_key(jsval_region_t *region, jsval_t object, jsval_t key,
		jsval_t *value_ptr);
int jsval_object_set_key(jsval_region_t *region, jsval_t object, jsval_t key,
		jsval_t value);
int jsval_object_delete_key(jsval_region_t *region, jsval_t object,
		jsval_t key, int *deleted_ptr);
int jsval_object_key_at(jsval_region_t *region, jsval_t object, size_t index, jsval_t *key_ptr);
int jsval_object_value_at(jsval_region_t *region, jsval_t object, size_t index, jsval_t *value_ptr);
int jsval_object_copy_own(jsval_region_t *region, jsval_t dst, jsval_t src);
int jsval_object_clone_own(jsval_region_t *region, jsval_t src, size_t capacity, jsval_t *value_ptr);
int jsval_set_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_set_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr);
int jsval_set_size(jsval_region_t *region, jsval_t set, size_t *size_ptr);
int jsval_set_has(jsval_region_t *region, jsval_t set, jsval_t key,
		int *has_ptr);
int jsval_set_add(jsval_region_t *region, jsval_t set, jsval_t key);
int jsval_set_delete(jsval_region_t *region, jsval_t set, jsval_t key,
		int *deleted_ptr);
int jsval_set_clear(jsval_region_t *region, jsval_t set);
int jsval_map_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_map_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr);
int jsval_map_size(jsval_region_t *region, jsval_t map, size_t *size_ptr);
int jsval_map_has(jsval_region_t *region, jsval_t map, jsval_t key,
		int *has_ptr);
int jsval_map_get(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t *value_ptr);
int jsval_map_set(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t value);
int jsval_map_delete(jsval_region_t *region, jsval_t map, jsval_t key,
		int *deleted_ptr);
int jsval_map_clear(jsval_region_t *region, jsval_t map);
int jsval_map_key_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *key_ptr);
int jsval_map_value_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *value_ptr);
int jsval_get_iterator(jsval_region_t *region, jsval_t iterable,
		jsval_iterator_selector_t selector, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_iterator_next_entry(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *key_ptr, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_array_get(jsval_region_t *region, jsval_t array, size_t index, jsval_t *value_ptr);
int jsval_array_set(jsval_region_t *region, jsval_t array, size_t index, jsval_t value);
int jsval_array_clone_dense(jsval_region_t *region, jsval_t src, size_t capacity, jsval_t *value_ptr);
int jsval_array_splice_dense(jsval_region_t *region, jsval_t array, size_t start, size_t delete_count, const jsval_t *insert_values, size_t insert_count, jsval_t *removed_ptr);
int jsval_array_push(jsval_region_t *region, jsval_t array, jsval_t value);
int jsval_array_pop(jsval_region_t *region, jsval_t array, jsval_t *value_ptr);
int jsval_array_shift(jsval_region_t *region, jsval_t array, jsval_t *value_ptr);
int jsval_array_unshift(jsval_region_t *region, jsval_t array, jsval_t value);
int jsval_array_set_length(jsval_region_t *region, jsval_t array, size_t new_len);

int jsval_url_new(jsval_region_t *region, jsval_t input_value, int have_base,
		jsval_t base_value, jsval_t *value_ptr);
int jsval_url_can_parse(jsval_region_t *region, jsval_t input_value,
		int have_base, jsval_t base_value, jsval_t *value_ptr);
int jsval_url_parse(jsval_region_t *region, jsval_t input_value,
		int have_base, jsval_t base_value, jsval_t *value_ptr);
int jsval_url_href(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_origin(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_origin_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_username(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_password(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_host(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_host_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hostname_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_port(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_search(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_search_params(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_set_href(jsval_region_t *region, jsval_t url_value,
		jsval_t href_value);
int jsval_url_set_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t protocol_value);
int jsval_url_set_username(jsval_region_t *region, jsval_t url_value,
		jsval_t username_value);
int jsval_url_set_password(jsval_region_t *region, jsval_t url_value,
		jsval_t password_value);
int jsval_url_set_host(jsval_region_t *region, jsval_t url_value,
		jsval_t host_value);
int jsval_url_set_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t hostname_value);
int jsval_url_set_port(jsval_region_t *region, jsval_t url_value,
		jsval_t port_value);
int jsval_url_set_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t pathname_value);
int jsval_url_set_search(jsval_region_t *region, jsval_t url_value,
		jsval_t search_value);
int jsval_url_set_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t hash_value);
int jsval_url_to_string(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_to_json(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);

int jsval_url_search_params_new(jsval_region_t *region, jsval_t init_value,
		jsval_t *value_ptr);
int jsval_url_search_params_size(jsval_region_t *region, jsval_t params_value,
		jsval_t *value_ptr);
int jsval_url_search_params_append(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value);
int jsval_url_search_params_delete(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value);
int jsval_url_search_params_get(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_get_all(jsval_region_t *region,
		jsval_t params_value, jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_has(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_set(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value);
int jsval_url_search_params_sort(jsval_region_t *region, jsval_t params_value);
int jsval_url_search_params_to_string(jsval_region_t *region,
		jsval_t params_value, jsval_t *value_ptr);

int jsval_to_number(jsval_region_t *region, jsval_t value, double *number_ptr);
int jsval_to_int32(jsval_region_t *region, jsval_t value, int32_t *result_ptr);
int jsval_to_uint32(jsval_region_t *region, jsval_t value,
		uint32_t *result_ptr);
int jsval_is_nullish(jsval_t value);
int jsval_typeof(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_truthy(jsval_region_t *region, jsval_t value);
int jsval_strict_eq(jsval_region_t *region, jsval_t left, jsval_t right);
int jsval_abstract_eq(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr);
int jsval_abstract_ne(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr);
int jsval_less_than(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_less_equal(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_greater_than(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_greater_equal(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_unary_plus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_unary_minus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_bitwise_not(jsval_region_t *region, jsval_t value,
		jsval_t *value_ptr);
int jsval_add(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_bitwise_and(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_bitwise_or(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_bitwise_xor(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_left(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_right(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_right_unsigned(jsval_region_t *region, jsval_t left,
		jsval_t right, jsval_t *value_ptr);
int jsval_subtract(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_multiply(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_divide(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_remainder(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_method_string_to_lower_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_upper_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_locale_lower_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_locale_upper_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_is_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_start(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_end(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_left(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_right(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_concat_measure(jsval_region_t *region,
		jsval_t this_value, size_t arg_count, const jsval_t *args,
		jsmethod_string_concat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_concat(jsval_region_t *region, jsval_t this_value,
		size_t arg_count, const jsval_t *args, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_replace(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_replace_all(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_repeat_measure(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsmethod_string_repeat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_repeat(jsval_region_t *region, jsval_t this_value,
		int have_count, jsval_t count_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_pad_start_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_pad_start(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_pad_end_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_pad_end(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_char_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_char_code_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_code_point_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_slice(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_substring(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_substr(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_length, jsval_t length_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split(jsval_region_t *region, jsval_t this_value,
		int have_separator, jsval_t separator_value,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_index_of(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_last_index_of(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_includes(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_starts_with(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_ends_with(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_end_position,
		jsval_t end_position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
#if JSMX_WITH_REGEX
int jsval_regexp_new(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_new_jit(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_source(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr);
int jsval_regexp_global(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_ignore_case(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_multiline(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_dot_all(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_unicode(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_sticky(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_flags(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr);
int jsval_regexp_info(jsval_region_t *region, jsval_t regexp_value,
		jsval_regexp_info_t *info_ptr);
int jsval_regexp_get_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t *last_index_ptr);
int jsval_regexp_set_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t last_index);
int jsval_regexp_test(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, int *result_ptr, jsmethod_error_t *error);
int jsval_regexp_exec(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_match_all(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_match_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_surrogate(
		jsval_region_t *region, jsval_t this_value,
		uint16_t surrogate_unit, jsval_t *value_ptr,
		jsmethod_error_t *error);
#define JSVAL_DECLARE_U_PREDEFINED_CLASS_API(name) \
int jsval_method_string_search_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_match_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_match_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int global, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_replace_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_replace_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_replace_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_replace_all_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_split_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int have_limit, \
		jsval_t limit_value, jsval_t *value_ptr, \
		jsmethod_error_t *error);
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(digit)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_digit)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(whitespace)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_whitespace)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(word)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_word)
#undef JSVAL_DECLARE_U_PREDEFINED_CLASS_API
int jsval_method_string_search_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_surrogate(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_split_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_regex(jsval_region_t *region,
		jsval_t this_value, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
#endif
int jsval_method_string_normalize_measure(jsval_region_t *region,
		jsval_t this_value, int have_form, jsval_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_normalize(jsval_region_t *region, jsval_t this_value,
		int have_form, jsval_t form_value,
		uint16_t *this_storage, size_t this_storage_cap,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsval_t *value_ptr, jsmethod_error_t *error);

/*
 * WHATWG Fetch API — JS object model.
 *
 * Public value kinds: JSVAL_KIND_HEADERS, JSVAL_KIND_REQUEST,
 * JSVAL_KIND_RESPONSE. Implementation lives in jsval.c.
 *
 * Network transport (real fetch over HTTP/1.1) is a separate future
 * slice; for now jsval_fetch returns a Promise that rejects with
 * DOMException{name: "TypeError", message: "network not implemented yet"}.
 */

typedef enum jsval_headers_guard_e {
	JSVAL_HEADERS_GUARD_NONE = 0,
	JSVAL_HEADERS_GUARD_IMMUTABLE = 1,
	JSVAL_HEADERS_GUARD_REQUEST = 2,
	JSVAL_HEADERS_GUARD_REQUEST_NO_CORS = 3,
	JSVAL_HEADERS_GUARD_RESPONSE = 4
} jsval_headers_guard_t;

int jsval_headers_new(jsval_region_t *region, jsval_headers_guard_t guard,
		jsval_t *value_ptr);
int jsval_headers_new_from_init(jsval_region_t *region,
		jsval_headers_guard_t guard, jsval_t init_value, jsval_t *value_ptr);
int jsval_headers_append(jsval_region_t *region, jsval_t headers,
		jsval_t name, jsval_t value);
int jsval_headers_set(jsval_region_t *region, jsval_t headers,
		jsval_t name, jsval_t value);
int jsval_headers_delete(jsval_region_t *region, jsval_t headers,
		jsval_t name);
int jsval_headers_has(jsval_region_t *region, jsval_t headers,
		jsval_t name, int *has_ptr);
int jsval_headers_get(jsval_region_t *region, jsval_t headers,
		jsval_t name, jsval_t *value_ptr);
int jsval_headers_get_set_cookie(jsval_region_t *region, jsval_t headers,
		jsval_t *array_ptr);
int jsval_headers_size(jsval_region_t *region, jsval_t headers,
		size_t *size_ptr);
int jsval_headers_entry_at(jsval_region_t *region, jsval_t headers,
		size_t index, jsval_t *name_ptr, jsval_t *value_ptr);

int jsval_request_new(jsval_region_t *region, jsval_t input_value,
		jsval_t init_value, int have_init, jsval_t *value_ptr);
int jsval_request_method(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_url(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_headers(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_body_used(jsval_region_t *region, jsval_t request,
		int *used_ptr);
int jsval_request_mode(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_credentials(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_cache(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_redirect(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_clone(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);
int jsval_request_text(jsval_region_t *region, jsval_t request,
		jsval_t *promise_ptr);
int jsval_request_json(jsval_region_t *region, jsval_t request,
		jsval_t *promise_ptr);
int jsval_request_array_buffer(jsval_region_t *region, jsval_t request,
		jsval_t *promise_ptr);
int jsval_request_bytes(jsval_region_t *region, jsval_t request,
		jsval_t *promise_ptr);
/*
 * Request.body getter: returns a ReadableStream wrapping the Request's
 * body. If the Request has no body (!has_body) or its body is already
 * used, returns JSVAL_KIND_NULL. Otherwise returns a new
 * JSVAL_KIND_READABLE_STREAM and flips bodyUsed to 1 — same Phase-1B
 * simplification as jsval_response_body. Subsequent consumer calls
 * (text/json/arrayBuffer/bytes) reject with the body-already-used
 * TypeError. Returns 0 on success, -1 with errno on error.
 */
int jsval_request_body(jsval_region_t *region, jsval_t request,
		jsval_t *value_ptr);

int jsval_response_new(jsval_region_t *region, jsval_t body_value,
		int have_body, jsval_t init_value, int have_init, jsval_t *value_ptr);
int jsval_response_error(jsval_region_t *region, jsval_t *value_ptr);
int jsval_response_redirect(jsval_region_t *region, jsval_t url_value,
		int have_status, uint32_t status, jsval_t *value_ptr);
int jsval_response_json(jsval_region_t *region, jsval_t data_value,
		int have_init, jsval_t init_value, jsval_t *value_ptr);
int jsval_response_status(jsval_region_t *region, jsval_t response,
		uint32_t *status_ptr);
int jsval_response_status_text(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);
int jsval_response_ok(jsval_region_t *region, jsval_t response, int *ok_ptr);
int jsval_response_type(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);
int jsval_response_redirected(jsval_region_t *region, jsval_t response,
		int *redirected_ptr);
int jsval_response_url(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);
int jsval_response_headers(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);
int jsval_response_body_used(jsval_region_t *region, jsval_t response,
		int *used_ptr);
int jsval_response_clone(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);
int jsval_response_text(jsval_region_t *region, jsval_t response,
		jsval_t *promise_ptr);
int jsval_response_json_body(jsval_region_t *region, jsval_t response,
		jsval_t *promise_ptr);
int jsval_response_array_buffer(jsval_region_t *region, jsval_t response,
		jsval_t *promise_ptr);
int jsval_response_bytes(jsval_region_t *region, jsval_t response,
		jsval_t *promise_ptr);
/*
 * Response.body getter: returns a ReadableStream wrapping the Response's
 * body. If the Response has no body (!has_body) or its body is already
 * used, returns JSVAL_KIND_NULL. Otherwise returns a new
 * JSVAL_KIND_READABLE_STREAM and flips bodyUsed to 1 — this is a
 * Phase-1B simplification that diverges from WHATWG (which only flips
 * bodyUsed when the stream is drained). Subsequent consumer calls
 * (text/json/arrayBuffer/bytes) reject with the usual body-already-used
 * TypeError. Returns 0 on success, -1 with errno on error.
 */
int jsval_response_body(jsval_region_t *region, jsval_t response,
		jsval_t *value_ptr);

int jsval_fetch(jsval_region_t *region, jsval_t input_value,
		jsval_t init_value, int have_init, jsval_t *promise_ptr);

/*
 * Body source abstraction for lazy Request / Response bodies.
 *
 * An embedder (today: test harness; tomorrow: mnvkd vk_socket adapter)
 * implements this vtable to feed bytes into the Body mixin asynchronously.
 * The drain microtask pumps the source until it reports EOF, then resolves
 * the body-consumption promise.
 *
 * The read callback runs inside the jsmx microtask drain loop. It must not
 * yield or allocate, and it must return quickly. A single read fills a
 * scratch buffer; the drain re-enqueues itself to keep draining.
 */

typedef enum jsval_body_source_status_e {
	JSVAL_BODY_SOURCE_STATUS_READY = 0,
	JSVAL_BODY_SOURCE_STATUS_PENDING = 1,
	JSVAL_BODY_SOURCE_STATUS_EOF = 2,
	JSVAL_BODY_SOURCE_STATUS_ERROR = 3
} jsval_body_source_status_t;

typedef struct jsval_body_source_vtable_s {
	int (*read)(void *userdata, uint8_t *buf, size_t cap,
			size_t *out_len, jsval_body_source_status_t *status_ptr);
	void (*close)(void *userdata);
} jsval_body_source_vtable_t;

/*
 * Producer-side wake: signals to jsmx that the body source at
 * `source_off` has new data or has transitioned to EOF/ERROR.
 * If a ReadableStream pump was parked waiting on this source,
 * a fresh pump microtask is scheduled. No-op if nothing is
 * parked. The producer typically calls this after each push,
 * mark-eof, or mark-error on its own buffering layer.
 *
 * `source_off` must be a value previously obtained via
 * jsval_response_body_source_off (or the future Request
 * equivalent). Returns 0 on success, -1 with errno on error.
 */
int jsval_body_source_notify(jsval_region_t *region,
		jsval_off_t source_off);

/*
 * Returns the internal body-source offset of a streaming
 * Response via *out. For non-streaming Responses (in-memory
 * body_buffer), *out is set to 0. Intended for producer-side
 * embedders that need to call jsval_body_source_notify.
 */
int jsval_response_body_source_off(jsval_region_t *region,
		jsval_t response, jsval_off_t *out);

/*
 * Parallel to jsval_response_body_source_off for Requests. Returns 0
 * with *out set to the streaming Request's internal body-source
 * offset (for producer-side jsval_body_source_notify), or 0 for
 * non-streaming Requests. Shipped for symmetry so a future streaming
 * inbound-body adapter (when the FaaS bridge feeds Request bodies
 * through vk_jsmx_body_source instead of pre-materializing) has a
 * ready producer hook.
 */
int jsval_request_body_source_off(jsval_region_t *region,
		jsval_t request, jsval_off_t *out);

/*
 * ReadableStream: minimal WHATWG-shaped facade over the body-source vtable.
 *
 * A stream wraps a jsval_body_source_vtable_t + userdata (foundational path)
 * or a byte buffer (in-memory shortcut). Readers drain one chunk per
 * reader.read() via the microtask-pump pattern: READY fulfills with a
 * Uint8Array chunk, EOF fulfills with { done: true }, ERROR rejects with
 * a DOMException, PENDING queues the read until the next drain.
 *
 * Phase 1 scope: no controller-based construction (new ReadableStream({pull})),
 * no tee(), no pipeTo/pipeThrough, no BYOB readers, no async iteration.
 * Reader.read() returns a Promise<{ value: Uint8Array | undefined, done: bool }>.
 */
int jsval_readable_stream_new_from_source(jsval_region_t *region,
		const jsval_body_source_vtable_t *vtable, void *userdata,
		jsval_t *value_ptr);
int jsval_readable_stream_new_from_bytes(jsval_region_t *region,
		const uint8_t *bytes, size_t len, jsval_t *value_ptr);
int jsval_readable_stream_locked(jsval_region_t *region, jsval_t stream,
		int *locked_ptr);
int jsval_readable_stream_get_reader(jsval_region_t *region, jsval_t stream,
		jsval_t *reader_ptr);
int jsval_readable_stream_reader_release_lock(jsval_region_t *region,
		jsval_t reader);
int jsval_readable_stream_reader_read(jsval_region_t *region, jsval_t reader,
		jsval_t *promise_ptr);
int jsval_readable_stream_cancel(jsval_region_t *region, jsval_t stream,
		jsval_t reason, jsval_t *promise_ptr);

/*
 * Underlying-sink abstraction for WritableStream.
 *
 * Symmetric to jsval_body_source_vtable_t on the read side. An embedder
 * implements this vtable to accept bytes pushed from JavaScript via
 * writer.write(). The pump microtask drains the stream's pending-writes
 * FIFO, calling write() per chunk; if the sink reports PENDING the pump
 * parks on parked_writer_off and the producer must call
 * jsval_underlying_sink_notify when capacity is available to re-wake
 * the pump.
 *
 * Status semantics:
 *   READY    - bytes were accepted; *accepted_len >= 0 (may be < chunk_len
 *              for partial accept, in which case the pump re-tries the
 *              tail on the next tick).
 *   PENDING  - sink cannot accept now; producer must notify when ready.
 *              *accepted_len ignored.
 *   ERROR    - sink failed; *error_value carries the rejection reason
 *              (e.g. a DOMException or TypeError) for all queued writes.
 *
 * close() is invoked once after all queued writes drain (or immediately
 * on abort, with a TypeError-shaped error_value supplied by abort's
 * reason). It may also return PENDING/ERROR.
 */
typedef enum jsval_underlying_sink_status_e {
	JSVAL_UNDERLYING_SINK_STATUS_READY = 0,
	JSVAL_UNDERLYING_SINK_STATUS_PENDING = 1,
	JSVAL_UNDERLYING_SINK_STATUS_ERROR = 2
} jsval_underlying_sink_status_t;

typedef struct jsval_underlying_sink_vtable_s {
	jsval_underlying_sink_status_t (*write)(jsval_region_t *region,
			void *userdata, const uint8_t *chunk, size_t chunk_len,
			size_t *accepted_len, jsval_t *error_value);
	jsval_underlying_sink_status_t (*close)(jsval_region_t *region,
			void *userdata, jsval_t *error_value);
} jsval_underlying_sink_vtable_t;

/*
 * Producer-side wake symmetric to jsval_body_source_notify. Signals
 * that the underlying sink at `sink_off` has cleared backpressure
 * (write() will now return READY) or has otherwise transitioned. If
 * the writable stream's pump was parked, a fresh pump microtask is
 * scheduled. No-op when nothing is parked.
 *
 * `sink_off` must be a value previously obtained via
 * jsval_writable_stream_sink_off.
 */
int jsval_underlying_sink_notify(jsval_region_t *region,
		jsval_off_t sink_off);

/*
 * WritableStream: minimal WHATWG-shaped facade over the underlying-sink
 * vtable. Single-writer (one default writer at a time).
 *
 * Phase 3a-1 scope: no controller-based construction (new
 * WritableStream({write})), no pipeTo/pipeThrough, no size/highWaterMark
 * queueing strategy (queue depth is unbounded; backpressure is signaled
 * solely by the sink returning PENDING). writer.write() and
 * writer.close() return Promise<undefined> resolved when the chunk is
 * accepted by the sink (or close runs); writer.ready returns a Promise
 * that fulfils once the sink reports READY (or immediately if no write
 * is currently parked).
 */
int jsval_writable_stream_new_from_sink(jsval_region_t *region,
		const jsval_underlying_sink_vtable_t *vtable, void *userdata,
		jsval_t *value_ptr);
int jsval_writable_stream_locked(jsval_region_t *region, jsval_t stream,
		int *locked_ptr);
int jsval_writable_stream_get_writer(jsval_region_t *region, jsval_t stream,
		jsval_t *writer_ptr);
int jsval_writable_stream_writer_release_lock(jsval_region_t *region,
		jsval_t writer);
int jsval_writable_stream_writer_write(jsval_region_t *region, jsval_t writer,
		const uint8_t *chunk, size_t chunk_len, jsval_t *promise_ptr);
int jsval_writable_stream_writer_ready(jsval_region_t *region, jsval_t writer,
		jsval_t *promise_ptr);
int jsval_writable_stream_writer_close(jsval_region_t *region, jsval_t writer,
		jsval_t *promise_ptr);
int jsval_writable_stream_abort(jsval_region_t *region, jsval_t stream,
		jsval_t reason, jsval_t *promise_ptr);

/*
 * Returns the internal underlying-sink offset of a WritableStream via
 * *out (for producer-side jsval_underlying_sink_notify). Mirrors
 * jsval_response_body_source_off on the read side.
 */
int jsval_writable_stream_sink_off(jsval_region_t *region, jsval_t stream,
		jsval_off_t *out);

/*
 * TransformStream: WHATWG-shaped composition of a ReadableStream
 * and a WritableStream connected through a C-callable transformer.
 *
 * A transformer's transform() callback runs once per writer.write()
 * with a controller that can enqueue zero or more output chunks
 * onto the readable side, signal an error, or terminate the stream
 * early. flush() (optional) runs once on writer.close(), giving the
 * transformer a final opportunity to enqueue trailing bytes (e.g.
 * a Z_FINISH flush for compression) before the readable side
 * reports EOF.
 *
 * Phase 4-1 scope: no constructor-based construction
 * (new TransformStream({transform})), no high-water mark / bounded
 * channel (the chunk FIFO is unbounded and the writable side is
 * always READY), no pipeTo / pipeThrough. The C API is symmetric
 * with the *_new_from_source / *_new_from_sink factories on the
 * Read and Write halves.
 *
 * The controller is a synchronous handle valid only for the
 * duration of a transform()/flush() call. enqueue() copies the
 * supplied bytes into a region-allocated chunk node and notifies
 * any reader parked on the readable side; error() flips the
 * channel into ERROR with the given reason and notifies; terminate()
 * is the controller-side equivalent of writer.close() and flips
 * the channel into a closed state.
 */
typedef struct jsval_transform_controller_s jsval_transform_controller_t;

int jsval_transform_controller_enqueue(
		jsval_transform_controller_t *controller,
		const uint8_t *bytes, size_t len);
int jsval_transform_controller_error(
		jsval_transform_controller_t *controller, jsval_t reason);
int jsval_transform_controller_terminate(
		jsval_transform_controller_t *controller);

typedef struct jsval_transformer_vtable_s {
	int (*transform)(jsval_region_t *region, void *userdata,
			const uint8_t *chunk, size_t chunk_len,
			jsval_transform_controller_t *controller);
	int (*flush)(jsval_region_t *region, void *userdata,
			jsval_transform_controller_t *controller);
} jsval_transformer_vtable_t;

int jsval_transform_stream_new(jsval_region_t *region,
		const jsval_transformer_vtable_t *vtable, void *userdata,
		jsval_t *value_ptr);

int jsval_transform_stream_readable(jsval_region_t *region, jsval_t stream,
		jsval_t *value_ptr);
int jsval_transform_stream_writable(jsval_region_t *region, jsval_t stream,
		jsval_t *value_ptr);

/*
 * Returns the internal channel offset of a TransformStream. Reserved
 * for future producer-side wake hooks; callers can ignore in v1.
 */
int jsval_transform_stream_channel_off(jsval_region_t *region, jsval_t stream,
		jsval_off_t *out);

/*
 * Factory: a TransformStream whose transformer guarantees that no
 * UTF-8 codepoint is split across an output chunk boundary. Trailing
 * incomplete sequences are buffered until the next input chunk;
 * malformed sequences are replaced with U+FFFD; if the writable
 * side is closed while a partial sequence is buffered, flush emits
 * a single U+FFFD before EOF.
 *
 * Returned value is JSVAL_KIND_TRANSFORM_STREAM; access via
 * jsval_transform_stream_readable / jsval_transform_stream_writable.
 */
int jsval_text_decoder_stream_new(jsval_region_t *region,
		jsval_t *value_ptr);

/*
 * Factory: a TransformStream whose transformer validates each input
 * chunk as UTF-8, replacing any malformed sequence with U+FFFD.
 * Stateless across chunks (incomplete sequences at a chunk boundary
 * become U+FFFDs at that boundary); use a TextDecoderStream upstream
 * if chunk-boundary correctness on the input bytes is required.
 *
 * Returned value is JSVAL_KIND_TRANSFORM_STREAM.
 */
int jsval_text_encoder_stream_new(jsval_region_t *region,
		jsval_t *value_ptr);

/*
 * Drains `readable` into `writable` end-to-end. Both streams are
 * locked for the duration via internal default reader/writer
 * acquisitions; a stream that is already locked makes pipe_to
 * fail (errno EBUSY) before scheduling.
 *
 * Returns a Promise via *promise_ptr that fulfils with undefined
 * once the readable reaches EOF and the writable's close()
 * promise has resolved, or rejects with the first error from
 * either side. Both locks are released on settle.
 *
 * Phase 6 v1: no AbortSignal / preventClose / preventAbort /
 * preventCancel options.
 */
int jsval_readable_stream_pipe_to(jsval_region_t *region,
		jsval_t readable, jsval_t writable, jsval_t *promise_ptr);

/*
 * Convenience: pipes `readable` into `transform_stream`'s writable
 * side and returns `transform_stream`'s readable side via *out for
 * onward chaining (the canonical
 * `response.body.pipeThrough(new TextDecoderStream())` pattern).
 * The pipe Promise itself is internal to this call; downstream
 * errors propagate to the returned readable's reads.
 */
int jsval_readable_stream_pipe_through(jsval_region_t *region,
		jsval_t readable, jsval_t transform_stream, jsval_t *out);

/*
 * WHATWG Compression Streams: zlib-backed CompressionStream and
 * DecompressionStream factories. Both return JSVAL_KIND_TRANSFORM_STREAM
 * jsvals usable like any other transform stream (access via
 * jsval_transform_stream_readable / _writable).
 *
 * Format selects the wire framing via zlib's windowBits:
 *   GZIP        — windowBits = 31 (deflate + gzip wrapper)
 *   DEFLATE     — windowBits = 15 (deflate + zlib wrapper)
 *   DEFLATE_RAW — windowBits = -15 (raw deflate, no wrapper)
 *
 * Per-instance zlib state is allocated through the jsmx arena
 * (zalloc/zfree shims backed by jsval_region_alloc); deflateEnd /
 * inflateEnd run on writer.close().
 *
 * Decompression input that does not parse as the declared format
 * causes the readable side to error with a TypeError DOMException
 * (matching WHATWG).
 */
typedef enum jsval_compression_format_e {
	JSVAL_COMPRESSION_FORMAT_GZIP        = 0,
	JSVAL_COMPRESSION_FORMAT_DEFLATE     = 1,
	JSVAL_COMPRESSION_FORMAT_DEFLATE_RAW = 2
} jsval_compression_format_t;

int jsval_compression_stream_new(jsval_region_t *region,
		jsval_compression_format_t format, jsval_t *value_ptr);
int jsval_decompression_stream_new(jsval_region_t *region,
		jsval_compression_format_t format, jsval_t *value_ptr);

typedef enum jsval_body_consume_mode_e {
	JSVAL_BODY_CONSUME_TEXT = 0,
	JSVAL_BODY_CONSUME_JSON = 1,
	JSVAL_BODY_CONSUME_ARRAY_BUFFER = 2,
	JSVAL_BODY_CONSUME_BYTES = 3
} jsval_body_consume_mode_t;

#define JSVAL_FETCH_BODY_DEFAULT_LIMIT ((size_t)(16u * 1024u * 1024u))

typedef int (*jsval_fetch_header_emit_fn)(
		void *userdata,
		const uint8_t *name, size_t name_len,
		const uint8_t *value, size_t value_len);

/*
 * Context passed as userdata to jsval_fetch_emit_header_into. The caller
 * fills both fields and passes &ctx as the emit callback's userdata.
 */
typedef struct jsval_fetch_header_emit_ctx_s {
	jsval_region_t *region;
	jsval_t headers;
} jsval_fetch_header_emit_ctx_t;

int jsval_fetch_emit_header_into(void *userdata,
		const uint8_t *name, size_t name_len,
		const uint8_t *value, size_t value_len);

/*
 * method_value must be a string jsval containing a standard HTTP method
 * name (case-insensitive). Forbidden methods (CONNECT/TRACE/TRACK) are
 * rejected. An undefined method_value defaults to GET.
 */
int jsval_request_new_from_parts(jsval_region_t *region,
		jsval_t method_value,
		jsval_t url_value,
		jsval_t headers_value,
		const jsval_body_source_vtable_t *body_vtable,
		void *body_userdata,
		size_t content_length_hint,
		jsval_t *value_ptr);

/*
 * Read-without-consume body accessors. Return the Request / Response
 * body as an ArrayBuffer jsval without flipping body_used. Used by the
 * FaaS bridge's response serializer and a future outbound-fetch
 * request writer.
 *
 * Returns 0 with *buffer_out set to an ArrayBuffer jsval on success,
 * or to jsval_undefined() if there is no body. Returns -1 with
 * errno=ENOTSUP if the body is a pending streaming source (streaming
 * serialization is deferred). Returns -1 with errno=EINVAL on kind
 * mismatch.
 */
int jsval_request_body_snapshot(jsval_region_t *region,
		jsval_t request, jsval_t *buffer_out);
int jsval_response_body_snapshot(jsval_region_t *region,
		jsval_t response, jsval_t *buffer_out);

/*
 * Eagerly drain a streaming Request body into the in-memory
 * `body_buffer` ArrayBuffer, converting the Request from streaming
 * to eager without flipping `body_used`. After this returns, the
 * handler can call request.text() / .json() / .arrayBuffer() /
 * .bytes() and they will all resolve synchronously against the
 * drained bytes.
 *
 * If the Request already has an eager body or no body at all, this
 * is a no-op that returns 0.
 *
 * The drain is driven by running the microtask queue locally until
 * the internal drain promise settles. The caller must not be inside
 * a jsval_microtask_drain already; this function is intended to run
 * before the handler is invoked.
 */
int jsval_request_prewarm_body(jsval_region_t *region,
		jsval_t request, jsmethod_error_t *error);

int jsval_response_new_from_parts(jsval_region_t *region,
		uint16_t status,
		jsval_t status_text_value,
		jsval_t headers_value,
		const jsval_body_source_vtable_t *body_vtable,
		void *body_userdata,
		size_t content_length_hint,
		jsval_t *value_ptr);

struct jsval_url_parts {
	char scheme[8];
	char host[256];
	char port[8];
	char path[2048];
	int is_tls;
};

int jsval_url_extract(jsval_region_t *region, jsval_t url_string,
		struct jsval_url_parts *parts);

#endif
