#include "jsval.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include "jsnum.h"
#include "jsurl.h"
#include "jsregex.h"
#include "utf8.h"

#define JSVAL_ALIGN sizeof(void *)
#define JSVAL_JSON_EMIT_MAX_DEPTH 256
#define JSVAL_METHOD_CASE_EXPANSION_MAX 3u

typedef struct jsval_native_string_s {
	size_t len;
	size_t cap;
} jsval_native_string_t;

typedef struct jsval_native_object_s {
	size_t len;
	size_t cap;
} jsval_native_object_t;

typedef struct jsval_native_array_s {
	size_t len;
	size_t cap;
} jsval_native_array_t;

typedef struct jsval_native_set_s {
	size_t len;
	size_t cap;
} jsval_native_set_t;

typedef struct jsval_native_map_s {
	size_t len;
	size_t cap;
} jsval_native_map_t;

typedef struct jsval_native_map_entry_s {
	jsval_t key;
	jsval_t value;
} jsval_native_map_entry_t;

typedef enum jsval_iterator_mode_e {
	JSVAL_ITERATOR_MODE_ARRAY_VALUES = 0,
	JSVAL_ITERATOR_MODE_ARRAY_KEYS = 1,
	JSVAL_ITERATOR_MODE_ARRAY_ENTRIES = 2,
	JSVAL_ITERATOR_MODE_SET_VALUES = 3,
	JSVAL_ITERATOR_MODE_SET_KEYS = 4,
	JSVAL_ITERATOR_MODE_SET_ENTRIES = 5,
	JSVAL_ITERATOR_MODE_MAP_KEYS = 6,
	JSVAL_ITERATOR_MODE_MAP_VALUES = 7,
	JSVAL_ITERATOR_MODE_MAP_ENTRIES = 8,
	JSVAL_ITERATOR_MODE_STRING_VALUES = 9,
	JSVAL_ITERATOR_MODE_STRING_KEYS = 10,
	JSVAL_ITERATOR_MODE_STRING_ENTRIES = 11
} jsval_iterator_mode_t;

typedef struct jsval_native_iterator_s {
	jsval_t source_value;
	size_t cursor;
	uint8_t mode;
	uint8_t done;
	uint8_t reserved[6];
} jsval_native_iterator_t;

typedef struct jsval_native_url_field_s {
	jsval_off_t off;
	size_t len;
	size_t cap;
} jsval_native_url_field_t;

typedef struct jsval_native_url_s {
	jsval_native_url_field_t protocol;
	jsval_native_url_field_t username;
	jsval_native_url_field_t password;
	jsval_native_url_field_t hostname;
	jsval_native_url_field_t port;
	jsval_native_url_field_t pathname;
	jsval_native_url_field_t search;
	jsval_native_url_field_t hash;
	jsval_native_url_field_t host;
	jsval_native_url_field_t origin;
	jsval_native_url_field_t href;
	size_t search_params_cap;
	jsval_t search_params_value;
	uint8_t has_authority;
	uint8_t reserved[7];
} jsval_native_url_t;

typedef struct jsval_native_url_search_params_s {
	jsval_native_url_field_t body;
	size_t params_cap;
	jsval_off_t owner_off;
	uint8_t attached;
	uint8_t reserved[7];
} jsval_native_url_search_params_t;

typedef struct jsval_native_regexp_s {
	jsval_off_t source_off;
	jsval_off_t named_groups_off;
	uint32_t flags;
	uint32_t capture_count;
	uint32_t named_group_count;
	size_t last_index;
	uintptr_t backend_code;
} jsval_native_regexp_t;

typedef struct jsval_native_named_group_s {
	jsval_off_t name_off;
	uint32_t capture_index;
} jsval_native_named_group_t;

typedef enum jsval_match_iterator_mode_e {
	JSVAL_MATCH_ITERATOR_MODE_REGEX = 0,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SURROGATE = 1,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SEQUENCE = 2,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_CLASS = 3,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_CLASS = 4,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_RANGE_CLASS = 5,
	JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_RANGE_CLASS = 6,
	JSVAL_MATCH_ITERATOR_MODE_U_PREDEFINED_CLASS = 7
} jsval_match_iterator_mode_t;

typedef struct jsval_native_match_iterator_s {
	jsval_t search_value;
	jsval_t input_value;
	size_t cursor;
	uint16_t surrogate_unit;
	uint8_t done;
	uint8_t mode;
	uint8_t reserved[4];
} jsval_native_match_iterator_t;

typedef struct jsval_native_prop_s {
	jsval_off_t name_off;
	jsval_t value;
} jsval_native_prop_t;

typedef struct jsval_object_copy_action_s {
	jsval_t key;
	jsval_t value;
	size_t index;
	jsval_off_t name_off;
	uint8_t append;
	uint8_t reserved[7];
} jsval_object_copy_action_t;

typedef struct jsval_json_doc_s {
	jsval_off_t json_off;
	size_t json_len;
	jsval_off_t tokens_off;
	unsigned int tokcap;
	unsigned int tokused;
	int root_i;
} jsval_json_doc_t;

typedef struct jsval_json_emit_state_s {
	uint8_t *buf;
	size_t cap;
	size_t len;
	jsval_off_t stack[JSVAL_JSON_EMIT_MAX_DEPTH];
	size_t depth;
} jsval_json_emit_state_t;

static int jsval_stringify_value_to_native(jsval_region_t *region, jsval_t value,
		int require_object_coercible, jsval_t *string_value_ptr,
		jsmethod_error_t *error);

static size_t jsval_align_up(size_t value, size_t align)
{
	size_t mask = align - 1;
	return (value + mask) & ~mask;
}

static size_t jsval_pages_head_size_aligned(void)
{
	return jsval_align_up(sizeof(jsval_pages_t), JSVAL_ALIGN);
}

static int jsval_region_valid(const jsval_region_t *region)
{
	return region != NULL &&
	       region->base != NULL &&
	       region->pages != NULL &&
	       region->pages->magic == JSVAL_PAGES_MAGIC &&
	       region->pages->version == JSVAL_PAGES_VERSION &&
	       region->pages->header_size == sizeof(jsval_pages_t);
}

static void *jsval_region_ptr(jsval_region_t *region, jsval_off_t off)
{
	if (!jsval_region_valid(region) || off > region->pages->total_len) {
		return NULL;
	}
	return region->base + off;
}

static int jsval_region_reserve(jsval_region_t *region, size_t len, size_t align, jsval_off_t *off_ptr, void **ptr_ptr)
{
	size_t start;
	size_t stop;

	if (!jsval_region_valid(region) || align == 0) {
		errno = EINVAL;
		return -1;
	}

	start = jsval_align_up(region->pages->used_len, align);
	stop = start + len;
	if (stop < start || stop > region->pages->total_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (stop > UINT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

	if (off_ptr != NULL) {
		*off_ptr = (jsval_off_t)start;
	}
	if (ptr_ptr != NULL) {
		*ptr_ptr = region->base + start;
	}
	region->pages->used_len = (uint32_t)stop;
	region->used = stop;
	return 0;
}

static int jsval_region_measure_reserve(const jsval_region_t *region,
		size_t *used_ptr, size_t len, size_t align)
{
	size_t start;
	size_t stop;

	if (!jsval_region_valid(region) || used_ptr == NULL || align == 0) {
		errno = EINVAL;
		return -1;
	}

	start = jsval_align_up(*used_ptr, align);
	stop = start + len;
	if (stop < start || stop > region->pages->total_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (stop > UINT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

	*used_ptr = stop;
	return 0;
}

static jsval_native_string_t *jsval_native_string(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_STRING) {
		return NULL;
	}
	return (jsval_native_string_t *)jsval_region_ptr(region, value.off);
}

static uint16_t *jsval_native_string_units(jsval_native_string_t *string)
{
	return (uint16_t *)(string + 1);
}

static jsval_native_object_t *jsval_native_object(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_OBJECT) {
		return NULL;
	}
	return (jsval_native_object_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_prop_t *jsval_native_object_props(jsval_native_object_t *object)
{
	return (jsval_native_prop_t *)(object + 1);
}

static jsval_native_array_t *jsval_native_array(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_ARRAY) {
		return NULL;
	}
	return (jsval_native_array_t *)jsval_region_ptr(region, value.off);
}

static jsval_t *jsval_native_array_values(jsval_native_array_t *array)
{
	return (jsval_t *)(array + 1);
}

static jsval_native_set_t *jsval_native_set(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_SET) {
		return NULL;
	}
	return (jsval_native_set_t *)jsval_region_ptr(region, value.off);
}

static jsval_t *jsval_native_set_values(jsval_native_set_t *set)
{
	return (jsval_t *)(set + 1);
}

static jsval_native_map_t *jsval_native_map(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_MAP) {
		return NULL;
	}
	return (jsval_native_map_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_map_entry_t *jsval_native_map_entries(jsval_native_map_t *map)
{
	return (jsval_native_map_entry_t *)(map + 1);
}

static jsval_native_iterator_t *jsval_native_iterator(jsval_region_t *region,
		jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_ITERATOR) {
		return NULL;
	}
	return (jsval_native_iterator_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_url_t *jsval_native_url(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_URL) {
		return NULL;
	}
	return (jsval_native_url_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_url_search_params_t *
jsval_native_url_search_params(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE
			|| value.kind != JSVAL_KIND_URL_SEARCH_PARAMS) {
		return NULL;
	}
	return (jsval_native_url_search_params_t *)jsval_region_ptr(region,
			value.off);
}

static int jsval_kind_is_object_like(uint8_t kind)
{
	switch ((jsval_kind_t)kind) {
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		return 1;
	default:
		return 0;
	}
}

static jsstr8_t
jsval_url_field_storage(jsval_region_t *region,
		const jsval_native_url_field_t *field)
{
	jsstr8_t value;
	uint8_t *bytes;

	jsstr8_init(&value);
	if (field == NULL || field->cap == 0) {
		return value;
	}
	bytes = (uint8_t *)jsval_region_ptr(region, field->off);
	if (bytes == NULL) {
		return value;
	}
	jsstr8_init_from_buf(&value, (const char *)bytes, field->cap);
	return value;
}

static jsstr8_t
jsval_url_field_value(jsval_region_t *region,
		const jsval_native_url_field_t *field)
{
	jsstr8_t value = jsval_url_field_storage(region, field);

	value.len = field != NULL ? field->len : 0;
	if (value.len > value.cap) {
		value.len = value.cap;
	}
	return value;
}

static jsstr8_t jsval_url_query_body(jsstr8_t search)
{
	if (search.len > 0 && search.bytes[0] == '?') {
		jsstr8_t body;

		jsstr8_init(&body);
		body.bytes = search.bytes + 1;
		body.len = search.len - 1;
		body.cap = body.len;
		return body;
	}
	return search;
}

static int jsval_url_field_alloc(jsval_region_t *region, size_t cap,
		jsval_native_url_field_t *field)
{
	void *ptr = NULL;

	if (field == NULL) {
		errno = EINVAL;
		return -1;
	}
	memset(field, 0, sizeof(*field));
	if (cap == 0) {
		return 0;
	}
	if (jsval_region_reserve(region, cap, 1, &field->off, &ptr) < 0) {
		return -1;
	}
	field->cap = cap;
	field->len = 0;
	return 0;
}

static int jsval_url_cap_with_extra(size_t len, size_t extra, size_t min_cap,
		size_t *cap_ptr)
{
	size_t cap;

	if (cap_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (len > SIZE_MAX - extra) {
		errno = EOVERFLOW;
		return -1;
	}
	cap = len + extra;
	if (cap < min_cap) {
		cap = min_cap;
	}
	*cap_ptr = cap;
	return 0;
}

static int jsval_url_apply_default_caps(const jsurl_sizes_t *exact,
		jsurl_sizes_t *caps_ptr)
{
	if (exact == NULL || caps_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_cap_with_extra(exact->protocol_cap, 8, 8,
			&caps_ptr->protocol_cap) < 0
			|| jsval_url_cap_with_extra(exact->username_cap, 16, 16,
				&caps_ptr->username_cap) < 0
			|| jsval_url_cap_with_extra(exact->password_cap, 16, 16,
				&caps_ptr->password_cap) < 0
			|| jsval_url_cap_with_extra(exact->hostname_cap, 32, 64,
				&caps_ptr->hostname_cap) < 0
			|| jsval_url_cap_with_extra(exact->port_cap, 16, 16,
				&caps_ptr->port_cap) < 0
			|| jsval_url_cap_with_extra(exact->pathname_cap, 64, 64,
				&caps_ptr->pathname_cap) < 0
			|| jsval_url_cap_with_extra(exact->search_cap, 64, 64,
				&caps_ptr->search_cap) < 0
			|| jsval_url_cap_with_extra(exact->hash_cap, 32, 32,
				&caps_ptr->hash_cap) < 0
			|| jsval_url_cap_with_extra(exact->host_cap, 32, 64,
				&caps_ptr->host_cap) < 0
			|| jsval_url_cap_with_extra(exact->origin_cap, 32, 64,
				&caps_ptr->origin_cap) < 0
			|| jsval_url_cap_with_extra(exact->href_cap, 64, 128,
				&caps_ptr->href_cap) < 0) {
		return -1;
	}
	caps_ptr->search_params_storage_cap = 0;
	if (exact->search_params_cap > SIZE_MAX - 4) {
		errno = EOVERFLOW;
		return -1;
	}
	caps_ptr->search_params_cap = exact->search_params_cap + 4;
	if (caps_ptr->search_params_cap < 4) {
		caps_ptr->search_params_cap = 4;
	}
	return 0;
}

static int jsval_url_search_params_default_caps(size_t body_len,
		size_t params_len, size_t *body_cap_ptr, size_t *params_cap_ptr)
{
	if (body_cap_ptr == NULL || params_cap_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_cap_with_extra(body_len, 64, 64, body_cap_ptr) < 0) {
		return -1;
	}
	if (params_len > SIZE_MAX - 4) {
		errno = EOVERFLOW;
		return -1;
	}
	*params_cap_ptr = params_len + 4;
	if (*params_cap_ptr < 4) {
		*params_cap_ptr = 4;
	}
	return 0;
}

static int jsval_url_init_storage(jsval_region_t *region,
		jsval_native_url_t *native, jsurl_t *url, jsurl_param_t *params_buf,
		uint8_t *storage_buf, size_t storage_cap)
{
	jsurl_storage_t storage;

	if (region == NULL || native == NULL || url == NULL
			|| (native->search_params_cap > 0 && params_buf == NULL)
			|| (storage_cap > 0 && storage_buf == NULL)) {
		errno = EINVAL;
		return -1;
	}
	memset(&storage, 0, sizeof(storage));
	storage.protocol = jsval_url_field_storage(region, &native->protocol);
	storage.username = jsval_url_field_storage(region, &native->username);
	storage.password = jsval_url_field_storage(region, &native->password);
	storage.hostname = jsval_url_field_storage(region, &native->hostname);
	storage.port = jsval_url_field_storage(region, &native->port);
	storage.pathname = jsval_url_field_storage(region, &native->pathname);
	storage.search = jsval_url_field_storage(region, &native->search);
	storage.hash = jsval_url_field_storage(region, &native->hash);
	storage.host = jsval_url_field_storage(region, &native->host);
	storage.origin = jsval_url_field_storage(region, &native->origin);
	storage.href = jsval_url_field_storage(region, &native->href);
	storage.search_params.params = native->search_params_cap > 0 ? params_buf : NULL;
	storage.search_params.params_cap = native->search_params_cap;
	jsstr8_init_from_buf(&storage.search_params.storage,
			(const char *)storage_buf, storage_cap);
	return jsurl_init(url, &storage);
}

static int jsval_url_load_current(jsval_region_t *region,
		jsval_native_url_t *native, jsurl_t *url, jsurl_param_t *params_buf,
		uint8_t *storage_buf, size_t storage_cap)
{
	jsurl_view_t view;

	if (jsval_url_init_storage(region, native, url, params_buf, storage_buf,
			storage_cap) < 0) {
		return -1;
	}
	jsurl_view_clear(&view);
	view.href = jsval_url_field_value(region, &native->href);
	view.protocol = jsval_url_field_value(region, &native->protocol);
	view.username = jsval_url_field_value(region, &native->username);
	view.password = jsval_url_field_value(region, &native->password);
	view.hostname = jsval_url_field_value(region, &native->hostname);
	view.port = jsval_url_field_value(region, &native->port);
	view.pathname = jsval_url_field_value(region, &native->pathname);
	view.search = jsval_url_field_value(region, &native->search);
	view.hash = jsval_url_field_value(region, &native->hash);
	view.has_authority = native->has_authority != 0;
	return jsurl_copy_from_view(url, &view);
}

static void jsval_url_sync_native(jsval_native_url_t *native, const jsurl_t *url)
{
	native->protocol.len = url->protocol.len;
	native->username.len = url->username.len;
	native->password.len = url->password.len;
	native->hostname.len = url->hostname.len;
	native->port.len = url->port.len;
	native->pathname.len = url->pathname.len;
	native->search.len = url->search.len;
	native->hash.len = url->hash.len;
	native->host.len = url->host.len;
	native->origin.len = url->origin.len;
	native->href.len = url->href.len;
	native->has_authority = url->has_authority != 0;
}

static int jsval_url_field_make_string(jsval_region_t *region,
		const jsval_native_url_field_t *field, jsval_t *value_ptr)
{
	jsstr8_t value = jsval_url_field_value(region, field);
	static const uint8_t empty[1] = {0};

	return jsval_string_new_utf8(region, value.len > 0 ? value.bytes : empty,
			value.len, value_ptr);
}

typedef int (*jsval_url_string_measure_fn)(const jsurl_t *url, size_t *len_ptr);
typedef int (*jsval_url_string_serialize_fn)(const jsurl_t *url,
		jsstr8_t *result_ptr);

typedef struct jsval_url_serialized_string_ctx_s {
	jsval_region_t *region;
	jsval_t *value_ptr;
	jsval_url_string_measure_fn measure;
	jsval_url_string_serialize_fn serialize;
} jsval_url_serialized_string_ctx_t;

static int jsval_url_serialized_string_from_callback(const jsurl_t *url,
		void *opaque)
{
	jsval_url_serialized_string_ctx_t *ctx =
		(jsval_url_serialized_string_ctx_t *)opaque;
	size_t len;
	static const uint8_t empty[1] = {0};

	if (url == NULL || ctx == NULL || ctx->region == NULL
			|| ctx->value_ptr == NULL || ctx->measure == NULL
			|| ctx->serialize == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (ctx->measure(url, &len) < 0) {
		return -1;
	}
	if (len == 0) {
		return jsval_string_new_utf8(ctx->region, empty, 0, ctx->value_ptr);
	}
	{
		uint8_t buf[len];
		jsstr8_t out;

		jsstr8_init_from_buf(&out, (const char *)buf, sizeof(buf));
		if (ctx->serialize(url, &out) < 0) {
			return -1;
		}
		return jsval_string_new_utf8(ctx->region, out.bytes, out.len,
				ctx->value_ptr);
	}
}

static int jsval_url_search_params_stringify(jsval_region_t *region,
		jsval_native_url_search_params_t *native, jsval_t *value_ptr);

static int jsval_url_stringify(jsval_region_t *region, jsval_t value,
		jsval_t *value_ptr)
{
	jsmethod_error_t error;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	memset(&error, 0, sizeof(error));
	if (value.kind == JSVAL_KIND_URL) {
		jsval_native_url_t *native = jsval_native_url(region, value);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		return jsval_url_field_make_string(region, &native->href, value_ptr);
	}
	if (value.kind == JSVAL_KIND_URL_SEARCH_PARAMS) {
		jsval_native_url_search_params_t *native =
			jsval_native_url_search_params(region, value);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		return jsval_url_search_params_stringify(region, native, value_ptr);
	}
	return jsval_stringify_value_to_native(region, value, 0, value_ptr, &error);
}

static int jsval_url_copy_string_utf8(jsval_region_t *region, jsval_t value,
		size_t *len_ptr, uint8_t *buf, size_t cap)
{
	jsval_t string_value;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_stringify(region, value, &string_value) < 0) {
		return -1;
	}
	return jsval_string_copy_utf8(region, string_value, buf, cap, len_ptr);
}

typedef int (*jsval_url_callback_fn)(const jsurl_t *url, void *opaque);

static int jsval_url_with_temp_parsed(jsstr8_t input,
		jsval_url_callback_fn callback, void *opaque)
{
	jsurl_view_t view;
	jsurl_sizes_t sizes;

	if (callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_view_parse(&view, input) < 0) {
		return -1;
	}
	if (jsurl_copy_sizes(&view, &sizes) < 0) {
		return -1;
	}
	{
		uint8_t protocol_buf[sizes.protocol_cap > 0 ? sizes.protocol_cap : 1];
		uint8_t username_buf[sizes.username_cap > 0 ? sizes.username_cap : 1];
		uint8_t password_buf[sizes.password_cap > 0 ? sizes.password_cap : 1];
		uint8_t hostname_buf[sizes.hostname_cap > 0 ? sizes.hostname_cap : 1];
		uint8_t port_buf[sizes.port_cap > 0 ? sizes.port_cap : 1];
		uint8_t pathname_buf[sizes.pathname_cap > 0 ? sizes.pathname_cap : 1];
		uint8_t search_buf[sizes.search_cap > 0 ? sizes.search_cap : 1];
		uint8_t hash_buf[sizes.hash_cap > 0 ? sizes.hash_cap : 1];
		uint8_t host_buf[sizes.host_cap > 0 ? sizes.host_cap : 1];
		uint8_t origin_buf[sizes.origin_cap > 0 ? sizes.origin_cap : 1];
		uint8_t href_buf[sizes.href_cap > 0 ? sizes.href_cap : 1];
		jsurl_param_t params_buf[sizes.search_params_cap > 0
			? sizes.search_params_cap : 1];
		uint8_t params_storage_buf[sizes.search_params_storage_cap > 0
			? sizes.search_params_storage_cap : 1];
		jsurl_storage_t storage;
		jsurl_t url;

		memset(&storage, 0, sizeof(storage));
		jsstr8_init_from_buf(&storage.protocol, (const char *)protocol_buf,
				sizeof(protocol_buf));
		jsstr8_init_from_buf(&storage.username, (const char *)username_buf,
				sizeof(username_buf));
		jsstr8_init_from_buf(&storage.password, (const char *)password_buf,
				sizeof(password_buf));
		jsstr8_init_from_buf(&storage.hostname, (const char *)hostname_buf,
				sizeof(hostname_buf));
		jsstr8_init_from_buf(&storage.port, (const char *)port_buf,
				sizeof(port_buf));
		jsstr8_init_from_buf(&storage.pathname, (const char *)pathname_buf,
				sizeof(pathname_buf));
		jsstr8_init_from_buf(&storage.search, (const char *)search_buf,
				sizeof(search_buf));
		jsstr8_init_from_buf(&storage.hash, (const char *)hash_buf,
				sizeof(hash_buf));
		jsstr8_init_from_buf(&storage.host, (const char *)host_buf,
				sizeof(host_buf));
		jsstr8_init_from_buf(&storage.origin, (const char *)origin_buf,
				sizeof(origin_buf));
		jsstr8_init_from_buf(&storage.href, (const char *)href_buf,
				sizeof(href_buf));
		storage.search_params.params = sizes.search_params_cap > 0
			? params_buf : NULL;
		storage.search_params.params_cap = sizes.search_params_cap;
		jsstr8_init_from_buf(&storage.search_params.storage,
				(const char *)params_storage_buf, sizeof(params_storage_buf));
		if (jsurl_init(&url, &storage) < 0) {
			return -1;
		}
		if (jsurl_copy_from_view(&url, &view) < 0) {
			return -1;
		}
		return callback(&url, opaque);
	}
}

static int jsval_url_with_bound_native(jsval_region_t *region,
		jsval_native_url_t *native, jsval_url_callback_fn callback,
		void *opaque)
{
	size_t params_cap;
	size_t storage_cap;
	jsurl_param_t *params_buf;
	uint8_t *storage_buf;
	jsurl_t url;

	if (region == NULL || native == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	params_cap = native->search_params_cap > 0 ? native->search_params_cap : 1;
	storage_cap = native->search.cap > 0 ? native->search.cap : 1;
	{
		jsurl_param_t params_vla[params_cap];
		uint8_t storage_vla[storage_cap];

		params_buf = params_vla;
		storage_buf = storage_vla;
		if (jsval_url_load_current(region, native, &url, params_buf, storage_buf,
				storage_cap) < 0) {
			return -1;
		}
		return callback(&url, opaque);
	}
}

static int jsval_url_make_callback_string(jsval_region_t *region,
		jsval_t url_value, jsval_url_string_measure_fn measure,
		jsval_url_string_serialize_fn serialize, jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);
	jsval_url_serialized_string_ctx_t ctx;

	if (native == NULL || value_ptr == NULL || measure == NULL
			|| serialize == NULL) {
		errno = EINVAL;
		return -1;
	}
	ctx.region = region;
	ctx.value_ptr = value_ptr;
	ctx.measure = measure;
	ctx.serialize = serialize;
	return jsval_url_with_bound_native(region, native,
			jsval_url_serialized_string_from_callback, &ctx);
}

static jsval_native_regexp_t *jsval_native_regexp(jsval_region_t *region,
		jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE || value.kind != JSVAL_KIND_REGEXP) {
		return NULL;
	}
	return (jsval_native_regexp_t *)jsval_region_ptr(region, value.off);
}

static jsval_native_named_group_t *jsval_native_regexp_named_groups(
		jsval_region_t *region, jsval_native_regexp_t *regexp)
{
	if (regexp == NULL || regexp->named_group_count == 0) {
		return NULL;
	}
	return (jsval_native_named_group_t *)jsval_region_ptr(region,
			regexp->named_groups_off);
}

static int jsval_object_append_native_prop(jsval_region_t *region, jsval_t object,
		jsval_off_t name_off, jsval_t value)
{
	jsval_native_object_t *native = jsval_native_object(region, object);
	jsval_native_prop_t *props;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}
	props = jsval_native_object_props(native);
	props[native->len].name_off = name_off;
	props[native->len].value = value;
	native->len++;
	return 0;
}

static int
jsval_regexp_named_group_capture_index(jsval_region_t *region,
		jsval_native_regexp_t *regexp, const uint16_t *name, size_t name_len,
		size_t *capture_index_ptr)
{
	jsval_native_named_group_t *named_groups;
	size_t i;

	if (regexp == NULL || capture_index_ptr == NULL
			|| (name_len > 0 && name == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (regexp->named_group_count == 0) {
		errno = ENOENT;
		return -1;
	}

	named_groups = jsval_native_regexp_named_groups(region, regexp);
	if (named_groups == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < regexp->named_group_count; i++) {
		jsval_t group_name;
		jsval_native_string_t *group_name_native;

		group_name = jsval_undefined();
		group_name.kind = JSVAL_KIND_STRING;
		group_name.repr = JSVAL_REPR_NATIVE;
		group_name.off = named_groups[i].name_off;
		group_name_native = jsval_native_string(region, group_name);
		if (group_name_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (group_name_native->len == name_len
				&& (name_len == 0 || memcmp(
					jsval_native_string_units(group_name_native), name,
					name_len * sizeof(name[0])) == 0)) {
			*capture_index_ptr = named_groups[i].capture_index;
			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

static jsval_native_match_iterator_t *jsval_native_match_iterator(
		jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_NATIVE
			|| value.kind != JSVAL_KIND_MATCH_ITERATOR) {
		return NULL;
	}
	return (jsval_native_match_iterator_t *)jsval_region_ptr(region, value.off);
}

static jsval_json_doc_t *jsval_json_doc(jsval_region_t *region, jsval_t value)
{
	if (value.repr != JSVAL_REPR_JSON) {
		return NULL;
	}
	return (jsval_json_doc_t *)jsval_region_ptr(region, value.off);
}

static const uint8_t *jsval_json_doc_bytes(jsval_region_t *region, jsval_json_doc_t *doc)
{
	return (const uint8_t *)jsval_region_ptr(region, doc->json_off);
}

static jsmntok_t *jsval_json_doc_tokens(jsval_region_t *region, jsval_json_doc_t *doc)
{
	return (jsmntok_t *)jsval_region_ptr(region, doc->tokens_off);
}

static size_t jsval_utf16_units_for_codepoint(uint32_t codepoint)
{
	if (codepoint < 0x10000) {
		return 1;
	}
	return 2;
}

static size_t jsval_utf8_utf16len(const uint8_t *str, size_t len)
{
	size_t out_len = 0;
	const uint8_t *cursor = str;
	const uint8_t *stop = str + len;

	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF8_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		out_len += jsval_utf16_units_for_codepoint(codepoint);
		cursor += seq_len;
	}

	return out_len;
}

static size_t jsval_utf16_utf8len(const uint16_t *str, size_t len)
{
	size_t out_len = 0;
	const uint16_t *cursor = str;
	const uint16_t *stop = str + len;

	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		out_len += UTF8_CLEN(codepoint);
		cursor += seq_len;
	}

	return out_len;
}

static int jsval_native_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *cursor;
	const uint16_t *stop;
	uint8_t *write;
	uint8_t *write_stop;
	size_t needed;

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}

	needed = jsval_utf16_utf8len(jsval_native_string_units(string), string->len);
	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < needed) {
		errno = ENOBUFS;
		return -1;
	}

	cursor = jsval_native_string_units(string);
	stop = cursor + string->len;
	write = buf;
	write_stop = buf + cap;
	while (cursor < stop && write < write_stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;
		const uint32_t *codepoint_ptr = &codepoint;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		UTF8_ENCODE(&codepoint_ptr, codepoint_ptr + 1, &write, write_stop);
		cursor += seq_len;
	}

	return 0;
}

static int
jsval_object_set_string_key(jsval_region_t *region, jsval_t object,
		jsval_t key, jsval_t value)
{
	size_t key_len = 0;

	if (region == NULL || key.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_copy_utf8(region, key, NULL, 0, &key_len) < 0) {
		return -1;
	}
	{
		uint8_t key_buf[key_len ? key_len : 1];

		if (key_len > 0 && jsval_string_copy_utf8(region, key, key_buf,
				key_len, NULL) < 0) {
			return -1;
		}
		return jsval_object_set_utf8(region, object, key_len > 0 ? key_buf : NULL,
				key_len, value);
	}
}

static int jsval_native_string_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_native_string_t *string = jsval_native_string(region, value);

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (len_ptr != NULL) {
		*len_ptr = string->len;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < string->len) {
		errno = ENOBUFS;
		return -1;
	}
	if (string->len > 0) {
		memcpy(buf, jsval_native_string_units(string), string->len * sizeof(uint16_t));
	}
	return 0;
}

static int jsval_native_string_eq_utf8(jsval_region_t *region, jsval_t value, const uint8_t *key, size_t key_len)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *left;
	const uint16_t *left_stop;
	const uint8_t *right;
	const uint8_t *right_stop;

	if (string == NULL) {
		return 0;
	}

	left = jsval_native_string_units(string);
	left_stop = left + string->len;
	right = key;
	right_stop = key + key_len;
	while (left < left_stop && right < right_stop) {
		uint32_t left_cp = 0;
		uint32_t right_cp = 0;
		int left_len = 0;
		int right_len = 0;

		UTF16_CHAR(left, left_stop, &left_cp, &left_len);
		UTF8_CHAR(right, right_stop, &right_cp, &right_len);
		if (left_len == 0 || right_len == 0) {
			return 0;
		}
		if (left_len < 0) {
			left_len = -left_len;
			left_cp = 0xFFFD;
		}
		if (right_len < 0) {
			right_len = -right_len;
			right_cp = 0xFFFD;
		}
		if (left_cp != right_cp) {
			return 0;
		}

		left += left_len;
		right += right_len;
	}

	return left == left_stop && right == right_stop;
}

static int jsval_json_token_range(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, const uint8_t **start_ptr, const uint8_t **stop_ptr)
{
	const uint8_t *json;
	jsmntok_t *tokens;
	jsmntok_t *token;

	if (doc == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= doc->tokused) {
		errno = EINVAL;
		return -1;
	}

	json = jsval_json_doc_bytes(region, doc);
	tokens = jsval_json_doc_tokens(region, doc);
	token = &tokens[index];
	if (token->start < 0 || token->end < token->start) {
		errno = EINVAL;
		return -1;
	}

	*start_ptr = json + token->start;
	*stop_ptr = json + token->end;
	return 0;
}

static jsval_kind_t jsval_json_token_kind(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index)
{
	const uint8_t *start;
	const uint8_t *stop;
	jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
	jsmntok_t *token = &tokens[index];

	switch (token->type) {
	case JSMN_STRING:
		return JSVAL_KIND_STRING;
	case JSMN_OBJECT:
		return JSVAL_KIND_OBJECT;
	case JSMN_ARRAY:
		return JSVAL_KIND_ARRAY;
	case JSMN_PRIMITIVE:
		if (jsval_json_token_range(region, doc, index, &start, &stop) < 0 || start >= stop) {
			return JSVAL_KIND_UNDEFINED;
		}
		if (*start == 'n') {
			return JSVAL_KIND_NULL;
		}
		if (*start == 't' || *start == 'f') {
			return JSVAL_KIND_BOOL;
		}
		return JSVAL_KIND_NUMBER;
	default:
		return JSVAL_KIND_UNDEFINED;
	}
}

static jsval_t jsval_json_value(jsval_region_t *region, jsval_t value, uint32_t index)
{
	jsval_json_doc_t *doc = jsval_json_doc(region, value);
	jsval_t out = jsval_undefined();

	out.kind = jsval_json_token_kind(region, doc, index);
	out.repr = JSVAL_REPR_JSON;
	out.off = value.off;
	out.as.index = index;
	return out;
}

static int jsval_json_next(jsval_region_t *region, jsval_json_doc_t *doc, int index)
{
	int next;
	unsigned int i;
	jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

	if (index < 0 || (unsigned int)index >= doc->tokused) {
		return -1;
	}

	next = index + 1;
	switch (tokens[index].type) {
	case JSMN_STRING:
	case JSMN_PRIMITIVE:
		return next;
	case JSMN_ARRAY:
		for (i = 0; i < (unsigned int)tokens[index].size; i++) {
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
		}
		return next;
	case JSMN_OBJECT:
		for (i = 0; i < (unsigned int)tokens[index].size; i++) {
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
			next = jsval_json_next(region, doc, next);
			if (next < 0) {
				return -1;
			}
		}
		return next;
	default:
		return -1;
	}
}

static int jsval_json_bool_value(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, int *boolean_ptr)
{
	const uint8_t *start;
	const uint8_t *stop;
	size_t len;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_BOOL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &start, &stop) < 0) {
		return -1;
	}

	len = (size_t)(stop - start);
	if (len == 4 && memcmp(start, "true", 4) == 0) {
		*boolean_ptr = 1;
		return 0;
	}
	if (len == 5 && memcmp(start, "false", 5) == 0) {
		*boolean_ptr = 0;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_json_number_value(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, double *number_ptr)
{
	const uint8_t *start;
	const uint8_t *stop;
	size_t len;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_NUMBER) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &start, &stop) < 0) {
		return -1;
	}

	len = (size_t)(stop - start);
	return jsnum_parse_json(start, len, number_ptr);
}

static int jsval_json_string_copy_utf32(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint32_t *buf, size_t cap, size_t *len_ptr)
{
	const uint8_t *pos_cursor;
	const uint8_t *pos_stop;
	uint32_t q32[128];
	uint32_t val32[128];
	uint32_t *q32_out;
	uint32_t *q32_in;
	uint32_t *val32_out;
	uint32_t *write;
	uint32_t *write_stop;
	size_t needed = 0;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &pos_cursor, &pos_stop) < 0) {
		return -1;
	}

	write = buf;
	write_stop = buf == NULL ? NULL : buf + cap;
	while (pos_cursor < pos_stop) {
		q32_out = q32;
		q32_in = q32;
		val32_out = val32;

		UTF8_DECODE(&pos_cursor, pos_stop, &q32_out, q32 + 128);
		JSMN_UNQUOTE((const uint32_t **)&q32_in, (const uint32_t *)q32_out, &val32_out, val32 + 128);

		needed += (size_t)(val32_out - val32);
		if (buf != NULL) {
			size_t chunk = (size_t)(val32_out - val32);
			if ((size_t)(write_stop - write) < chunk) {
				errno = ENOBUFS;
				return -1;
			}
			memcpy(write, val32, chunk * sizeof(uint32_t));
			write += chunk;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	return 0;
}

static int jsval_json_string_copy_utf16(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	size_t utf32_len;
	size_t i;

	if (jsval_json_string_copy_utf32(region, doc, index, NULL, 0, &utf32_len) < 0) {
		return -1;
	}

	if (buf == NULL) {
		if (len_ptr != NULL) {
			uint32_t utf32_buf[utf32_len ? utf32_len : 1];
			if (jsval_json_string_copy_utf32(region, doc, index, utf32_buf, utf32_len, NULL) < 0) {
				return -1;
			}
			*len_ptr = 0;
			for (i = 0; i < utf32_len; i++) {
				*len_ptr += jsval_utf16_units_for_codepoint(utf32_buf[i]);
			}
		}
		return 0;
	}

	{
		uint32_t utf32_buf[utf32_len ? utf32_len : 1];
		uint16_t *write = buf;
		uint16_t *write_stop = buf + cap;
		const uint32_t *read = utf32_buf;
		const uint32_t *read_stop = utf32_buf + utf32_len;

		if (jsval_json_string_copy_utf32(region, doc, index, utf32_buf, utf32_len, NULL) < 0) {
			return -1;
		}
		for (i = 0; i < utf32_len; i++) {
			if ((size_t)(write_stop - write) < jsval_utf16_units_for_codepoint(utf32_buf[i])) {
				errno = ENOBUFS;
				return -1;
			}
		}
		UTF16_ENCODE(&read, read_stop, &write, write_stop);
		if (len_ptr != NULL) {
			*len_ptr = (size_t)(write - buf);
		}
		return 0;
	}
}

static int jsval_json_string_copy_utf8_internal(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	const uint8_t *pos_cursor;
	const uint8_t *pos_stop;
	uint32_t q32[128];
	uint32_t val32[128];
	uint32_t *q32_out;
	uint32_t *q32_in;
	uint32_t *val32_out;
	const uint32_t *val32_in;
	uint8_t *write;
	uint8_t *write_stop;
	size_t needed = 0;

	if (jsval_json_token_kind(region, doc, index) != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, index, &pos_cursor, &pos_stop) < 0) {
		return -1;
	}

	write = buf;
	write_stop = buf == NULL ? NULL : buf + cap;
	while (pos_cursor < pos_stop) {
		uint8_t scratch[512];
		uint8_t *scratch_write = scratch;

		q32_out = q32;
		q32_in = q32;
		val32_out = val32;
		val32_in = val32;

		UTF8_DECODE(&pos_cursor, pos_stop, &q32_out, q32 + 128);
		JSMN_UNQUOTE((const uint32_t **)&q32_in, (const uint32_t *)q32_out, &val32_out, val32 + 128);
		UTF8_ENCODE(&val32_in, val32_out, &scratch_write, scratch + sizeof(scratch));

		needed += (size_t)(scratch_write - scratch);
		if (buf != NULL) {
			size_t chunk = (size_t)(scratch_write - scratch);
			if ((size_t)(write_stop - write) < chunk) {
				errno = ENOBUFS;
				return -1;
			}
			memcpy(write, scratch, chunk);
			write += chunk;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = needed;
	}
	return 0;
}

static int jsval_json_string_eq_utf8(jsval_region_t *region, jsval_json_doc_t *doc, uint32_t index, const uint8_t *key, size_t key_len)
{
	size_t len;

	if (jsval_json_string_copy_utf8_internal(region, doc, index, NULL, 0, &len) < 0) {
		return 0;
	}
	if (len != key_len) {
		return 0;
	}

	{
		uint8_t buf[key_len ? key_len : 1];
		if (jsval_json_string_copy_utf8_internal(region, doc, index, buf, key_len, NULL) < 0) {
			return 0;
		}
		return memcmp(buf, key, key_len) == 0;
	}
}

static int jsval_json_emit_append(jsval_json_emit_state_t *state, const uint8_t *src, size_t len)
{
	if (SIZE_MAX - state->len < len) {
		errno = EOVERFLOW;
		return -1;
	}
	if (state->buf != NULL) {
		if (state->len + len > state->cap) {
			errno = ENOBUFS;
			return -1;
		}
		if (len > 0) {
			memcpy(state->buf + state->len, src, len);
		}
	}
	state->len += len;
	return 0;
}

static int jsval_json_emit_byte(jsval_json_emit_state_t *state, uint8_t byte)
{
	return jsval_json_emit_append(state, &byte, 1);
}

static int jsval_json_emit_ascii(jsval_json_emit_state_t *state, const char *text)
{
	return jsval_json_emit_append(state, (const uint8_t *)text, strlen(text));
}

static int jsval_json_emit_u16_escape(jsval_json_emit_state_t *state, uint16_t codeunit)
{
	static const char hex[] = "0123456789ABCDEF";
	uint8_t escape[6];

	escape[0] = '\\';
	escape[1] = 'u';
	escape[2] = (uint8_t)hex[(codeunit >> 12) & 0x0F];
	escape[3] = (uint8_t)hex[(codeunit >> 8) & 0x0F];
	escape[4] = (uint8_t)hex[(codeunit >> 4) & 0x0F];
	escape[5] = (uint8_t)hex[codeunit & 0x0F];
	return jsval_json_emit_append(state, escape, sizeof(escape));
}

static int jsval_json_emit_push(jsval_json_emit_state_t *state, jsval_t value)
{
	size_t i;

	if (value.repr != JSVAL_REPR_NATIVE ||
	    (value.kind != JSVAL_KIND_OBJECT && value.kind != JSVAL_KIND_ARRAY)) {
		return 0;
	}

	for (i = 0; i < state->depth; i++) {
		if (state->stack[i] == value.off) {
			errno = ELOOP;
			return -1;
		}
	}
	if (state->depth >= JSVAL_JSON_EMIT_MAX_DEPTH) {
		errno = EOVERFLOW;
		return -1;
	}

	state->stack[state->depth++] = value.off;
	return 0;
}

static void jsval_json_emit_pop(jsval_json_emit_state_t *state, jsval_t value)
{
	if (value.repr == JSVAL_REPR_NATIVE &&
	    (value.kind == JSVAL_KIND_OBJECT || value.kind == JSVAL_KIND_ARRAY) &&
	    state->depth > 0) {
		state->depth--;
	}
}

static int jsval_json_emit_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state);
static int jsval_stringify_value_to_native(jsval_region_t *region, jsval_t value,
		int require_object_coercible, jsval_t *string_value_ptr,
		jsmethod_error_t *error);

static int jsval_json_emit_native_string(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	jsval_native_string_t *string = jsval_native_string(region, value);
	const uint16_t *cursor;
	const uint16_t *stop;

	if (string == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_emit_byte(state, '"') < 0) {
		return -1;
	}

	cursor = jsval_native_string_units(string);
	stop = cursor + string->len;
	while (cursor < stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;

		UTF16_CHAR(cursor, stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		switch (codepoint) {
		case '"':
			if (jsval_json_emit_ascii(state, "\\\"") < 0) {
				return -1;
			}
			break;
		case '\\':
			if (jsval_json_emit_ascii(state, "\\\\") < 0) {
				return -1;
			}
			break;
		case '\b':
			if (jsval_json_emit_ascii(state, "\\b") < 0) {
				return -1;
			}
			break;
		case '\f':
			if (jsval_json_emit_ascii(state, "\\f") < 0) {
				return -1;
			}
			break;
		case '\n':
			if (jsval_json_emit_ascii(state, "\\n") < 0) {
				return -1;
			}
			break;
		case '\r':
			if (jsval_json_emit_ascii(state, "\\r") < 0) {
				return -1;
			}
			break;
		case '\t':
			if (jsval_json_emit_ascii(state, "\\t") < 0) {
				return -1;
			}
			break;
		default:
			if (codepoint < 0x20) {
				if (jsval_json_emit_u16_escape(state, (uint16_t)codepoint) < 0) {
					return -1;
				}
			} else {
				uint8_t utf8[4];
				uint8_t *write = utf8;
				const uint32_t *read = &codepoint;

				UTF8_ENCODE(&read, read + 1, &write, utf8 + sizeof(utf8));
				if (jsval_json_emit_append(state, utf8, (size_t)(write - utf8)) < 0) {
					return -1;
				}
			}
			break;
		}

		cursor += seq_len;
	}

	return jsval_json_emit_byte(state, '"');
}

static int jsval_json_emit_json_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	jsval_json_doc_t *doc = jsval_json_doc(region, value);
	const uint8_t *start;
	const uint8_t *stop;

	if (doc == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_json_token_range(region, doc, value.as.index, &start, &stop) < 0) {
		return -1;
	}

	if (value.kind == JSVAL_KIND_STRING) {
		if (jsval_json_emit_byte(state, '"') < 0) {
			return -1;
		}
		if (jsval_json_emit_append(state, start, (size_t)(stop - start)) < 0) {
			return -1;
		}
		return jsval_json_emit_byte(state, '"');
	}

	return jsval_json_emit_append(state, start, (size_t)(stop - start));
}

static int jsval_json_emit_value(jsval_region_t *region, jsval_t value, jsval_json_emit_state_t *state)
{
	size_t i;

	if (value.repr == JSVAL_REPR_JSON) {
		return jsval_json_emit_json_value(region, value, state);
	}

	switch (value.kind) {
	case JSVAL_KIND_NULL:
		return jsval_json_emit_ascii(state, "null");
	case JSVAL_KIND_BOOL:
		return jsval_json_emit_ascii(state, value.as.boolean ? "true" : "false");
		case JSVAL_KIND_NUMBER:
			if (!isfinite(value.as.number)) {
				errno = ENOTSUP;
				return -1;
			}
			{
				char numbuf[64];
				size_t len = 0;

				if (jsnum_format(value.as.number, numbuf, sizeof(numbuf), &len) < 0) {
					return -1;
				}
				return jsval_json_emit_append(state, (const uint8_t *)numbuf, len);
			}
	case JSVAL_KIND_STRING:
		return jsval_json_emit_native_string(region, value, state);
	case JSVAL_KIND_ARRAY:
	{
		jsval_native_array_t *array = jsval_native_array(region, value);
		jsval_t *values;

		if (array == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_json_emit_push(state, value) < 0) {
			return -1;
		}
		if (jsval_json_emit_byte(state, '[') < 0) {
			jsval_json_emit_pop(state, value);
			return -1;
		}

		values = jsval_native_array_values(array);
		for (i = 0; i < array->len; i++) {
			if (i > 0 && jsval_json_emit_byte(state, ',') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_value(region, values[i], state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
		}

		jsval_json_emit_pop(state, value);
		return jsval_json_emit_byte(state, ']');
	}
	case JSVAL_KIND_OBJECT:
	{
		jsval_native_object_t *object = jsval_native_object(region, value);
		jsval_native_prop_t *props;

		if (object == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_json_emit_push(state, value) < 0) {
			return -1;
		}
		if (jsval_json_emit_byte(state, '{') < 0) {
			jsval_json_emit_pop(state, value);
			return -1;
		}

		props = jsval_native_object_props(object);
		for (i = 0; i < object->len; i++) {
			jsval_t name = jsval_undefined();

			if (i > 0 && jsval_json_emit_byte(state, ',') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}

			name.kind = JSVAL_KIND_STRING;
			name.repr = JSVAL_REPR_NATIVE;
			name.off = props[i].name_off;
			if (jsval_json_emit_native_string(region, name, state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_byte(state, ':') < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
			if (jsval_json_emit_value(region, props[i].value, state) < 0) {
				jsval_json_emit_pop(state, value);
				return -1;
			}
		}

		jsval_json_emit_pop(state, value);
		return jsval_json_emit_byte(state, '}');
	}
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
	case JSVAL_KIND_UNDEFINED:
	default:
		errno = ENOTSUP;
		return -1;
	}
}

static int jsval_value_utf16_len(jsval_region_t *region, jsval_t value, size_t *len_ptr);
static int jsval_value_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr);

static int jsval_ascii_copy_utf16(const char *text, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	size_t len = strlen(text);
	size_t i;

	if (len_ptr != NULL) {
		*len_ptr = len;
	}
	if (buf == NULL) {
		return 0;
	}
	if (cap < len) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < len; i++) {
		buf[i] = (uint8_t)text[i];
	}
	return 0;
}

static const char *jsval_number_text(double number, char buf[64])
{
	if (jsnum_format(number, buf, 64, NULL) < 0) {
		return NULL;
	}
	return buf;
}

static int jsval_string_reserve_utf16(jsval_region_t *region, size_t cap,
		jsval_t *value_ptr, jsval_native_string_t **string_ptr)
{
	jsval_native_string_t *string;
	jsval_off_t off;
	size_t units_cap = cap > 0 ? cap : 1;
	size_t bytes_len;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (units_cap > (SIZE_MAX - sizeof(*string)) / sizeof(uint16_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	bytes_len = sizeof(*string) + units_cap * sizeof(uint16_t);
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off,
			(void **)&string) < 0) {
		return -1;
	}

	string->len = 0;
	string->cap = units_cap;
	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_STRING;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	if (string_ptr != NULL) {
		*string_ptr = string;
	}
	return 0;
}

static int jsval_string_measure_utf16(const jsval_region_t *region,
		size_t *used_ptr, size_t cap)
{
	size_t units_cap = cap > 0 ? cap : 1;
	size_t bytes_len;

	if (used_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (units_cap > (SIZE_MAX - sizeof(jsval_native_string_t)) / sizeof(uint16_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	bytes_len = sizeof(jsval_native_string_t) + units_cap * sizeof(uint16_t);
	return jsval_region_measure_reserve(region, used_ptr, bytes_len, JSVAL_ALIGN);
}

static int jsval_value_utf16_len(jsval_region_t *region, jsval_t value, size_t *len_ptr)
{
	jsval_json_doc_t *doc;
	char numbuf[64];
	const char *text;

	switch (value.kind) {
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			return jsval_native_string_copy_utf16(region, value, NULL, 0, len_ptr);
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0, len_ptr);
		}
		break;
	case JSVAL_KIND_UNDEFINED:
		return jsval_ascii_copy_utf16("undefined", NULL, 0, len_ptr);
	case JSVAL_KIND_NULL:
		return jsval_ascii_copy_utf16("null", NULL, 0, len_ptr);
	case JSVAL_KIND_BOOL:
		return jsval_ascii_copy_utf16(value.repr == JSVAL_REPR_JSON ? (jsval_truthy(region, value) ? "true" : "false") : (value.as.boolean ? "true" : "false"), NULL, 0, len_ptr);
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			double number;
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			text = jsval_number_text(number, numbuf);
			return jsval_ascii_copy_utf16(text, NULL, 0, len_ptr);
		}
		text = jsval_number_text(value.as.number, numbuf);
		return jsval_ascii_copy_utf16(text, NULL, 0, len_ptr);
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		errno = ENOTSUP;
		return -1;
	default:
		errno = ENOTSUP;
		return -1;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_value_copy_utf16(jsval_region_t *region, jsval_t value, uint16_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_json_doc_t *doc;
	double number;
	char numbuf[64];
	const char *text;

	switch (value.kind) {
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			return jsval_native_string_copy_utf16(region, value, buf, cap, len_ptr);
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_string_copy_utf16(region, doc, value.as.index, buf, cap, len_ptr);
		}
		break;
	case JSVAL_KIND_UNDEFINED:
		return jsval_ascii_copy_utf16("undefined", buf, cap, len_ptr);
	case JSVAL_KIND_NULL:
		return jsval_ascii_copy_utf16("null", buf, cap, len_ptr);
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			int boolean;
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			return jsval_ascii_copy_utf16(boolean ? "true" : "false", buf, cap, len_ptr);
		}
		return jsval_ascii_copy_utf16(value.as.boolean ? "true" : "false", buf, cap, len_ptr);
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			text = jsval_number_text(number, numbuf);
			return jsval_ascii_copy_utf16(text, buf, cap, len_ptr);
		}
		text = jsval_number_text(value.as.number, numbuf);
		return jsval_ascii_copy_utf16(text, buf, cap, len_ptr);
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		errno = ENOTSUP;
		return -1;
	default:
		errno = ENOTSUP;
		return -1;
	}

	errno = EINVAL;
	return -1;
}

static int jsval_ascii_space(uint8_t ch)
{
	switch (ch) {
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x20:
		return 1;
	default:
		return 0;
	}
}

static int jsval_is_ascii_alpha(uint8_t ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int jsval_exact_ascii(const uint8_t *buf, size_t len, const char *text)
{
	size_t text_len = strlen(text);
	return len == text_len && memcmp(buf, text, text_len) == 0;
}

static int jsval_string_to_number(jsval_region_t *region, jsval_t value,
		double *number_ptr)
{
	size_t len = 0;

	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return -1;
	}

	{
		uint8_t bytes[len ? len : 1];

		if (len > 0 && jsval_string_copy_utf8(region, value, bytes, len, NULL) < 0) {
			return -1;
		}
		return jsnum_parse_string(bytes, len, number_ptr);
	}
}

int jsval_to_number(jsval_region_t *region, jsval_t value, double *number_ptr)
{
	jsval_json_doc_t *doc;
	int boolean;

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*number_ptr = NAN;
		return 0;
	case JSVAL_KIND_NULL:
		*number_ptr = 0;
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			*number_ptr = boolean ? 1 : 0;
			return 0;
		}
		*number_ptr = value.as.boolean ? 1 : 0;
		return 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			return jsval_json_number_value(region, doc, value.as.index, number_ptr);
		}
		*number_ptr = value.as.number;
		return 0;
	case JSVAL_KIND_STRING:
		return jsval_string_to_number(region, value, number_ptr);
	default:
		errno = ENOTSUP;
		return -1;
	}
}

int jsval_to_int32(jsval_region_t *region, jsval_t value, int32_t *result_ptr)
{
	double number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}
	*result_ptr = jsnum_to_int32(number);
	return 0;
}

int jsval_to_uint32(jsval_region_t *region, jsval_t value, uint32_t *result_ptr)
{
	double number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}
	*result_ptr = jsnum_to_uint32(number);
	return 0;
}

static int jsval_method_output_cap(size_t input_len, size_t factor,
		size_t *cap_ptr)
{
	size_t cap;

	if (cap_ptr == NULL || factor == 0) {
		errno = EINVAL;
		return -1;
	}
	if (input_len > SIZE_MAX / factor) {
		errno = EOVERFLOW;
		return -1;
	}

	cap = input_len * factor;
	if (cap == 0) {
		cap = 1;
	}
	*cap_ptr = cap;
	return 0;
}

static int jsval_method_value_from_jsval(jsval_region_t *region, jsval_t value,
		uint16_t *string_storage, size_t string_storage_cap,
		jsmethod_value_t *method_value_ptr)
{
	jsval_json_doc_t *doc;
	jsval_native_string_t *string;
	double number;
	int boolean;
	size_t len;

	if (region == NULL || method_value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*method_value_ptr = jsmethod_value_undefined();
		return 0;
	case JSVAL_KIND_NULL:
		*method_value_ptr = jsmethod_value_null();
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_bool(boolean);
			return 0;
		}
		*method_value_ptr = jsmethod_value_bool(value.as.boolean);
		return 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_number(number);
			return 0;
		}
		*method_value_ptr = jsmethod_value_number(value.as.number);
		return 0;
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			string = jsval_native_string(region, value);
			if (string == NULL) {
				errno = EINVAL;
				return -1;
			}
			*method_value_ptr = jsmethod_value_string_utf16(
					jsval_native_string_units(string), string->len);
			return 0;
		}
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0,
					&len) < 0) {
				return -1;
			}
			if (len == 0) {
				*method_value_ptr = jsmethod_value_string_utf16(NULL, 0);
				return 0;
			}
			if (string_storage == NULL) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf16(region, doc, value.as.index,
					string_storage, string_storage_cap, &len) < 0) {
				return -1;
			}
			*method_value_ptr = jsmethod_value_string_utf16(string_storage, len);
			return 0;
		}
		break;
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		errno = ENOTSUP;
		return -1;
	default:
		break;
	}

	errno = EINVAL;
	return -1;
}

static int
jsval_stringify_value_to_native(jsval_region_t *region, jsval_t value,
		int require_object_coercible, jsval_t *string_value_ptr,
		jsmethod_error_t *error)
{
	jsval_t out;
	jsval_native_string_t *string;
	jsstr16_t str;
	jsmethod_value_t method_value;
	size_t storage_cap;

	if (region == NULL || string_value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, value, &storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, storage_cap, &out, &string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&str, (const char *)jsval_native_string_units(string),
			string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, value, str.codeunits, str.cap,
			&method_value) < 0) {
		return -1;
	}
	if (jsmethod_value_to_string(&str, method_value, require_object_coercible,
			error) < 0) {
		return -1;
	}
	string->len = str.len;
	*string_value_ptr = out;
	return 0;
}

typedef int (*jsval_method_unary_fn)(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);

typedef int (*jsval_method_repeat_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_count,
		jsmethod_value_t count_value, jsmethod_error_t *error);

typedef int (*jsval_method_concat_measure_fn)(jsmethod_value_t this_value,
		size_t arg_count, const jsmethod_value_t *args,
		jsmethod_string_concat_sizes_t *sizes, jsmethod_error_t *error);

typedef int (*jsval_method_concat_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, size_t arg_count,
		const jsmethod_value_t *args, jsmethod_error_t *error);

typedef int (*jsval_method_replace_measure_fn)(jsmethod_value_t this_value,
		jsmethod_value_t search_value, jsmethod_value_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);

typedef int (*jsval_method_replace_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		jsmethod_value_t replacement_value, jsmethod_error_t *error);

typedef int (*jsval_method_pad_measure_fn)(jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes, jsmethod_error_t *error);

typedef int (*jsval_method_pad_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_max_length,
		jsmethod_value_t max_length_value, int have_fill_string,
		jsmethod_value_t fill_string_value, jsmethod_error_t *error);

typedef int (*jsval_method_locale_fn)(jsstr16_t *out, jsmethod_value_t this_value,
		int have_locale, jsmethod_value_t locale_value,
		uint16_t *locale_storage, size_t locale_storage_cap,
		jsmethod_error_t *error);

typedef int (*jsval_method_string_pos_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_optional_fn)(jsstr16_t *out,
		int *has_value_ptr, jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_double_fn)(double *value_ptr,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_optional_double_fn)(int *has_value_ptr,
		double *value_ptr, jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_range_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_start,
		jsmethod_value_t start_value, int have_end,
		jsmethod_value_t end_value, jsmethod_error_t *error);

typedef int (*jsval_method_string_index_fn)(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);

typedef int (*jsval_method_string_bool_fn)(int *result_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);

typedef struct jsval_replace_part_s {
	size_t match_start;
	size_t match_end;
	jsval_t replacement;
} jsval_replace_part_t;

static int jsval_stringify_value_to_native(jsval_region_t *region, jsval_t value,
		int require_object_coercible, jsval_t *string_value_ptr,
		jsmethod_error_t *error);

#if JSMX_WITH_REGEX
typedef int (*jsval_method_string_regex_index_fn)(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t pattern_value,
		int have_flags, jsmethod_value_t flags_value,
		jsmethod_error_t *error);

static int jsval_method_string_replace_all_regex_check(
		jsval_region_t *region, jsval_t regexp_value,
		jsmethod_error_t *error);
static int jsval_method_string_replace_regex_measure_bridge(
		jsval_region_t *region, jsval_t this_value, jsval_t regexp_value,
		jsval_t replacement_value, jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);
static int jsval_method_string_replace_regex(jsval_region_t *region,
		jsval_t this_value, jsval_t regexp_value, jsval_t replacement_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
static int jsval_method_string_replace_regex_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t regexp_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error);
#endif

static int jsval_method_string_unary_bridge(jsval_region_t *region,
		jsval_t this_value, size_t expansion_factor,
		jsval_method_unary_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	jsmethod_value_t method_value;
	size_t input_len;
	size_t output_cap;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &input_len) < 0) {
		return -1;
	}
	if (jsval_method_output_cap(input_len, expansion_factor, &output_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&method_value) < 0) {
		return -1;
	}
	if (fn(&out, method_value, error) < 0) {
		return -1;
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_repeat_bridge(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsval_method_repeat_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsmethod_string_repeat_sizes_t sizes;
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t count_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t count_method_value = jsmethod_value_undefined();

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_repeat_measure(region, this_value, have_count,
			count_value, &sizes, error) < 0) {
		return -1;
	}
	if (have_count &&
			jsval_value_utf16_len(region, count_value, &count_storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&this_method_value) < 0) {
		return -1;
	}

	{
		uint16_t count_storage[count_storage_cap ? count_storage_cap : 1];

		if (have_count &&
				jsval_method_value_from_jsval(region, count_value,
				count_storage, count_storage_cap,
				&count_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_count, count_method_value,
				error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_string_measure_bridge(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsval_method_replace_measure_fn measure_fn,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error)
{
	size_t this_storage_cap = 0;
	size_t search_storage_cap = 0;
	size_t replacement_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t replacement_method_value;

	if (region == NULL || measure_fn == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, replacement_value,
			&replacement_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t replacement_storage[replacement_storage_cap ?
				replacement_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, replacement_value,
				replacement_storage, replacement_storage_cap,
				&replacement_method_value) < 0) {
			return -1;
		}
		return measure_fn(this_method_value, search_method_value,
				replacement_method_value, sizes, error);
	}
}

static int
jsval_method_string_replace_string_bridge(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsval_method_replace_measure_fn measure_fn,
		jsval_method_replace_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsmethod_string_replace_sizes_t sizes;
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t this_storage_cap = 0;
	size_t search_storage_cap = 0;
	size_t replacement_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t replacement_method_value;

	if (region == NULL || measure_fn == NULL || fn == NULL
			|| value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_replace_string_measure_bridge(region, this_value,
			search_value, replacement_value, measure_fn, &sizes, error) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, replacement_value,
			&replacement_storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t replacement_storage[replacement_storage_cap ?
				replacement_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, replacement_value,
				replacement_storage, replacement_storage_cap,
				&replacement_method_value) < 0) {
			return -1;
		}
		jsstr16_init_from_buf(&out,
				(const char *)jsval_native_string_units(result_string),
				result_string->cap * sizeof(uint16_t));
		if (fn(&out, this_method_value, search_method_value,
				replacement_method_value, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_append_segment(jsstr16_t *out, size_t *offset_ptr,
		const uint16_t *segment, size_t segment_len, int build)
{
	if (offset_ptr == NULL || (segment_len > 0 && segment == NULL)
			|| (build && out == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (SIZE_MAX - *offset_ptr < segment_len) {
		errno = EOVERFLOW;
		return -1;
	}
	if (build) {
		if (*offset_ptr > out->cap || out->cap - *offset_ptr < segment_len) {
			errno = ENOBUFS;
			return -1;
		}
		if (segment_len > 0) {
			memcpy(out->codeunits + *offset_ptr, segment,
					segment_len * sizeof(out->codeunits[0]));
		}
		out->len = *offset_ptr + segment_len;
	}
	*offset_ptr += segment_len;
	return 0;
}

static int
jsval_string_replace_find_match_from(const uint16_t *subject,
		size_t subject_len, const uint16_t *search, size_t search_len,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	size_t i;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| (subject_len > 0 && subject == NULL)
			|| (search_len > 0 && search == NULL)
			|| from_index > subject_len) {
		errno = EINVAL;
		return -1;
	}
	if (search_len == 0) {
		*matched_ptr = 1;
		*start_ptr = from_index;
		*end_ptr = from_index;
		return 0;
	}
	if (search_len > subject_len || from_index > subject_len - search_len) {
		*matched_ptr = 0;
		*start_ptr = 0;
		*end_ptr = 0;
		return 0;
	}
	for (i = from_index; i + search_len <= subject_len; i++) {
		if (memcmp(subject + i, search,
				search_len * sizeof(subject[0])) == 0) {
			*matched_ptr = 1;
			*start_ptr = i;
			*end_ptr = i + search_len;
			return 0;
		}
	}
	*matched_ptr = 0;
	*start_ptr = 0;
	*end_ptr = 0;
	return 0;
}

static int
jsval_replace_callback_stringify(jsval_region_t *region,
		jsval_replace_callback_fn callback, void *callback_ctx,
		const jsval_replace_call_t *call, jsval_t *replacement_ptr,
		jsmethod_error_t *error)
{
	jsval_t callback_result;

	if (region == NULL || callback == NULL || call == NULL
			|| replacement_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (callback(region, callback_ctx, call, &callback_result, error) < 0) {
		return -1;
	}
	return jsval_stringify_value_to_native(region, callback_result, 0,
			replacement_ptr, error);
}

static int
jsval_string_replace_count_parts(const uint16_t *subject, size_t subject_len,
		const uint16_t *search, size_t search_len, int replace_all,
		size_t *count_ptr)
{
	size_t count = 0;

	if (count_ptr == NULL || (subject_len > 0 && subject == NULL)
			|| (search_len > 0 && search == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_string_replace_find_match_from(subject, subject_len, search,
				search_len, 0, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}
	if (search_len == 0) {
		if (subject_len == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		*count_ptr = subject_len + 1;
		return 0;
	}

	{
		size_t cursor = 0;

		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;

			if (jsval_string_replace_find_match_from(subject, subject_len, search,
					search_len, cursor, &matched, &match_start,
					&match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (count == SIZE_MAX) {
				errno = EOVERFLOW;
				return -1;
			}
			count++;
			cursor = match_end;
		}
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_string_replace_fill_parts(jsval_region_t *region, jsval_t input_value,
		const uint16_t *subject, size_t subject_len, const uint16_t *search,
		size_t search_len, int replace_all, jsval_replace_callback_fn callback,
		void *callback_ctx, jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)
			|| (search_len > 0 && search == NULL)) {
		errno = EINVAL;
		return -1;
	}

	if (search_len == 0) {
		size_t limit = replace_all ? subject_len + 1 : 1;
		size_t pos;

		for (pos = 0; pos < limit; pos++) {
			jsval_replace_call_t call;
			jsval_t match_value;
			jsval_t replacement_string;
			jsval_native_string_t *replacement_native;

			call.capture_count = 0;
			call.captures = NULL;
			call.offset = pos;
			call.input = input_value;
			call.groups = jsval_undefined();
			if (jsval_string_new_utf16(region, NULL, 0, &match_value) < 0) {
				return -1;
			}
			call.match = match_value;
			if (jsval_replace_callback_stringify(region, callback, callback_ctx,
					&call, &replacement_string, error) < 0) {
				return -1;
			}
			replacement_native = jsval_native_string(region, replacement_string);
			if (replacement_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			parts[write_index].match_start = pos;
			parts[write_index].match_end = pos;
			parts[write_index].replacement = replacement_string;
			write_index++;
			if (jsval_append_segment(NULL, &total_len,
					replacement_native->len > 0
						? jsval_native_string_units(replacement_native) : NULL,
					replacement_native->len,
					0) < 0) {
				return -1;
			}
			if (pos < subject_len && jsval_append_segment(NULL, &total_len,
					subject + pos,
					1, 0) < 0) {
				return -1;
			}
		}
		if (write_index != part_count) {
			errno = EINVAL;
			return -1;
		}
		*total_len_ptr = total_len;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_string_replace_find_match_from(subject, subject_len, search,
				search_len, cursor, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len,
				0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_replace_build_parts(jsval_region_t *region, const uint16_t *subject,
		size_t subject_len, const jsval_replace_part_t *parts,
		size_t part_count, size_t total_len, jsval_t *value_ptr)
{
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t cursor = 0;
	size_t offset = 0;
	size_t i;

	if (region == NULL || value_ptr == NULL
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_reserve_utf16(region, total_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	for (i = 0; i < part_count; i++) {
		jsval_native_string_t *replacement_native;

		if (parts[i].match_start > parts[i].match_end
				|| parts[i].match_start < cursor
				|| parts[i].match_end > subject_len) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(&out, &offset,
				parts[i].match_start > cursor ? subject + cursor : NULL,
				parts[i].match_start - cursor, 1) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, parts[i].replacement);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(&out, &offset,
				jsval_native_string_units(replacement_native),
				replacement_native->len, 1) < 0) {
			return -1;
		}
		cursor = parts[i].match_end;
	}
	if (jsval_append_segment(&out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 1) < 0) {
		return -1;
	}
	result_string->len = total_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_string_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t search_string;
	jsval_native_string_t *input_native;
	jsval_native_string_t *search_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, search_value, 0, &search_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	search_native = jsval_native_string(region, search_string);
	if (input_native == NULL || search_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_replace_count_parts(jsval_native_string_units(input_native),
			input_native->len, jsval_native_string_units(search_native),
			search_native->len, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_string_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			jsval_native_string_units(search_native), search_native->len,
			replace_all, callback, callback_ctx, parts, part_count,
			&total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

static int
jsval_method_string_concat_bridge(jsval_region_t *region,
		jsval_t this_value, size_t arg_count, const jsval_t *args,
		jsval_method_concat_measure_fn measure_fn, jsval_method_concat_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_string_concat_sizes_t sizes;
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t this_storage_cap = 0;
	size_t arg_storage_total = 0;
	size_t i;

	if (region == NULL || measure_fn == NULL || fn == NULL || value_ptr == NULL ||
			(arg_count > 0 && args == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	{
		size_t arg_storage_caps[arg_count ? arg_count : 1];
		jsmethod_value_t this_method_value;
		jsmethod_value_t arg_method_values[arg_count ? arg_count : 1];

		for (i = 0; i < arg_count; i++) {
			if (jsval_value_utf16_len(region, args[i], &arg_storage_caps[i]) < 0) {
				return -1;
			}
			if (SIZE_MAX - arg_storage_total < arg_storage_caps[i]) {
				errno = EOVERFLOW;
				return -1;
			}
			arg_storage_total += arg_storage_caps[i];
		}

		{
			uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
			uint16_t arg_storage[arg_storage_total ? arg_storage_total : 1];
			uint16_t *cursor = arg_storage;

			if (jsval_method_value_from_jsval(region, this_value, this_storage,
					this_storage_cap, &this_method_value) < 0) {
				return -1;
			}
			for (i = 0; i < arg_count; i++) {
				if (jsval_method_value_from_jsval(region, args[i], cursor,
						arg_storage_caps[i], &arg_method_values[i]) < 0) {
					return -1;
				}
				cursor += arg_storage_caps[i];
			}
			if (measure_fn(this_method_value, arg_count, arg_method_values, &sizes,
					error) < 0) {
				return -1;
			}
		}
	}

	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	{
		size_t arg_storage_caps[arg_count ? arg_count : 1];
		jsmethod_value_t this_method_value;
		jsmethod_value_t arg_method_values[arg_count ? arg_count : 1];
		uint16_t *cursor;

		arg_storage_total = 0;
		for (i = 0; i < arg_count; i++) {
			if (jsval_value_utf16_len(region, args[i], &arg_storage_caps[i]) < 0) {
				return -1;
			}
			if (SIZE_MAX - arg_storage_total < arg_storage_caps[i]) {
				errno = EOVERFLOW;
				return -1;
			}
			arg_storage_total += arg_storage_caps[i];
		}

		{
			uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
			uint16_t arg_storage[arg_storage_total ? arg_storage_total : 1];

			cursor = arg_storage;
			if (jsval_method_value_from_jsval(region, this_value, this_storage,
					this_storage_cap, &this_method_value) < 0) {
				return -1;
			}
			for (i = 0; i < arg_count; i++) {
				if (jsval_method_value_from_jsval(region, args[i], cursor,
						arg_storage_caps[i], &arg_method_values[i]) < 0) {
					return -1;
				}
				cursor += arg_storage_caps[i];
			}
			jsstr16_init_from_buf(&out,
					(const char *)jsval_native_string_units(result_string),
					result_string->cap * sizeof(uint16_t));
			if (fn(&out, this_method_value, arg_count, arg_method_values, error)
					< 0) {
				return -1;
			}
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_pad_bridge(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsval_method_pad_measure_fn measure_fn, jsval_method_pad_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_string_pad_sizes_t sizes;
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t max_length_storage_cap = 0;
	size_t fill_string_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t max_length_method_value = jsmethod_value_undefined();
	jsmethod_value_t fill_string_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;

	if (region == NULL || measure_fn == NULL || fn == NULL ||
			value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_max_length &&
			jsval_value_utf16_len(region, max_length_value,
			&max_length_storage_cap) < 0) {
		return -1;
	}
	if (have_fill_string &&
			fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, fill_string_value,
			&fill_string_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t max_length_storage[max_length_storage_cap ?
				max_length_storage_cap : 1];
		uint16_t fill_string_storage[fill_string_storage_cap ?
				fill_string_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_max_length &&
				jsval_method_value_from_jsval(region, max_length_value,
				max_length_storage, max_length_storage_cap,
				&max_length_method_value) < 0) {
			return -1;
		}
		if (have_fill_string &&
				fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, fill_string_value,
				fill_string_storage, fill_string_storage_cap,
				&fill_string_method_value) < 0) {
			return -1;
		}
		if (measure_fn(this_method_value, have_max_length,
				max_length_method_value, have_fill_string,
				fill_string_method_value, &sizes, error) < 0) {
			return -1;
		}
	}

	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t max_length_storage[max_length_storage_cap ?
				max_length_storage_cap : 1];
		uint16_t fill_string_storage[fill_string_storage_cap ?
				fill_string_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_max_length &&
				jsval_method_value_from_jsval(region, max_length_value,
				max_length_storage, max_length_storage_cap,
				&max_length_method_value) < 0) {
			return -1;
		}
		if (have_fill_string &&
				fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, fill_string_value,
				fill_string_storage, fill_string_storage_cap,
				&fill_string_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_max_length,
				max_length_method_value, have_fill_string,
				fill_string_method_value, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int jsval_method_string_locale_bridge(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		size_t expansion_factor, jsval_method_locale_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	jsmethod_value_t this_method_value;
	jsmethod_value_t locale_method_value = jsmethod_value_undefined();
	size_t input_len;
	size_t output_cap;
	size_t locale_storage_cap = 1;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &input_len) < 0) {
		return -1;
	}
	if (jsval_method_output_cap(input_len, expansion_factor, &output_cap) < 0) {
		return -1;
	}
	if (have_locale && locale_value.kind != JSVAL_KIND_UNDEFINED) {
		if (jsval_value_utf16_len(region, locale_value, &locale_storage_cap) < 0) {
			return -1;
		}
		if (locale_storage_cap == 0) {
			locale_storage_cap = 1;
		}
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, out.codeunits, out.cap,
			&this_method_value) < 0) {
		return -1;
	}

	{
		uint16_t locale_storage[locale_storage_cap ? locale_storage_cap : 1];

		if (have_locale &&
		    jsval_method_value_from_jsval(region, locale_value, locale_storage,
			    locale_storage_cap, &locale_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_locale, locale_method_value,
				locale_storage, locale_storage_cap, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_position_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_pos_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	uint16_t out_storage[1];
	jsstr16_t out;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		jsstr16_init_from_buf(&out, (const char *)out_storage,
				sizeof(out_storage));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_position, position_method_value,
				error) < 0) {
			return -1;
		}
	}

	return jsval_string_new_utf16(region, out.codeunits, out.len, value_ptr);
}

static int
jsval_method_string_optional_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_optional_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	uint16_t out_storage[1];
	jsstr16_t out;
	int has_value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		jsstr16_init_from_buf(&out, (const char *)out_storage,
				sizeof(out_storage));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&out, &has_value, this_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	if (!has_value) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	return jsval_string_new_utf16(region, out.codeunits, out.len, value_ptr);
}

static int
jsval_method_string_double_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_double_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	double value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&value, this_method_value, have_position, position_method_value,
				error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_number(value);
	return 0;
}

static int
jsval_method_string_optional_double_bridge(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_method_string_optional_double_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	int has_value;
	double value;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&has_value, &value, this_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	if (!has_value) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	*value_ptr = jsval_number(value);
	return 0;
}

static int
jsval_method_string_range_bridge(jsval_region_t *region,
		jsval_t this_value, int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_method_string_range_fn fn,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;
	size_t this_storage_cap;
	size_t start_storage_cap = 0;
	size_t end_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t start_method_value = jsmethod_value_undefined();
	jsmethod_value_t end_method_value = jsmethod_value_undefined();
	size_t output_cap;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	output_cap = this_storage_cap;
	if (have_start &&
			jsval_value_utf16_len(region, start_value, &start_storage_cap) < 0) {
		return -1;
	}
	if (have_end &&
			jsval_value_utf16_len(region, end_value, &end_storage_cap) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, output_cap, &result,
			&result_string) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t start_storage[start_storage_cap ? start_storage_cap : 1];
		uint16_t end_storage[end_storage_cap ? end_storage_cap : 1];

		jsstr16_init_from_buf(&out,
				(const char *)jsval_native_string_units(result_string),
				result_string->cap * sizeof(uint16_t));
		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_start &&
				jsval_method_value_from_jsval(region, start_value, start_storage,
				start_storage_cap, &start_method_value) < 0) {
			return -1;
		}
		if (have_end &&
				jsval_method_value_from_jsval(region, end_value, end_storage,
				end_storage_cap, &end_method_value) < 0) {
			return -1;
		}
		if (fn(&out, this_method_value, have_start, start_method_value,
				have_end, end_method_value, error) < 0) {
			return -1;
		}
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_index_bridge(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_method_string_index_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t search_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	ssize_t index;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&index, this_method_value, search_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_number((double)index);
	return 0;
}

static int
jsval_method_string_bool_bridge(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_method_string_bool_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t search_storage_cap;
	size_t position_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t search_method_value;
	jsmethod_value_t position_method_value = jsmethod_value_undefined();
	int result;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, search_value, &search_storage_cap) < 0) {
		return -1;
	}
	if (have_position &&
			jsval_value_utf16_len(region, position_value,
			&position_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t search_storage[search_storage_cap ? search_storage_cap : 1];
		uint16_t position_storage[position_storage_cap ? position_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, search_value, search_storage,
				search_storage_cap, &search_method_value) < 0) {
			return -1;
		}
		if (have_position &&
				jsval_method_value_from_jsval(region, position_value,
				position_storage, position_storage_cap,
				&position_method_value) < 0) {
			return -1;
		}
		if (fn(&result, this_method_value, search_method_value, have_position,
				position_method_value, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_bool(result);
	return 0;
}

typedef struct jsval_split_array_emit_ctx_s {
	jsval_region_t *region;
	jsval_t array;
	size_t write_index;
} jsval_split_array_emit_ctx_t;

static int
jsval_split_limit(jsval_region_t *region, int have_limit, jsval_t limit_value,
		size_t *limit_ptr)
{
	uint32_t limit32;

	if (limit_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!have_limit || limit_value.kind == JSVAL_KIND_UNDEFINED) {
		*limit_ptr = (size_t)UINT32_MAX;
		return 0;
	}
	if (jsval_to_uint32(region, limit_value, &limit32) < 0) {
		return -1;
	}
	*limit_ptr = (size_t)limit32;
	return 0;
}

static int
jsval_split_emit_array(void *opaque, const uint16_t *segment,
		size_t segment_len)
{
	static const uint16_t empty_unit = 0;
	jsval_split_array_emit_ctx_t *ctx =
		(jsval_split_array_emit_ctx_t *)opaque;
	jsval_t value;

	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_new_utf16(ctx->region,
			segment_len > 0 ? segment : &empty_unit,
			segment_len, &value) < 0) {
		return -1;
	}
	if (jsval_array_set(ctx->region, ctx->array, ctx->write_index++, value) < 0) {
		return -1;
	}
	return 0;
}

static int
jsval_method_string_split_string(jsval_region_t *region, jsval_t this_value,
		int have_separator, jsval_t separator_value,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap = 0;
	size_t separator_storage_cap = 0;
	size_t limit_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t separator_method_value = jsmethod_value_undefined();
	jsmethod_value_t limit_method_value = jsmethod_value_undefined();
	size_t part_count = 0;
	jsval_t array;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_separator && separator_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, separator_value,
				&separator_storage_cap) < 0) {
		return -1;
	}
	if (have_limit && limit_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, limit_value,
				&limit_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t separator_storage[separator_storage_cap ? separator_storage_cap : 1];
		uint16_t limit_storage[limit_storage_cap ? limit_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_separator && separator_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, separator_value,
				separator_storage, separator_storage_cap,
				&separator_method_value) < 0) {
			return -1;
		}
		if (have_limit && limit_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, limit_value,
				limit_storage, limit_storage_cap,
				&limit_method_value) < 0) {
			return -1;
		}
		if (jsmethod_string_split(this_method_value, have_separator,
				separator_method_value, have_limit, limit_method_value,
				NULL, NULL, &part_count, error) < 0) {
			return -1;
		}
		if (jsval_array_new(region, part_count, &array) < 0) {
			return -1;
		}
		{
			jsval_split_array_emit_ctx_t emit_ctx = {region, array, 0};

			if (jsmethod_string_split(this_method_value, have_separator,
					separator_method_value, have_limit, limit_method_value,
					&emit_ctx, jsval_split_emit_array, &part_count,
					error) < 0) {
				return -1;
			}
		}
	}

	*value_ptr = array;
	return 0;
}

#if JSMX_WITH_REGEX
static int
jsval_method_string_regex_index_bridge(jsval_region_t *region,
		jsval_t this_value, jsval_t pattern_value,
		int have_flags, jsval_t flags_value,
		jsval_method_string_regex_index_fn fn, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	size_t this_storage_cap;
	size_t pattern_storage_cap;
	size_t flags_storage_cap = 0;
	jsmethod_value_t this_method_value;
	jsmethod_value_t pattern_method_value;
	jsmethod_value_t flags_method_value = jsmethod_value_undefined();
	ssize_t index;

	if (region == NULL || fn == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (jsval_value_utf16_len(region, pattern_value, &pattern_storage_cap) < 0) {
		return -1;
	}
	if (have_flags &&
			jsval_value_utf16_len(region, flags_value, &flags_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t pattern_storage[pattern_storage_cap ? pattern_storage_cap : 1];
		uint16_t flags_storage[flags_storage_cap ? flags_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (jsval_method_value_from_jsval(region, pattern_value, pattern_storage,
				pattern_storage_cap, &pattern_method_value) < 0) {
			return -1;
		}
		if (have_flags &&
				jsval_method_value_from_jsval(region, flags_value, flags_storage,
				flags_storage_cap, &flags_method_value) < 0) {
			return -1;
		}
		if (fn(&index, this_method_value, pattern_method_value, have_flags,
				flags_method_value, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_number((double)index);
	return 0;
}
#endif

int jsval_method_string_normalize_measure(jsval_region_t *region,
		jsval_t this_value, int have_form, jsval_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t form_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t form_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, form_value, &form_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t form_storage[form_storage_cap ? form_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, form_value, form_storage,
					form_storage_cap, &form_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_normalize_measure(this_method_value, have_form,
				form_method_value, sizes, error);
	}
}

int jsval_method_string_repeat_measure(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsmethod_string_repeat_sizes_t *sizes,
		jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t count_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t count_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_count &&
			jsval_value_utf16_len(region, count_value, &count_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t count_storage[count_storage_cap ? count_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_count &&
				jsval_method_value_from_jsval(region, count_value,
				count_storage, count_storage_cap,
				&count_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_repeat_measure(this_method_value, have_count,
				count_method_value, sizes, error);
	}
}

int
jsval_method_string_concat_measure(jsval_region_t *region, jsval_t this_value,
		size_t arg_count, const jsval_t *args,
		jsmethod_string_concat_sizes_t *sizes,
		jsmethod_error_t *error)
{
	size_t this_storage_cap = 0;
	size_t arg_storage_total = 0;
	size_t i;

	if (region == NULL || sizes == NULL ||
			(arg_count > 0 && args == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}

	{
		size_t arg_storage_caps[arg_count ? arg_count : 1];
		jsmethod_value_t this_method_value;
		jsmethod_value_t arg_method_values[arg_count ? arg_count : 1];

		for (i = 0; i < arg_count; i++) {
			if (jsval_value_utf16_len(region, args[i], &arg_storage_caps[i]) < 0) {
				return -1;
			}
			if (SIZE_MAX - arg_storage_total < arg_storage_caps[i]) {
				errno = EOVERFLOW;
				return -1;
			}
			arg_storage_total += arg_storage_caps[i];
		}

		{
			uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
			uint16_t arg_storage[arg_storage_total ? arg_storage_total : 1];
			uint16_t *cursor = arg_storage;

			if (jsval_method_value_from_jsval(region, this_value, this_storage,
					this_storage_cap, &this_method_value) < 0) {
				return -1;
			}
			for (i = 0; i < arg_count; i++) {
				if (jsval_method_value_from_jsval(region, args[i], cursor,
						arg_storage_caps[i], &arg_method_values[i]) < 0) {
					return -1;
				}
				cursor += arg_storage_caps[i];
			}
			return jsmethod_string_concat_measure(this_method_value, arg_count,
					arg_method_values, sizes, error);
		}
	}
}

int
jsval_method_string_replace_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error)
{
	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		return jsval_method_string_replace_regex_measure_bridge(region,
				this_value, search_value, replacement_value, sizes, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_measure_bridge(region,
			this_value, search_value, replacement_value,
			jsmethod_string_replace_measure, sizes, error);
}

int
jsval_method_string_replace_all_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error)
{
	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		if (jsval_method_string_replace_all_regex_check(region, search_value,
				error) < 0) {
			return -1;
		}
		return jsval_method_string_replace_regex_measure_bridge(region,
				this_value, search_value, replacement_value, sizes, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_measure_bridge(region,
			this_value, search_value, replacement_value,
			jsmethod_string_replace_all_measure, sizes, error);
}

int
jsval_method_string_pad_start_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes, jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t max_length_method_value = jsmethod_value_undefined();
	jsmethod_value_t fill_string_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t max_length_storage_cap = 0;
	size_t fill_string_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_max_length &&
			jsval_value_utf16_len(region, max_length_value,
			&max_length_storage_cap) < 0) {
		return -1;
	}
	if (have_fill_string && fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, fill_string_value,
			&fill_string_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t max_length_storage[max_length_storage_cap ?
				max_length_storage_cap : 1];
		uint16_t fill_string_storage[fill_string_storage_cap ?
				fill_string_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_max_length &&
				jsval_method_value_from_jsval(region, max_length_value,
				max_length_storage, max_length_storage_cap,
				&max_length_method_value) < 0) {
			return -1;
		}
		if (have_fill_string &&
				fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, fill_string_value,
				fill_string_storage, fill_string_storage_cap,
				&fill_string_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_pad_start_measure(this_method_value,
				have_max_length, max_length_method_value,
				have_fill_string, fill_string_method_value, sizes, error);
	}
}

int
jsval_method_string_pad_end_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes, jsmethod_error_t *error)
{
	jsmethod_value_t this_method_value;
	jsmethod_value_t max_length_method_value = jsmethod_value_undefined();
	jsmethod_value_t fill_string_method_value = jsmethod_value_undefined();
	size_t this_storage_cap = 0;
	size_t max_length_storage_cap = 0;
	size_t fill_string_storage_cap = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &this_storage_cap) < 0) {
		return -1;
	}
	if (have_max_length &&
			jsval_value_utf16_len(region, max_length_value,
			&max_length_storage_cap) < 0) {
		return -1;
	}
	if (have_fill_string && fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_value_utf16_len(region, fill_string_value,
			&fill_string_storage_cap) < 0) {
		return -1;
	}

	{
		uint16_t this_storage[this_storage_cap ? this_storage_cap : 1];
		uint16_t max_length_storage[max_length_storage_cap ?
				max_length_storage_cap : 1];
		uint16_t fill_string_storage[fill_string_storage_cap ?
				fill_string_storage_cap : 1];

		if (jsval_method_value_from_jsval(region, this_value, this_storage,
				this_storage_cap, &this_method_value) < 0) {
			return -1;
		}
		if (have_max_length &&
				jsval_method_value_from_jsval(region, max_length_value,
				max_length_storage, max_length_storage_cap,
				&max_length_method_value) < 0) {
			return -1;
		}
		if (have_fill_string &&
				fill_string_value.kind != JSVAL_KIND_UNDEFINED &&
				jsval_method_value_from_jsval(region, fill_string_value,
				fill_string_storage, fill_string_storage_cap,
				&fill_string_method_value) < 0) {
			return -1;
		}
		return jsmethod_string_pad_end_measure(this_method_value,
				have_max_length, max_length_method_value,
				have_fill_string, fill_string_method_value, sizes, error);
	}
}

int jsval_method_string_normalize(jsval_region_t *region, jsval_t this_value,
		int have_form, jsval_t form_value,
		uint16_t *this_storage, size_t this_storage_cap,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_string_normalize_sizes_t sizes;
	jsmethod_value_t this_method_value;
	jsmethod_value_t form_method_value = jsmethod_value_undefined();
	jsval_native_string_t *result_string;
	jsval_t result;
	jsstr16_t out;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_normalize_measure(region, this_value, have_form,
			form_value, &sizes, error) < 0) {
		return -1;
	}
	if (this_storage_cap < sizes.this_storage_len ||
			form_storage_cap < sizes.form_storage_len ||
			workspace_cap < sizes.workspace_len) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}

	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_method_value_from_jsval(region, this_value, this_storage,
			this_storage_cap, &this_method_value) < 0) {
		return -1;
	}
	if (have_form && form_value.kind != JSVAL_KIND_UNDEFINED &&
			jsval_method_value_from_jsval(region, form_value, form_storage,
				form_storage_cap, &form_method_value) < 0) {
		return -1;
	}
	if (jsmethod_string_normalize_into(&out, this_method_value,
			this_storage, this_storage_cap,
			have_form, form_method_value,
			form_storage, form_storage_cap,
			workspace, workspace_cap, error) < 0) {
		return -1;
	}

	result_string->len = out.len;
	*value_ptr = result;
	return 0;
}

void jsval_region_init(jsval_region_t *region, void *buf, size_t len)
{
	size_t head_size = jsval_pages_head_size_aligned();

	region->base = (uint8_t *)buf;
	region->len = len;
	region->used = 0;
	region->pages = NULL;

	if (buf == NULL || len < head_size || len > UINT32_MAX) {
		return;
	}

	region->pages = (jsval_pages_t *)buf;
	region->pages->magic = JSVAL_PAGES_MAGIC;
	region->pages->version = JSVAL_PAGES_VERSION;
	region->pages->header_size = sizeof(jsval_pages_t);
	region->pages->total_len = (uint32_t)len;
	region->pages->used_len = (uint32_t)head_size;
	region->pages->root = jsval_undefined();
	region->used = head_size;
}

void jsval_region_rebase(jsval_region_t *region, void *buf, size_t len)
{
	jsval_pages_t *pages;

	region->base = (uint8_t *)buf;
	region->len = len;
	region->used = 0;
	region->pages = NULL;

	if (buf == NULL || len < sizeof(jsval_pages_t)) {
		return;
	}

	pages = (jsval_pages_t *)buf;
	if (pages->magic != JSVAL_PAGES_MAGIC ||
	    pages->version != JSVAL_PAGES_VERSION ||
	    pages->header_size != sizeof(jsval_pages_t) ||
	    pages->used_len < jsval_pages_head_size_aligned() ||
	    pages->used_len > pages->total_len ||
	    len < pages->total_len) {
		return;
	}

	region->pages = pages;
	region->len = pages->total_len;
	region->used = pages->used_len;
}

size_t jsval_region_remaining(jsval_region_t *region)
{
	if (!jsval_region_valid(region)) {
		return 0;
	}
	if (region->used >= region->len) {
		return 0;
	}
	return region->len - region->used;
}

int jsval_region_alloc(jsval_region_t *region, size_t len, size_t align,
		void **ptr_ptr)
{
	return jsval_region_reserve(region, len, align, NULL, ptr_ptr);
}

int jsval_region_measure_alloc(const jsval_region_t *region, size_t *used_ptr,
		size_t len, size_t align)
{
	return jsval_region_measure_reserve(region, used_ptr, len, align);
}

size_t jsval_pages_head_size(void)
{
	return jsval_pages_head_size_aligned();
}

const jsval_pages_t *jsval_region_pages(const jsval_region_t *region)
{
	if (!jsval_region_valid(region)) {
		return NULL;
	}
	return region->pages;
}

int jsval_region_root(jsval_region_t *region, jsval_t *value_ptr)
{
	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	*value_ptr = region->pages->root;
	return 0;
}

int jsval_region_set_root(jsval_region_t *region, jsval_t value)
{
	if (!jsval_region_valid(region)) {
		errno = EINVAL;
		return -1;
	}

	region->pages->root = value;
	return 0;
}

int jsval_is_native(jsval_t value)
{
	return value.repr == JSVAL_REPR_NATIVE;
}

int jsval_is_json_backed(jsval_t value)
{
	return value.repr == JSVAL_REPR_JSON;
}

jsval_t jsval_undefined(void)
{
	jsval_t value;
	memset(&value, 0, sizeof(value));
	value.kind = JSVAL_KIND_UNDEFINED;
	value.repr = JSVAL_REPR_INLINE;
	return value;
}

jsval_t jsval_null(void)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_NULL;
	return value;
}

jsval_t jsval_bool(int boolean)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_BOOL;
	value.as.boolean = boolean ? 1 : 0;
	return value;
}

jsval_t jsval_number(double number)
{
	jsval_t value = jsval_undefined();
	value.kind = JSVAL_KIND_NUMBER;
	value.as.number = number;
	return value;
}

int jsval_string_new_utf8(jsval_region_t *region, const uint8_t *str, size_t len, jsval_t *value_ptr)
{
	size_t utf16_len;
	jsval_native_string_t *string;
	jsval_off_t off;
	size_t bytes_len;
	const uint8_t *read;
	const uint8_t *read_stop;
	uint16_t *write;
	uint16_t *write_stop;

	utf16_len = jsval_utf8_utf16len(str, len);
	bytes_len = sizeof(*string) + utf16_len * sizeof(uint16_t);
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&string) < 0) {
		return -1;
	}

	string->len = utf16_len;
	string->cap = utf16_len;
	read = str;
	read_stop = str + len;
	write = jsval_native_string_units(string);
	write_stop = write + utf16_len;
	while (read < read_stop) {
		uint32_t codepoint = 0;
		int seq_len = 0;
		const uint32_t *codepoint_ptr = &codepoint;

		UTF8_CHAR(read, read_stop, &codepoint, &seq_len);
		if (seq_len == 0) {
			break;
		}
		if (seq_len < 0) {
			seq_len = -seq_len;
			codepoint = 0xFFFD;
		}

		UTF16_ENCODE(&codepoint_ptr, codepoint_ptr + 1, &write, write_stop);
		read += seq_len;
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_STRING;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_string_new_utf16(jsval_region_t *region, const uint16_t *str, size_t len,
		jsval_t *value_ptr)
{
	jsval_native_string_t *string;
	jsval_t value;

	if (region == NULL || value_ptr == NULL || (len > 0 && str == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_reserve_utf16(region, len, &value, &string) < 0) {
		return -1;
	}
	if (len > 0) {
		memcpy(jsval_native_string_units(string), str, len * sizeof(uint16_t));
	}
	string->len = len;
	*value_ptr = value;
	return 0;
}

int jsval_object_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_object_t *object;
	jsval_off_t off;
	size_t bytes_len = sizeof(*object) + cap * sizeof(jsval_native_prop_t);
	size_t i;

	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&object) < 0) {
		return -1;
	}

	object->len = 0;
	object->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_prop_t *prop = &jsval_native_object_props(object)[i];
		prop->name_off = 0;
		prop->value = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_OBJECT;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_array_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_array_t *array;
	jsval_off_t off;
	size_t bytes_len = sizeof(*array) + cap * sizeof(jsval_t);
	size_t i;

	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off, (void **)&array) < 0) {
		return -1;
	}

	array->len = 0;
	array->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_array_values(array)[i] = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_ARRAY;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

static int jsval_same_value_zero(jsval_region_t *region, jsval_t left,
		jsval_t right)
{
	double left_number;
	double right_number;

	if (left.kind != right.kind) {
		return 0;
	}
	if (left.kind == JSVAL_KIND_NUMBER) {
		if (jsval_to_number(region, left, &left_number) < 0
				|| jsval_to_number(region, right, &right_number) < 0) {
			return 0;
		}
		if (left_number != left_number && right_number != right_number) {
			return 1;
		}
		return left_number == right_number;
	}
	return jsval_strict_eq(region, left, right);
}

int jsval_set_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_set_t *set;
	jsval_off_t off;
	size_t bytes_len = sizeof(*set) + cap * sizeof(jsval_t);
	size_t i;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off,
			(void **)&set) < 0) {
		return -1;
	}

	set->len = 0;
	set->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_set_values(set)[i] = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_SET;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_set_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr)
{
	jsval_native_set_t *native;
	jsval_t out;
	jsval_t *src_values;
	jsval_t *dst_values;
	size_t i;

	if (region == NULL || value_ptr == NULL || src.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (src.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, src);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (capacity < native->len) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsval_set_new(region, capacity, &out) < 0) {
		return -1;
	}

	src_values = jsval_native_set_values(native);
	dst_values = jsval_native_set_values(jsval_native_set(region, out));
	for (i = 0; i < native->len; i++) {
		dst_values[i] = src_values[i];
	}
	jsval_native_set(region, out)->len = native->len;
	*value_ptr = out;
	return 0;
}

int jsval_set_size(jsval_region_t *region, jsval_t set, size_t *size_ptr)
{
	jsval_native_set_t *native;

	if (size_ptr == NULL || set.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (set.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, set);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	*size_ptr = native->len;
	return 0;
}

int jsval_set_has(jsval_region_t *region, jsval_t set, jsval_t key,
		int *has_ptr)
{
	jsval_native_set_t *native;
	jsval_t *values;
	size_t i;

	if (has_ptr == NULL || set.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (set.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, set);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	values = jsval_native_set_values(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, values[i], key)) {
			*has_ptr = 1;
			return 0;
		}
	}
	*has_ptr = 0;
	return 0;
}

int jsval_set_add(jsval_region_t *region, jsval_t set, jsval_t key)
{
	jsval_native_set_t *native;
	jsval_t *values;
	size_t i;

	if (set.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (set.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, set);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	values = jsval_native_set_values(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, values[i], key)) {
			return 0;
		}
	}
	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}
	values[native->len++] = key;
	return 0;
}

int jsval_set_delete(jsval_region_t *region, jsval_t set, jsval_t key,
		int *deleted_ptr)
{
	jsval_native_set_t *native;
	jsval_t *values;
	size_t i;

	if (deleted_ptr == NULL || set.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (set.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, set);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	values = jsval_native_set_values(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, values[i], key)) {
			if (i + 1 < native->len) {
				memmove(values + i, values + i + 1,
						(native->len - i - 1) * sizeof(*values));
			}
			native->len--;
			values[native->len] = jsval_undefined();
			*deleted_ptr = 1;
			return 0;
		}
	}
	*deleted_ptr = 0;
	return 0;
}

int jsval_set_clear(jsval_region_t *region, jsval_t set)
{
	jsval_native_set_t *native;
	jsval_t *values;
	size_t i;

	if (set.kind != JSVAL_KIND_SET) {
		errno = EINVAL;
		return -1;
	}
	if (set.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_set(region, set);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	values = jsval_native_set_values(native);
	for (i = 0; i < native->len; i++) {
		values[i] = jsval_undefined();
	}
	native->len = 0;
	return 0;
}

int jsval_map_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr)
{
	jsval_native_map_t *map;
	jsval_off_t off;
	size_t bytes_len = sizeof(*map) + cap * sizeof(jsval_native_map_entry_t);
	size_t i;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, bytes_len, JSVAL_ALIGN, &off,
			(void **)&map) < 0) {
		return -1;
	}

	map->len = 0;
	map->cap = cap;
	for (i = 0; i < cap; i++) {
		jsval_native_map_entry_t *entry = &jsval_native_map_entries(map)[i];
		entry->key = jsval_undefined();
		entry->value = jsval_undefined();
	}

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_MAP;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_map_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr)
{
	jsval_native_map_t *native;
	jsval_t out;
	jsval_native_map_entry_t *src_entries;
	jsval_native_map_entry_t *dst_entries;
	size_t i;

	if (region == NULL || value_ptr == NULL || src.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (src.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, src);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (capacity < native->len) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsval_map_new(region, capacity, &out) < 0) {
		return -1;
	}

	src_entries = jsval_native_map_entries(native);
	dst_entries = jsval_native_map_entries(jsval_native_map(region, out));
	for (i = 0; i < native->len; i++) {
		dst_entries[i] = src_entries[i];
	}
	jsval_native_map(region, out)->len = native->len;
	*value_ptr = out;
	return 0;
}

int jsval_map_size(jsval_region_t *region, jsval_t map, size_t *size_ptr)
{
	jsval_native_map_t *native;

	if (size_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	*size_ptr = native->len;
	return 0;
}

int jsval_map_has(jsval_region_t *region, jsval_t map, jsval_t key,
		int *has_ptr)
{
	jsval_native_map_t *native;
	jsval_native_map_entry_t *entries;
	size_t i;

	if (has_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	entries = jsval_native_map_entries(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, entries[i].key, key)) {
			*has_ptr = 1;
			return 0;
		}
	}
	*has_ptr = 0;
	return 0;
}

int jsval_map_get(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t *value_ptr)
{
	jsval_native_map_t *native;
	jsval_native_map_entry_t *entries;
	size_t i;

	if (value_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	entries = jsval_native_map_entries(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, entries[i].key, key)) {
			*value_ptr = entries[i].value;
			return 0;
		}
	}
	*value_ptr = jsval_undefined();
	return 0;
}

int jsval_map_set(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t value)
{
	jsval_native_map_t *native;
	jsval_native_map_entry_t *entries;
	size_t i;

	if (map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	entries = jsval_native_map_entries(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, entries[i].key, key)) {
			entries[i].value = value;
			return 0;
		}
	}
	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}
	entries[native->len].key = key;
	entries[native->len].value = value;
	native->len++;
	return 0;
}

int jsval_map_delete(jsval_region_t *region, jsval_t map, jsval_t key,
		int *deleted_ptr)
{
	jsval_native_map_t *native;
	jsval_native_map_entry_t *entries;
	size_t i;

	if (deleted_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	entries = jsval_native_map_entries(native);
	for (i = 0; i < native->len; i++) {
		if (jsval_same_value_zero(region, entries[i].key, key)) {
			if (i + 1 < native->len) {
				memmove(entries + i, entries + i + 1,
						(native->len - i - 1) * sizeof(*entries));
			}
			native->len--;
			entries[native->len].key = jsval_undefined();
			entries[native->len].value = jsval_undefined();
			*deleted_ptr = 1;
			return 0;
		}
	}
	*deleted_ptr = 0;
	return 0;
}

int jsval_map_clear(jsval_region_t *region, jsval_t map)
{
	jsval_native_map_t *native;
	jsval_native_map_entry_t *entries;
	size_t i;

	if (map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	entries = jsval_native_map_entries(native);
	for (i = 0; i < native->len; i++) {
		entries[i].key = jsval_undefined();
		entries[i].value = jsval_undefined();
	}
	native->len = 0;
	return 0;
}

int jsval_map_key_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *key_ptr)
{
	jsval_native_map_t *native;

	if (key_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= native->len) {
		*key_ptr = jsval_undefined();
		return 0;
	}
	*key_ptr = jsval_native_map_entries(native)[index].key;
	return 0;
}

int jsval_map_value_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *value_ptr)
{
	jsval_native_map_t *native;

	if (value_ptr == NULL || map.kind != JSVAL_KIND_MAP) {
		errno = EINVAL;
		return -1;
	}
	if (map.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_map(region, map);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= native->len) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	*value_ptr = jsval_native_map_entries(native)[index].value;
	return 0;
}

static int jsval_iterator_mode_is_entry(jsval_iterator_mode_t mode)
{
	return mode == JSVAL_ITERATOR_MODE_ARRAY_ENTRIES
			|| mode == JSVAL_ITERATOR_MODE_SET_ENTRIES
			|| mode == JSVAL_ITERATOR_MODE_MAP_ENTRIES
			|| mode == JSVAL_ITERATOR_MODE_STRING_ENTRIES;
}

static int jsval_iterator_string_source(jsval_region_t *region,
		jsval_t iterable, jsval_t *source_ptr)
{
	jsval_native_string_t *string;
	jsval_t source_value;
	size_t len;

	if (region == NULL || source_ptr == NULL || iterable.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (iterable.repr == JSVAL_REPR_NATIVE) {
		*source_ptr = iterable;
		return 0;
	}
	if (iterable.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, iterable, &len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, len, &source_value, &string) < 0) {
		return -1;
	}
	if (jsval_value_copy_utf16(region, iterable, jsval_native_string_units(string),
			string->cap, &string->len) < 0) {
		return -1;
	}
	*source_ptr = source_value;
	return 0;
}

static int jsval_iterator_new(jsval_region_t *region,
		jsval_iterator_mode_t mode, jsval_t source_value,
		jsval_t *value_ptr)
{
	jsval_native_iterator_t *iterator;
	jsval_off_t off;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, sizeof(*iterator), JSVAL_ALIGN, &off,
			(void **)&iterator) < 0) {
		return -1;
	}
	iterator->source_value = source_value;
	iterator->cursor = 0;
	iterator->mode = (uint8_t)mode;
	iterator->done = 0;
	memset(iterator->reserved, 0, sizeof(iterator->reserved));

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_ITERATOR;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int jsval_get_iterator(jsval_region_t *region, jsval_t iterable,
		jsval_iterator_selector_t selector, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_iterator_mode_t mode;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);

	switch (iterable.kind) {
	case JSVAL_KIND_STRING:
		if (jsval_iterator_string_source(region, iterable, &iterable) < 0) {
			return -1;
		}
		switch (selector) {
		case JSVAL_ITERATOR_SELECTOR_DEFAULT:
		case JSVAL_ITERATOR_SELECTOR_VALUES:
			mode = JSVAL_ITERATOR_MODE_STRING_VALUES;
			break;
		case JSVAL_ITERATOR_SELECTOR_KEYS:
			mode = JSVAL_ITERATOR_MODE_STRING_KEYS;
			break;
		case JSVAL_ITERATOR_SELECTOR_ENTRIES:
			mode = JSVAL_ITERATOR_MODE_STRING_ENTRIES;
			break;
		default:
			errno = EINVAL;
			return -1;
		}
		return jsval_iterator_new(region, mode, iterable, value_ptr);
	case JSVAL_KIND_ARRAY:
		switch (selector) {
		case JSVAL_ITERATOR_SELECTOR_DEFAULT:
		case JSVAL_ITERATOR_SELECTOR_VALUES:
			mode = JSVAL_ITERATOR_MODE_ARRAY_VALUES;
			break;
		case JSVAL_ITERATOR_SELECTOR_KEYS:
			mode = JSVAL_ITERATOR_MODE_ARRAY_KEYS;
			break;
		case JSVAL_ITERATOR_SELECTOR_ENTRIES:
			mode = JSVAL_ITERATOR_MODE_ARRAY_ENTRIES;
			break;
		default:
			errno = EINVAL;
			return -1;
		}
		return jsval_iterator_new(region, mode, iterable, value_ptr);
	case JSVAL_KIND_SET:
		if (iterable.repr != JSVAL_REPR_NATIVE
				|| jsval_native_set(region, iterable) == NULL) {
			errno = ENOTSUP;
			return -1;
		}
		switch (selector) {
		case JSVAL_ITERATOR_SELECTOR_DEFAULT:
		case JSVAL_ITERATOR_SELECTOR_VALUES:
			mode = JSVAL_ITERATOR_MODE_SET_VALUES;
			break;
		case JSVAL_ITERATOR_SELECTOR_KEYS:
			mode = JSVAL_ITERATOR_MODE_SET_KEYS;
			break;
		case JSVAL_ITERATOR_SELECTOR_ENTRIES:
			mode = JSVAL_ITERATOR_MODE_SET_ENTRIES;
			break;
		default:
			errno = EINVAL;
			return -1;
		}
		return jsval_iterator_new(region, mode, iterable, value_ptr);
	case JSVAL_KIND_MAP:
		if (iterable.repr != JSVAL_REPR_NATIVE
				|| jsval_native_map(region, iterable) == NULL) {
			errno = ENOTSUP;
			return -1;
		}
		switch (selector) {
		case JSVAL_ITERATOR_SELECTOR_DEFAULT:
		case JSVAL_ITERATOR_SELECTOR_ENTRIES:
			mode = JSVAL_ITERATOR_MODE_MAP_ENTRIES;
			break;
		case JSVAL_ITERATOR_SELECTOR_KEYS:
			mode = JSVAL_ITERATOR_MODE_MAP_KEYS;
			break;
		case JSVAL_ITERATOR_SELECTOR_VALUES:
			mode = JSVAL_ITERATOR_MODE_MAP_VALUES;
			break;
		default:
			errno = EINVAL;
			return -1;
		}
		return jsval_iterator_new(region, mode, iterable, value_ptr);
	case JSVAL_KIND_MATCH_ITERATOR:
		if (selector == JSVAL_ITERATOR_SELECTOR_DEFAULT
				|| selector == JSVAL_ITERATOR_SELECTOR_VALUES) {
			*value_ptr = iterable;
			return 0;
		}
		errno = EINVAL;
		return -1;
	case JSVAL_KIND_ITERATOR:
		if (selector == JSVAL_ITERATOR_SELECTOR_DEFAULT) {
			*value_ptr = iterable;
			return 0;
		}
		errno = EINVAL;
		return -1;
	default:
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "iterable required";
		}
		return -1;
	}
}

static int jsval_iterator_next_native(jsval_region_t *region,
		jsval_native_iterator_t *iterator, int want_entry, int *done_ptr,
		jsval_t *key_ptr, jsval_t *value_ptr)
{
	jsval_iterator_mode_t mode;

	if (region == NULL || iterator == NULL || done_ptr == NULL
			|| value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	mode = (jsval_iterator_mode_t)iterator->mode;
	if ((want_entry != 0) != jsval_iterator_mode_is_entry(mode)) {
		errno = ENOTSUP;
		return -1;
	}
	if (iterator->done) {
		*done_ptr = 1;
		if (key_ptr != NULL) {
			*key_ptr = jsval_undefined();
		}
		*value_ptr = jsval_undefined();
		return 0;
	}

	switch (mode) {
	case JSVAL_ITERATOR_MODE_ARRAY_VALUES:
	case JSVAL_ITERATOR_MODE_ARRAY_KEYS:
	case JSVAL_ITERATOR_MODE_ARRAY_ENTRIES:
	{
		size_t index = iterator->cursor;
		size_t len = jsval_array_length(region, iterator->source_value);
		jsval_t element;

		if (index >= len) {
			iterator->done = 1;
			*done_ptr = 1;
			if (key_ptr != NULL) {
				*key_ptr = jsval_undefined();
			}
			*value_ptr = jsval_undefined();
			return 0;
		}
		if (mode != JSVAL_ITERATOR_MODE_ARRAY_KEYS
				&& jsval_array_get(region, iterator->source_value, index,
						&element) < 0) {
			return -1;
		}
		iterator->cursor++;
		*done_ptr = 0;
		if (mode == JSVAL_ITERATOR_MODE_ARRAY_VALUES) {
			*value_ptr = element;
			return 0;
		}
		if (mode == JSVAL_ITERATOR_MODE_ARRAY_KEYS) {
			*value_ptr = jsval_number((double)index);
			return 0;
		}
		*key_ptr = jsval_number((double)index);
		*value_ptr = element;
		return 0;
	}
	case JSVAL_ITERATOR_MODE_SET_VALUES:
	case JSVAL_ITERATOR_MODE_SET_KEYS:
	case JSVAL_ITERATOR_MODE_SET_ENTRIES:
	{
		jsval_native_set_t *set = jsval_native_set(region,
				iterator->source_value);
		jsval_t *values;
		jsval_t element;

		if (set == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (iterator->cursor >= set->len) {
			iterator->done = 1;
			*done_ptr = 1;
			if (key_ptr != NULL) {
				*key_ptr = jsval_undefined();
			}
			*value_ptr = jsval_undefined();
			return 0;
		}
		values = jsval_native_set_values(set);
		element = values[iterator->cursor++];
		*done_ptr = 0;
		if (mode == JSVAL_ITERATOR_MODE_SET_ENTRIES) {
			*key_ptr = element;
			*value_ptr = element;
			return 0;
		}
		*value_ptr = element;
		return 0;
	}
	case JSVAL_ITERATOR_MODE_MAP_KEYS:
	case JSVAL_ITERATOR_MODE_MAP_VALUES:
	case JSVAL_ITERATOR_MODE_MAP_ENTRIES:
	{
		jsval_native_map_t *map = jsval_native_map(region,
				iterator->source_value);
		jsval_native_map_entry_t *entries;
		jsval_native_map_entry_t entry;

		if (map == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (iterator->cursor >= map->len) {
			iterator->done = 1;
			*done_ptr = 1;
			if (key_ptr != NULL) {
				*key_ptr = jsval_undefined();
			}
			*value_ptr = jsval_undefined();
			return 0;
		}
		entries = jsval_native_map_entries(map);
		entry = entries[iterator->cursor++];
		*done_ptr = 0;
		if (mode == JSVAL_ITERATOR_MODE_MAP_KEYS) {
			*value_ptr = entry.key;
			return 0;
		}
		if (mode == JSVAL_ITERATOR_MODE_MAP_VALUES) {
			*value_ptr = entry.value;
			return 0;
		}
		*key_ptr = entry.key;
		*value_ptr = entry.value;
		return 0;
	}
	case JSVAL_ITERATOR_MODE_STRING_VALUES:
	case JSVAL_ITERATOR_MODE_STRING_KEYS:
	case JSVAL_ITERATOR_MODE_STRING_ENTRIES:
	{
		jsval_native_string_t *string = jsval_native_string(region,
				iterator->source_value);
		jsstr16_t source;
		uint16_t *units;
		size_t index = iterator->cursor;
		size_t step_len;

		if (string == NULL) {
			errno = EINVAL;
			return -1;
		}
		units = jsval_native_string_units(string);
		jsstr16_init_from_buf(&source, (const char *)units,
				string->len * sizeof(*units));
		source.len = string->len;
		if (index >= source.len) {
			iterator->done = 1;
			*done_ptr = 1;
			if (key_ptr != NULL) {
				*key_ptr = jsval_undefined();
			}
			*value_ptr = jsval_undefined();
			return 0;
		}
		step_len = jsstr16_u16_codepoint_at(&source, index, NULL);
		if (step_len == 0) {
			errno = EINVAL;
			return -1;
		}
		iterator->cursor += step_len;
		*done_ptr = 0;
		if (mode == JSVAL_ITERATOR_MODE_STRING_KEYS) {
			*value_ptr = jsval_number((double)index);
			return 0;
		}
		if (jsval_string_new_utf16(region, units + index, step_len,
				value_ptr) < 0) {
			return -1;
		}
		if (mode == JSVAL_ITERATOR_MODE_STRING_ENTRIES) {
			*key_ptr = jsval_number((double)index);
		}
		return 0;
	}
	default:
		errno = EINVAL;
		return -1;
	}
}

int jsval_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_iterator_t *iterator;

	if (iterator_value.kind == JSVAL_KIND_MATCH_ITERATOR) {
#if JSMX_WITH_REGEX
		return jsval_match_iterator_next(region, iterator_value, done_ptr,
				value_ptr, error);
#else
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "iterator required";
		}
		return -1;
#endif
	}
	if (region == NULL || done_ptr == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	iterator = jsval_native_iterator(region, iterator_value);
	if (iterator == NULL) {
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "iterator required";
		}
		return -1;
	}
	return jsval_iterator_next_native(region, iterator, 0, done_ptr, NULL,
			value_ptr);
}

int jsval_iterator_next_entry(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *key_ptr, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_native_iterator_t *iterator;

	if (region == NULL || done_ptr == NULL || key_ptr == NULL
			|| value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (iterator_value.kind == JSVAL_KIND_MATCH_ITERATOR) {
		errno = ENOTSUP;
		return -1;
	}
	iterator = jsval_native_iterator(region, iterator_value);
	if (iterator == NULL) {
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "iterator required";
		}
		return -1;
	}
	return jsval_iterator_next_native(region, iterator, 1, done_ptr, key_ptr,
			value_ptr);
}

#if JSMX_WITH_REGEX
static int
jsval_regexp_flags_copy_utf16(uint32_t flags, uint16_t *buf, size_t cap,
		size_t *len_ptr)
{
	static const char *order = "gimsuy";
	size_t len = 0;
	size_t i;

	for (i = 0; order[i] != '\0'; i++) {
		uint32_t bit = 0;

		switch (order[i]) {
		case 'g':
			bit = JSREGEX_FLAG_GLOBAL;
			break;
		case 'i':
			bit = JSREGEX_FLAG_IGNORE_CASE;
			break;
		case 'm':
			bit = JSREGEX_FLAG_MULTILINE;
			break;
		case 's':
			bit = JSREGEX_FLAG_DOT_ALL;
			break;
		case 'u':
			bit = JSREGEX_FLAG_UNICODE;
			break;
		case 'y':
			bit = JSREGEX_FLAG_STICKY;
			break;
		default:
			break;
		}
		if ((flags & bit) != 0) {
			if (buf != NULL) {
				if (len >= cap) {
					errno = ENOBUFS;
					return -1;
				}
				buf[len] = (uint8_t)order[i];
			}
			len++;
		}
	}

	if (len_ptr != NULL) {
		*len_ptr = len;
	}
	return 0;
}

static int
jsval_regexp_slot_key(size_t index, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	uint8_t tmp[32];
	size_t len = 0;

	if (buf == NULL || cap == 0) {
		errno = EINVAL;
		return -1;
	}
	do {
		tmp[len++] = (uint8_t)('0' + (index % 10));
		index /= 10;
	} while (index > 0 && len < sizeof(tmp));
	if (index > 0 || len > cap) {
		errno = ENOBUFS;
		return -1;
	}
	for (size_t i = 0; i < len; i++) {
		buf[i] = tmp[len - 1 - i];
	}
	if (len_ptr != NULL) {
		*len_ptr = len;
	}
	return 0;
}

static int
jsval_regexp_init_cached_key(jsval_region_t *region, const uint8_t *key,
		size_t key_len, jsval_off_t *off_ptr)
{
	jsval_t key_value;

	if (region == NULL || off_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_new_utf8(region, key, key_len, &key_value) < 0) {
		return -1;
	}
	if (key_value.kind != JSVAL_KIND_STRING
			|| key_value.repr != JSVAL_REPR_NATIVE) {
		errno = EINVAL;
		return -1;
	}
	*off_ptr = key_value.off;
	return 0;
}

static int
jsval_regexp_new_from_utf16(jsval_region_t *region, const uint16_t *pattern,
		size_t pattern_len, const uint16_t *flags, size_t flags_len,
		int use_jit, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsregex_compiled_t compiled;
	jsval_native_regexp_t *regexp;
	jsval_native_named_group_t *named_groups = NULL;
	jsval_t source_value;
	jsval_off_t named_groups_off = 0;
	jsval_off_t off;
	size_t i;

	if (region == NULL || value_ptr == NULL
			|| (pattern_len > 0 && pattern == NULL)
			|| (flags_len > 0 && flags == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if ((use_jit
			? jsregex_compile_utf16_jit(pattern, pattern_len, flags, flags_len,
					&compiled)
			: jsregex_compile_utf16(pattern, pattern_len, flags, flags_len,
					&compiled)) < 0) {
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_SYNTAX;
			error->message = errno == ENOTSUP
				? "unsupported regular expression flags"
				: "invalid regular expression";
		}
		return -1;
	}
	if (jsval_string_new_utf16(region, pattern, pattern_len, &source_value) < 0) {
		jsregex_release(&compiled);
		return -1;
	}
	if (compiled.named_group_count > 0) {
		if (jsval_region_reserve(region,
				(size_t)compiled.named_group_count * sizeof(*named_groups),
				JSVAL_ALIGN, &named_groups_off, (void **)&named_groups) < 0) {
			jsregex_release(&compiled);
			return -1;
		}
		for (i = 0; i < compiled.named_group_count; i++) {
			uint32_t capture_index = 0;
			size_t name_len = 0;
			jsval_t name_value;

			if (jsregex_named_group_utf16(&compiled, i, &capture_index, NULL, 0,
					&name_len) < 0) {
				jsregex_release(&compiled);
				return -1;
			}
			{
				uint16_t name_buf[name_len ? name_len : 1];

				if (jsregex_named_group_utf16(&compiled, i, &capture_index,
						name_len > 0 ? name_buf : NULL, name_len,
						&name_len) < 0) {
					jsregex_release(&compiled);
					return -1;
				}
				if (jsval_string_new_utf16(region,
						name_len > 0 ? name_buf : NULL, name_len,
						&name_value) < 0) {
					jsregex_release(&compiled);
					return -1;
				}
			}
			if (name_value.kind != JSVAL_KIND_STRING
					|| name_value.repr != JSVAL_REPR_NATIVE) {
				jsregex_release(&compiled);
				errno = EINVAL;
				return -1;
			}
			named_groups[i].name_off = name_value.off;
			named_groups[i].capture_index = capture_index;
		}
		for (i = 1; i < compiled.named_group_count; i++) {
			size_t j = i;
			jsval_native_named_group_t entry = named_groups[i];

			while (j > 0
					&& named_groups[j - 1].capture_index > entry.capture_index) {
				named_groups[j] = named_groups[j - 1];
				j--;
			}
			named_groups[j] = entry;
		}
	}
	if (jsval_region_reserve(region, sizeof(*regexp), JSVAL_ALIGN, &off,
			(void **)&regexp) < 0) {
		jsregex_release(&compiled);
		return -1;
	}

	regexp->source_off = source_value.off;
	regexp->named_groups_off = named_groups_off;
	regexp->flags = compiled.flags;
	regexp->capture_count = compiled.capture_count;
	regexp->named_group_count = compiled.named_group_count;
	regexp->last_index = 0;
	regexp->backend_code = compiled.backend_code;

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_REGEXP;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int
jsval_regexp_source(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_STRING;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = regexp->source_off;
	return 0;
}

static int
jsval_regexp_flag_bool(jsval_region_t *region, jsval_t regexp_value,
		uint32_t flag, int *result_ptr)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*result_ptr = (regexp->flags & flag) != 0;
	return 0;
}

int
jsval_regexp_global(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_GLOBAL, result_ptr);
}

int
jsval_regexp_ignore_case(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_IGNORE_CASE, result_ptr);
}

int
jsval_regexp_multiline(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_MULTILINE, result_ptr);
}

int
jsval_regexp_dot_all(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_DOT_ALL, result_ptr);
}

int
jsval_regexp_unicode(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_UNICODE, result_ptr);
}

int
jsval_regexp_sticky(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr)
{
	return jsval_regexp_flag_bool(region, regexp_value,
			JSREGEX_FLAG_STICKY, result_ptr);
}

int
jsval_regexp_flags(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	size_t flags_len = 0;

	if (regexp == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_flags_copy_utf16(regexp->flags, NULL, 0, &flags_len) < 0) {
		return -1;
	}
	{
		uint16_t flags_buf[flags_len ? flags_len : 1];

		if (jsval_regexp_flags_copy_utf16(regexp->flags, flags_buf,
				flags_len ? flags_len : 1, &flags_len) < 0) {
			return -1;
		}
		return jsval_string_new_utf16(region, flags_buf, flags_len, value_ptr);
	}
}

int
jsval_regexp_info(jsval_region_t *region, jsval_t regexp_value,
		jsval_regexp_info_t *info_ptr)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL || info_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	info_ptr->flags = regexp->flags;
	info_ptr->capture_count = regexp->capture_count;
	info_ptr->last_index = regexp->last_index;
	return 0;
}

static int
jsval_regexp_exec_raw(jsval_region_t *region, jsval_t regexp_value,
		const uint16_t *subject, size_t subject_len, size_t *offsets,
		size_t offsets_cap, jsregex_exec_result_t *result_ptr,
		jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	jsregex_compiled_t compiled;
	size_t start_index = 0;
	int use_last_index = 0;

	if (regexp == NULL || result_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		if (error != NULL && regexp == NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "RegExp receiver required";
		}
		errno = regexp == NULL ? EINVAL : EINVAL;
		return -1;
	}

	use_last_index = (regexp->flags & (JSREGEX_FLAG_GLOBAL | JSREGEX_FLAG_STICKY)) != 0;
	if (use_last_index) {
		start_index = regexp->last_index;
		if (start_index > subject_len) {
			regexp->last_index = 0;
			result_ptr->matched = 0;
			result_ptr->start = 0;
			result_ptr->end = 0;
			result_ptr->slot_count = 0;
			return 0;
		}
	}

	compiled.backend_code = regexp->backend_code;
	compiled.flags = regexp->flags;
	compiled.capture_count = regexp->capture_count;
	compiled.named_group_count = regexp->named_group_count;
	if (jsregex_exec_utf16(&compiled, subject, subject_len, start_index,
			offsets, offsets_cap, result_ptr) < 0) {
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_SYNTAX;
			error->message = "invalid regular expression";
		}
		return -1;
	}
	if (!result_ptr->matched) {
		if (use_last_index) {
			regexp->last_index = 0;
		}
		return 0;
	}
	if (use_last_index) {
		regexp->last_index = result_ptr->end;
	}
	return 0;
}

static int
jsval_regexp_capture_values(jsval_region_t *region, const uint16_t *subject,
		const size_t *offsets, size_t slot_count, jsval_t *capture_values)
{
	size_t i;

	if (region == NULL || offsets == NULL || capture_values == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < slot_count; i++) {
		capture_values[i] = jsval_undefined();
		if (offsets[i * 2] != SIZE_MAX) {
			size_t start = offsets[i * 2];
			size_t end = offsets[i * 2 + 1];

			if (jsval_string_new_utf16(region, subject + start, end - start,
					&capture_values[i]) < 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int
jsval_regexp_groups_object(jsval_region_t *region,
		jsval_native_regexp_t *regexp, const jsval_t *capture_values,
		size_t slot_count, jsval_t *value_ptr)
{
	jsval_native_named_group_t *named_groups;
	jsval_t groups;
	size_t i;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (regexp == NULL || regexp->named_group_count == 0) {
		*value_ptr = jsval_undefined();
		return 0;
	}
	if (capture_values == NULL) {
		errno = EINVAL;
		return -1;
	}

	named_groups = jsval_native_regexp_named_groups(region, regexp);
	if (named_groups == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_object_new(region, regexp->named_group_count, &groups) < 0) {
		return -1;
	}
	for (i = 0; i < regexp->named_group_count; i++) {
		jsval_t capture_value = jsval_undefined();
		uint32_t capture_index = named_groups[i].capture_index;

		if ((size_t)capture_index < slot_count) {
			capture_value = capture_values[capture_index];
		}
		if (jsval_object_append_native_prop(region, groups,
				named_groups[i].name_off, capture_value) < 0) {
			return -1;
		}
	}

	*value_ptr = groups;
	return 0;
}

static int
jsval_regexp_exec_result_object(jsval_region_t *region,
		jsval_native_regexp_t *regexp, jsval_t input_value,
		const uint16_t *subject, size_t subject_len, const size_t *offsets,
		size_t slot_count, size_t match_index, jsval_t *value_ptr)
{
	jsval_t object;
	jsval_t groups;
	size_t prop_cap = slot_count + 4;
	size_t i;

	if (region == NULL || value_ptr == NULL || offsets == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (regexp != NULL) {
		jsval_t capture_values[slot_count ? slot_count : 1];

		if (jsval_regexp_capture_values(region, subject, offsets, slot_count,
				capture_values) < 0) {
			return -1;
		}
		if (jsval_object_new(region, prop_cap, &object) < 0) {
			return -1;
		}
		for (i = 0; i < slot_count; i++) {
			uint8_t key_buf[32];
			size_t key_len = 0;
			jsval_off_t key_off = 0;

			if (jsval_regexp_slot_key(i, key_buf, sizeof(key_buf), &key_len) < 0) {
				return -1;
			}
			if (jsval_regexp_init_cached_key(region, key_buf, key_len,
					&key_off) < 0) {
				return -1;
			}
			if (jsval_object_append_native_prop(region, object, key_off,
					capture_values[i]) < 0) {
				return -1;
			}
		}
		{
			jsval_off_t length_key_off = 0;
			jsval_off_t index_key_off = 0;
			jsval_off_t input_key_off = 0;
			jsval_off_t groups_key_off = 0;

			if (jsval_regexp_init_cached_key(region,
					(const uint8_t *)"length", 6, &length_key_off) < 0
					|| jsval_regexp_init_cached_key(region,
						(const uint8_t *)"index", 5, &index_key_off) < 0
					|| jsval_regexp_init_cached_key(region,
						(const uint8_t *)"input", 5, &input_key_off) < 0
					|| jsval_regexp_init_cached_key(region,
						(const uint8_t *)"groups", 6, &groups_key_off) < 0) {
				return -1;
			}
			if (jsval_object_append_native_prop(region, object, length_key_off,
					jsval_number((double)slot_count)) < 0
					|| jsval_object_append_native_prop(region, object, index_key_off,
						jsval_number((double)match_index)) < 0
					|| jsval_object_append_native_prop(region, object, input_key_off,
						input_value) < 0) {
				return -1;
			}
			if (jsval_regexp_groups_object(region, regexp, capture_values,
					slot_count, &groups) < 0) {
				return -1;
			}
			if (jsval_object_append_native_prop(region, object, groups_key_off,
					groups) < 0) {
				return -1;
			}
		}
		*value_ptr = object;
		return 0;
	}
	if (jsval_object_new(region, prop_cap, &object) < 0) {
		return -1;
	}

	for (i = 0; i < slot_count; i++) {
		uint8_t key[32];
		size_t key_len;
		jsval_t capture_value = jsval_undefined();

		if (jsval_regexp_slot_key(i, key, sizeof(key), &key_len) < 0) {
			return -1;
		}
		if (offsets[i * 2] != SIZE_MAX) {
			size_t start = offsets[i * 2];
			size_t end = offsets[i * 2 + 1];

			if (jsval_string_new_utf16(region, subject + start, end - start,
					&capture_value) < 0) {
				return -1;
			}
		}
		if (jsval_object_set_utf8(region, object, key, key_len,
				capture_value) < 0) {
			return -1;
		}
	}
	if (jsval_object_set_utf8(region, object, (const uint8_t *)"length", 6,
			jsval_number((double)slot_count)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, object, (const uint8_t *)"index", 5,
			jsval_number((double)match_index)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, object, (const uint8_t *)"input", 5,
			input_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, object, (const uint8_t *)"groups", 6,
			jsval_undefined()) < 0) {
		return -1;
	}

	*value_ptr = object;
	(void)subject_len;
	return 0;
}

static int
jsval_u_literal_surrogate_find_match(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t from_index,
		int *matched_ptr, size_t *start_ptr, size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_surrogate_utf16(subject, subject_len,
			surrogate_unit, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_match_exec_result_object(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		size_t match_start, size_t match_end, jsval_t *value_ptr)
{
	size_t offsets[2];

	if (match_start > match_end || match_end > subject_len) {
		errno = EINVAL;
		return -1;
	}
	offsets[0] = match_start;
	offsets[1] = match_end;
	return jsval_regexp_exec_result_object(region, NULL, input_value, subject,
			subject_len, offsets, 1, match_start, value_ptr);
}

static int
jsval_u_literal_surrogate_exec_result_object(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		size_t match_start, size_t match_end, jsval_t *value_ptr)
{
	return jsval_u_literal_match_exec_result_object(region, input_value,
			subject, subject_len, match_start, match_end, value_ptr);
}

static int
jsval_u_literal_surrogate_count_matches(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, int global,
		size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, 0, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_sequence_find_match(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| pattern == NULL || pattern_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_sequence_utf16(subject, subject_len,
			pattern, pattern_len, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_sequence_exec_result_object(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		size_t match_start, size_t match_end, jsval_t *value_ptr)
{
	return jsval_u_literal_match_exec_result_object(region, input_value,
			subject, subject_len, match_start, match_end, value_ptr);
}

static int
jsval_u_literal_sequence_count_matches(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || pattern == NULL || pattern_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_class_find_match(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_class_utf16(subject, subject_len,
			members, members_len, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_negated_class_find_match(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_negated_class_utf16(subject, subject_len,
			members, members_len, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_range_class_find_match(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_range_class_utf16(subject, subject_len,
			ranges, range_count, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_negated_range_class_find_match(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_literal_negated_range_class_utf16(subject,
			subject_len, ranges, range_count, from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_literal_class_exec_result_object(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		size_t match_start, size_t match_end, jsval_t *value_ptr)
{
	return jsval_u_literal_match_exec_result_object(region, input_value,
			subject, subject_len, match_start, match_end, value_ptr);
}

static int
jsval_u_literal_class_count_matches(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_range_class_count_matches(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_negated_range_class_count_matches(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, cursor, &matched,
				&match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_negated_class_count_matches(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_predefined_class_find_match(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		size_t from_index, int *matched_ptr, size_t *start_ptr,
		size_t *end_ptr)
{
	jsregex_search_result_t result;

	if (matched_ptr == NULL || start_ptr == NULL || end_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_search_u_predefined_class_utf16(subject, subject_len, kind,
			from_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	*start_ptr = result.start;
	*end_ptr = result.end;
	return 0;
}

static int
jsval_u_predefined_class_count_matches(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		int global, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!global) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind, 0,
				&matched, &match_start, &match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind,
				cursor, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_regexp_new_impl(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		int use_jit, jsmethod_error_t *error)
{
	jsval_t pattern_string;
	jsval_t flags_string = jsval_undefined();

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);

	if (pattern_value.kind == JSVAL_KIND_REGEXP) {
		jsval_t source_value;
		jsval_native_regexp_t *regexp = jsval_native_regexp(region, pattern_value);
		size_t flags_len = 0;

		if (regexp == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_regexp_source(region, pattern_value, &source_value) < 0) {
			return -1;
		}
		pattern_string = source_value;
		if (have_flags && flags_value.kind != JSVAL_KIND_UNDEFINED) {
			if (jsval_stringify_value_to_native(region, flags_value, 0,
					&flags_string, error) < 0) {
				return -1;
			}
		} else {
			if (jsval_regexp_flags_copy_utf16(regexp->flags, NULL, 0,
					&flags_len) < 0) {
				return -1;
			}
			{
				uint16_t flags_buf[flags_len ? flags_len : 1];

				if (jsval_regexp_flags_copy_utf16(regexp->flags, flags_buf,
						flags_len ? flags_len : 1, &flags_len) < 0) {
					return -1;
				}
				if (jsval_string_new_utf16(region, flags_buf, flags_len,
						&flags_string) < 0) {
					return -1;
				}
			}
		}
	} else {
		if (jsval_stringify_value_to_native(region, pattern_value, 0,
				&pattern_string, error) < 0) {
			return -1;
		}
		if (have_flags && flags_value.kind != JSVAL_KIND_UNDEFINED) {
			if (jsval_stringify_value_to_native(region, flags_value, 0,
					&flags_string, error) < 0) {
				return -1;
			}
		}
	}

	{
		jsval_native_string_t *pattern_native = jsval_native_string(region,
				pattern_string);
		jsval_native_string_t *flags_native = flags_string.kind == JSVAL_KIND_STRING
			? jsval_native_string(region, flags_string) : NULL;

		if (pattern_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		return jsval_regexp_new_from_utf16(region,
				jsval_native_string_units(pattern_native), pattern_native->len,
				flags_native != NULL ? jsval_native_string_units(flags_native) : NULL,
				flags_native != NULL ? flags_native->len : 0,
				use_jit, value_ptr, error);
	}
}

int
jsval_regexp_new(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_regexp_new_impl(region, pattern_value, have_flags,
			flags_value, value_ptr, 0, error);
}

int
jsval_regexp_new_jit(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_regexp_new_impl(region, pattern_value, have_flags,
			flags_value, value_ptr, 1, error);
}

int
jsval_regexp_get_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t *last_index_ptr)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL || last_index_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*last_index_ptr = regexp->last_index;
	return 0;
}

int
jsval_regexp_set_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t last_index)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL) {
		errno = EINVAL;
		return -1;
	}
	regexp->last_index = last_index;
	return 0;
}

int
jsval_regexp_test(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, int *result_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_native_regexp_t *regexp;
	size_t offsets_cap;

	if (region == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, input_value, 0, &input_string,
			error) < 0) {
		return -1;
	}
	regexp = jsval_native_regexp(region, regexp_value);
	input_native = jsval_native_string(region, input_string);
	if (regexp == NULL || input_native == NULL) {
		if (regexp == NULL && error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "RegExp receiver required";
		}
		errno = EINVAL;
		return -1;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		if (jsval_regexp_exec_raw(region, regexp_value,
				jsval_native_string_units(input_native), input_native->len,
				offsets, offsets_cap, &exec_result, error) < 0) {
			return -1;
		}
		*result_ptr = exec_result.matched ? 1 : 0;
		return 0;
	}
}

int
jsval_regexp_exec(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_native_regexp_t *regexp;
	size_t offsets_cap;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, input_value, 0, &input_string,
			error) < 0) {
		return -1;
	}
	regexp = jsval_native_regexp(region, regexp_value);
	input_native = jsval_native_string(region, input_string);
	if (regexp == NULL || input_native == NULL) {
		if (regexp == NULL && error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "RegExp receiver required";
		}
		errno = EINVAL;
		return -1;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		if (jsval_regexp_exec_raw(region, regexp_value,
				jsval_native_string_units(input_native), input_native->len,
				offsets, offsets_cap, &exec_result, error) < 0) {
			return -1;
		}
		if (!exec_result.matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_regexp_exec_result_object(region, regexp, input_string,
				jsval_native_string_units(input_native), input_native->len,
				offsets, exec_result.slot_count, exec_result.start, value_ptr);
	}
}

int
jsval_method_string_match(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t regex_value;
	jsval_native_regexp_t *regexp;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!have_regexp || regexp_value.kind == JSVAL_KIND_UNDEFINED) {
		uint16_t empty_pattern = 0;
		(void)empty_pattern;
		if (jsval_regexp_new_from_utf16(region, NULL, 0, NULL, 0, 0,
				&regex_value,
				error) < 0) {
			return -1;
		}
	} else if (regexp_value.kind == JSVAL_KIND_REGEXP) {
		regex_value = regexp_value;
	} else {
		if (jsval_regexp_new(region, regexp_value, 0, jsval_undefined(),
				&regex_value, error) < 0) {
			return -1;
		}
	}

	regexp = jsval_native_regexp(region, regex_value);
	if (regexp == NULL) {
		errno = EINVAL;
		return -1;
	}

	if ((regexp->flags & JSREGEX_FLAG_GLOBAL) != 0) {
		size_t offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
		size_t match_count = 0;
		jsval_t array;
		size_t write_index = 0;

		regexp->last_index = 0;
		{
			size_t offsets[offsets_cap ? offsets_cap : 2];
			jsregex_exec_result_t exec_result;

			for (;;) {
				if (jsval_regexp_exec_raw(region, regex_value,
						jsval_native_string_units(input_native), input_native->len,
						offsets, offsets_cap, &exec_result, error) < 0) {
					return -1;
				}
				if (!exec_result.matched) {
					break;
				}
				match_count++;
				if (exec_result.start == exec_result.end) {
					size_t next_index;

					if (jsregex_advance_string_index_utf16(
							jsval_native_string_units(input_native), input_native->len,
							regexp->last_index,
							(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
							&next_index) < 0) {
						return -1;
					}
					regexp->last_index = next_index;
				}
			}
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		regexp->last_index = 0;
		{
			size_t offsets[offsets_cap ? offsets_cap : 2];
			jsregex_exec_result_t exec_result;

			for (;;) {
				jsval_t capture_value;

				if (jsval_regexp_exec_raw(region, regex_value,
						jsval_native_string_units(input_native), input_native->len,
						offsets, offsets_cap, &exec_result, error) < 0) {
					return -1;
				}
				if (!exec_result.matched) {
					break;
				}
				if (jsval_string_new_utf16(region,
						jsval_native_string_units(input_native) + exec_result.start,
						exec_result.end - exec_result.start,
						&capture_value) < 0) {
					return -1;
				}
				if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
					return -1;
				}
				if (exec_result.start == exec_result.end) {
					size_t next_index;

					if (jsregex_advance_string_index_utf16(
							jsval_native_string_units(input_native), input_native->len,
							regexp->last_index,
							(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
							&next_index) < 0) {
						return -1;
					}
					regexp->last_index = next_index;
				}
			}
		}
		*value_ptr = array;
		return 0;
	}

	return jsval_regexp_exec(region, regex_value, input_string, value_ptr, error);
}

static int
jsval_regexp_clone_with_flags(jsval_region_t *region, jsval_t regexp_value,
		uint32_t flags, int copy_last_index, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	jsval_t source_value;
	jsval_native_string_t *source_string;
	size_t flags_len = 0;

	if (regexp == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_source(region, regexp_value, &source_value) < 0) {
		return -1;
	}
	source_string = jsval_native_string(region, source_value);
	if (source_string == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_flags_copy_utf16(flags, NULL, 0, &flags_len) < 0) {
		return -1;
	}
	{
		uint16_t flags_buf[flags_len ? flags_len : 1];

		if (jsval_regexp_flags_copy_utf16(flags, flags_buf,
				flags_len ? flags_len : 1, &flags_len) < 0) {
			return -1;
		}
		if (jsval_regexp_new_from_utf16(region,
				jsval_native_string_units(source_string), source_string->len,
				flags_len > 0 ? flags_buf : NULL, flags_len, 0, value_ptr,
				error) < 0) {
			return -1;
		}
		if (copy_last_index && jsval_regexp_set_last_index(region, *value_ptr,
				regexp->last_index) < 0) {
			return -1;
		}
		return 0;
	}
}

static int
jsval_regexp_clone_for_split(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_regexp_clone_with_flags(region, regexp_value,
			regexp->flags | JSREGEX_FLAG_STICKY, 0, value_ptr, error);
}

static int
jsval_regexp_clone_for_replace(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);

	if (regexp == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_regexp_clone_with_flags(region, regexp_value, regexp->flags,
			1, value_ptr, error);
}

static int
jsval_match_iterator_new(jsval_region_t *region,
		jsval_match_iterator_mode_t mode, jsval_t search_value,
		jsval_t input_value, size_t cursor, uint16_t surrogate_unit,
		jsval_t *value_ptr)
{
	jsval_native_match_iterator_t *iterator;
	jsval_off_t off;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, sizeof(*iterator), JSVAL_ALIGN, &off,
			(void **)&iterator) < 0) {
		return -1;
	}
	iterator->search_value = search_value;
	iterator->input_value = input_value;
	iterator->cursor = cursor;
	iterator->surrogate_unit = surrogate_unit;
	iterator->done = 0;
	iterator->mode = (uint8_t)mode;
	memset(iterator->reserved, 0, sizeof(iterator->reserved));

	*value_ptr = jsval_undefined();
	value_ptr->kind = JSVAL_KIND_MATCH_ITERATOR;
	value_ptr->repr = JSVAL_REPR_NATIVE;
	value_ptr->off = off;
	return 0;
}

int
jsval_regexp_match_all(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t iterator_regex;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_native_regexp(region, regexp_value) == NULL) {
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "RegExp receiver required";
		}
		return -1;
	}
	if (jsval_stringify_value_to_native(region, input_value, 0, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_regexp_clone_with_flags(region, regexp_value,
			jsval_native_regexp(region, regexp_value)->flags, 1,
			&iterator_regex, error) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region, JSVAL_MATCH_ITERATOR_MODE_REGEX,
			iterator_regex, input_string, 0, 0, value_ptr);
}

int
jsval_match_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_native_match_iterator_t *iterator;
	jsval_native_regexp_t *regexp;
	jsval_native_string_t *input_native;
	size_t offsets_cap;

	if (region == NULL || done_ptr == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	iterator = jsval_native_match_iterator(region, iterator_value);
	if (iterator == NULL) {
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "matchAll iterator required";
		}
		return -1;
	}
	if (iterator->done) {
		*done_ptr = 1;
		*value_ptr = jsval_undefined();
		return 0;
	}

	input_native = jsval_native_string(region, iterator->input_value);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch ((jsval_match_iterator_mode_t)iterator->mode) {
	case JSVAL_MATCH_ITERATOR_MODE_REGEX:
		regexp = jsval_native_regexp(region, iterator->search_value);
		if (regexp == NULL) {
			errno = EINVAL;
			return -1;
		}
		offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
		{
			size_t offsets[offsets_cap ? offsets_cap : 2];
			jsregex_exec_result_t exec_result;

			if (jsval_regexp_exec_raw(region, iterator->search_value,
					jsval_native_string_units(input_native),
					input_native->len, offsets, offsets_cap, &exec_result,
					error) < 0) {
				return -1;
			}
			if (!exec_result.matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_regexp_exec_result_object(region, regexp,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, offsets, exec_result.slot_count,
					exec_result.start, value_ptr) < 0) {
				return -1;
			}
			if ((regexp->flags & JSREGEX_FLAG_GLOBAL) == 0) {
				iterator->done = 1;
			} else if (exec_result.start == exec_result.end) {
				size_t next_index;

				if (jsregex_advance_string_index_utf16(
						jsval_native_string_units(input_native),
						input_native->len, regexp->last_index,
						(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
						&next_index) < 0) {
					return -1;
				}
				regexp->last_index = next_index;
			}
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SURROGATE:
		{
			int matched;
			size_t match_start;
			size_t match_end;

			if (jsval_u_literal_surrogate_find_match(
					jsval_native_string_units(input_native),
					input_native->len, iterator->surrogate_unit,
					iterator->cursor, &matched, &match_start,
					&match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_surrogate_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SEQUENCE:
		{
			jsval_native_string_t *pattern_native;
			int matched;
			size_t match_start;
			size_t match_end;

			pattern_native = jsval_native_string(region, iterator->search_value);
			if (pattern_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_u_literal_sequence_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					jsval_native_string_units(pattern_native),
					pattern_native->len, iterator->cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_sequence_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_CLASS:
		{
			jsval_native_string_t *members_native;
			int matched;
			size_t match_start;
			size_t match_end;

			members_native = jsval_native_string(region, iterator->search_value);
			if (members_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_u_literal_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					jsval_native_string_units(members_native),
					members_native->len, iterator->cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_class_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_CLASS:
		{
			jsval_native_string_t *members_native;
			int matched;
			size_t match_start;
			size_t match_end;

			members_native = jsval_native_string(region, iterator->search_value);
			if (members_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_u_literal_negated_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					jsval_native_string_units(members_native),
					members_native->len, iterator->cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_class_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_RANGE_CLASS:
		{
			jsval_native_string_t *ranges_native;
			int matched;
			size_t match_start;
			size_t match_end;

			ranges_native = jsval_native_string(region, iterator->search_value);
			if (ranges_native == NULL || (ranges_native->len % 2) != 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_u_literal_range_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					jsval_native_string_units(ranges_native),
					ranges_native->len / 2, iterator->cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_class_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_RANGE_CLASS:
		{
			jsval_native_string_t *ranges_native;
			int matched;
			size_t match_start;
			size_t match_end;

			ranges_native = jsval_native_string(region, iterator->search_value);
			if (ranges_native == NULL || (ranges_native->len % 2) != 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_u_literal_negated_range_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					jsval_native_string_units(ranges_native),
					ranges_native->len / 2, iterator->cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_class_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	case JSVAL_MATCH_ITERATOR_MODE_U_PREDEFINED_CLASS:
		{
			int matched;
			size_t match_start;
			size_t match_end;

			if (jsval_u_predefined_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len,
					(jsregex_u_predefined_class_kind_t)
						iterator->surrogate_unit,
					iterator->cursor, &matched, &match_start,
					&match_end) < 0) {
				return -1;
			}
			if (!matched) {
				iterator->done = 1;
				*done_ptr = 1;
				*value_ptr = jsval_undefined();
				return 0;
			}
			if (jsval_u_literal_class_exec_result_object(region,
					iterator->input_value,
					jsval_native_string_units(input_native),
					input_native->len, match_start, match_end,
					value_ptr) < 0) {
				return -1;
			}
			iterator->cursor = match_end;
			*done_ptr = 0;
			return 0;
		}
	default:
		errno = EINVAL;
		return -1;
	}
}

int
jsval_method_string_match_all(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t regex_value;
	jsval_t flags_value;
	jsval_native_regexp_t *regexp;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}

	if (!have_regexp || regexp_value.kind == JSVAL_KIND_UNDEFINED) {
		static const uint16_t global_flag[] = {'g'};

		if (jsval_regexp_new_from_utf16(region, NULL, 0, global_flag, 1, 0,
				&regex_value, error) < 0) {
			return -1;
		}
	} else if (regexp_value.kind == JSVAL_KIND_REGEXP) {
		regexp = jsval_native_regexp(region, regexp_value);
		if (regexp == NULL) {
			errno = EINVAL;
			return -1;
		}
		if ((regexp->flags & JSREGEX_FLAG_GLOBAL) == 0) {
			errno = EINVAL;
			if (error != NULL) {
				error->kind = JSMETHOD_ERROR_TYPE;
				error->message = "matchAll requires a global RegExp";
			}
			return -1;
		}
		regex_value = regexp_value;
	} else {
		if (jsval_string_new_utf8(region, (const uint8_t *)"g", 1,
				&flags_value) < 0) {
			return -1;
		}
		if (jsval_regexp_new(region, regexp_value, 1, flags_value,
				&regex_value, error) < 0) {
			return -1;
		}
	}

	return jsval_regexp_match_all(region, regex_value, input_string, value_ptr,
			error);
}

int
jsval_method_string_match_all_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SURROGATE,
			jsval_undefined(), input_string, 0, surrogate_unit, value_ptr);
}

static int
jsval_u_literal_range_string_new(jsval_region_t *region,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr)
{
	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	if (range_count > SIZE_MAX / 2) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf16(region, ranges, range_count * 2, value_ptr);
}

static int
jsval_method_string_search_u_predefined_class_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_predefined_class_find_match(
			jsval_native_string_units(input_native), input_native->len,
			kind, 0, &matched, &match_start, &match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

static int
jsval_method_string_match_all_u_predefined_class_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_PREDEFINED_CLASS,
			jsval_undefined(), input_string, 0, (uint16_t)kind, value_ptr);
}

static int
jsval_method_string_match_u_predefined_class_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind, int global,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_predefined_class_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				kind, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_predefined_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len, kind, cursor, &matched,
					&match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(
				jsval_native_string_units(input_native), input_native->len,
				kind, 0, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_class_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_search_u_literal_sequence(jsval_region_t *region,
		jsval_t this_value, const uint16_t *pattern, size_t pattern_len,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL || pattern == NULL
			|| pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_sequence_find_match(
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len, 0, &matched, &match_start,
			&match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

int
jsval_method_string_match_all_u_literal_sequence(jsval_region_t *region,
		jsval_t this_value, const uint16_t *pattern, size_t pattern_len,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t pattern_string;

	if (region == NULL || value_ptr == NULL || pattern == NULL
			|| pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_string_new_utf16(region, pattern, pattern_len,
			&pattern_string) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_SEQUENCE,
			pattern_string, input_string, 0, 0, value_ptr);
}

int
jsval_method_string_match_u_literal_sequence(jsval_region_t *region,
		jsval_t this_value, const uint16_t *pattern, size_t pattern_len,
		int global, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL || pattern == NULL
			|| pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_sequence_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				pattern, pattern_len, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_sequence_find_match(
					jsval_native_string_units(input_native),
					input_native->len, pattern, pattern_len, cursor,
					&matched, &match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(
				jsval_native_string_units(input_native), input_native->len,
				pattern, pattern_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_sequence_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_search_u_literal_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_class_find_match(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, 0, &matched, &match_start,
			&match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

int
jsval_method_string_search_u_literal_range_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *ranges, size_t range_count,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_range_class_find_match(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, 0, &matched, &match_start,
			&match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

int
jsval_method_string_search_u_literal_negated_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_class_find_match(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, 0, &matched, &match_start,
			&match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

int
jsval_method_string_search_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	int matched;
	size_t match_start;
	size_t match_end;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_range_class_find_match(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, 0, &matched, &match_start,
			&match_end) < 0) {
		return -1;
	}
	(void)match_end;
	*value_ptr = jsval_number(matched ? (double)match_start : -1.0);
	return 0;
}

int
jsval_method_string_match_all_u_literal_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t members_string;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_string_new_utf16(region, members, members_len,
			&members_string) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_CLASS,
			members_string, input_string, 0, 0, value_ptr);
}

int
jsval_method_string_match_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t ranges_string;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_u_literal_range_string_new(region, ranges, range_count,
			&ranges_string) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_RANGE_CLASS,
			ranges_string, input_string, 0, 0, value_ptr);
}

int
jsval_method_string_match_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t members_string;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_string_new_utf16(region, members, members_len,
			&members_string) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_CLASS,
			members_string, input_string, 0, 0, value_ptr);
}

int
jsval_method_string_match_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_t ranges_string;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_u_literal_range_string_new(region, ranges, range_count,
			&ranges_string) < 0) {
		return -1;
	}
	return jsval_match_iterator_new(region,
			JSVAL_MATCH_ITERATOR_MODE_U_LITERAL_NEGATED_RANGE_CLASS,
			ranges_string, input_string, 0, 0, value_ptr);
}

int
jsval_method_string_match_u_literal_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		int global, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_class_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				members, members_len, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len, members, members_len, cursor,
					&matched, &match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(
				jsval_native_string_units(input_native), input_native->len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_class_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_match_u_literal_range_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *ranges, size_t range_count,
		int global, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_range_class_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				ranges, range_count, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_range_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len, ranges, range_count, cursor,
					&matched, &match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(
				jsval_native_string_units(input_native), input_native->len,
				ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_class_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_match_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_negated_class_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				members, members_len, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_negated_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len, members, members_len, cursor,
					&matched, &match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(
				jsval_native_string_units(input_native), input_native->len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_class_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_match_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int global,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_negated_range_class_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				ranges, range_count, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_negated_range_class_find_match(
					jsval_native_string_units(input_native),
					input_native->len, ranges, range_count, cursor,
					&matched, &match_start, &match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(
				jsval_native_string_units(input_native), input_native->len,
				ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_class_exec_result_object(region,
				input_string, jsval_native_string_units(input_native),
				input_native->len, match_start, match_end, value_ptr);
	}
}

int
jsval_method_string_match_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int global,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (global) {
		size_t match_count;
		jsval_t array;
		size_t write_index = 0;
		size_t cursor = 0;

		if (jsval_u_literal_surrogate_count_matches(
				jsval_native_string_units(input_native), input_native->len,
				surrogate_unit, 1, &match_count) < 0) {
			return -1;
		}
		if (match_count == 0) {
			*value_ptr = jsval_null();
			return 0;
		}
		if (jsval_array_new(region, match_count, &array) < 0) {
			return -1;
		}
		for (;;) {
			int matched;
			size_t match_start;
			size_t match_end;
			jsval_t capture_value;

			if (jsval_u_literal_surrogate_find_match(
					jsval_native_string_units(input_native), input_native->len,
					surrogate_unit, cursor, &matched, &match_start,
					&match_end) < 0) {
				return -1;
			}
			if (!matched) {
				break;
			}
			if (jsval_string_new_utf16(region,
					jsval_native_string_units(input_native) + match_start,
					match_end - match_start, &capture_value) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, write_index++, capture_value) < 0) {
				return -1;
			}
			cursor = match_end;
		}
		*value_ptr = array;
		return 0;
	}

	{
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(
				jsval_native_string_units(input_native), input_native->len,
				surrogate_unit, 0, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			*value_ptr = jsval_null();
			return 0;
		}
		return jsval_u_literal_surrogate_exec_result_object(region, input_string,
				jsval_native_string_units(input_native), input_native->len,
				match_start, match_end, value_ptr);
	}
}

static int
jsval_method_string_replace_all_regex_check(jsval_region_t *region,
		jsval_t regexp_value, jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp;

	if (region == NULL) {
		errno = EINVAL;
		return -1;
	}
	regexp = jsval_native_regexp(region, regexp_value);
	if (regexp == NULL) {
		errno = EINVAL;
		return -1;
	}
	if ((regexp->flags & JSREGEX_FLAG_GLOBAL) == 0) {
		errno = EINVAL;
		if (error != NULL) {
			error->kind = JSMETHOD_ERROR_TYPE;
			error->message = "replaceAll requires a global RegExp";
		}
		return -1;
	}
	return 0;
}

static int
jsval_replace_append(jsstr16_t *out, size_t *offset_ptr,
		const uint16_t *segment, size_t segment_len, int build)
{
	if (offset_ptr == NULL || (segment_len > 0 && segment == NULL)
			|| (build && out == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (SIZE_MAX - *offset_ptr < segment_len) {
		errno = EOVERFLOW;
		return -1;
	}
	if (build) {
		if (*offset_ptr > out->cap || out->cap - *offset_ptr < segment_len) {
			errno = ENOBUFS;
			return -1;
		}
		if (segment_len > 0) {
			memcpy(out->codeunits + *offset_ptr, segment,
					segment_len * sizeof(out->codeunits[0]));
		}
		out->len = *offset_ptr + segment_len;
	}
	*offset_ptr += segment_len;
	return 0;
}

static int
jsval_replace_append_capture(jsstr16_t *out, size_t *offset_ptr,
		const uint16_t *subject, size_t subject_len, const size_t *offsets,
		size_t slot_count, size_t capture_index, int build)
{
	if (capture_index >= slot_count) {
		errno = EINVAL;
		return -1;
	}
	if (offsets[capture_index * 2] == SIZE_MAX) {
		return 0;
	}
	if (offsets[capture_index * 2] > offsets[capture_index * 2 + 1]
			|| offsets[capture_index * 2 + 1] > subject_len) {
		errno = EINVAL;
		return -1;
	}
	return jsval_replace_append(out, offset_ptr,
			subject + offsets[capture_index * 2],
			offsets[capture_index * 2 + 1] - offsets[capture_index * 2],
			build);
}

static int
jsval_replace_substitution(jsstr16_t *out, size_t *offset_ptr,
		const uint16_t *replacement, size_t replacement_len,
		const uint16_t *subject, size_t subject_len, size_t match_start,
		size_t match_end, const size_t *offsets, size_t slot_count,
		jsval_native_regexp_t *regexp, jsval_region_t *region, int build)
{
	size_t i = 0;

	if (offset_ptr == NULL || (replacement_len > 0 && replacement == NULL)
			|| (subject_len > 0 && subject == NULL) || match_start > match_end
			|| match_end > subject_len
			|| (regexp != NULL && region == NULL)) {
		errno = EINVAL;
		return -1;
	}
	while (i < replacement_len) {
		uint16_t cu = replacement[i];

		if (cu != '$' || i + 1 >= replacement_len) {
			if (jsval_replace_append(out, offset_ptr, replacement + i, 1,
					build) < 0) {
				return -1;
			}
			i++;
			continue;
		}

		switch (replacement[i + 1]) {
		case '$':
			if (jsval_replace_append(out, offset_ptr, replacement + i, 1,
					build) < 0) {
				return -1;
			}
			i += 2;
			break;
		case '&':
			if (jsval_replace_append(out, offset_ptr, subject + match_start,
					match_end - match_start, build) < 0) {
				return -1;
			}
			i += 2;
			break;
		case '`':
			if (jsval_replace_append(out, offset_ptr, subject, match_start,
					build) < 0) {
				return -1;
			}
			i += 2;
			break;
		case '\'':
			if (jsval_replace_append(out, offset_ptr, subject + match_end,
					subject_len - match_end, build) < 0) {
				return -1;
			}
			i += 2;
			break;
		default:
			if (regexp != NULL && replacement[i + 1] == '<') {
				size_t group_end = i + 2;

				while (group_end < replacement_len
						&& replacement[group_end] != '>') {
					group_end++;
				}
				if (group_end < replacement_len) {
					size_t capture_index;

					if (jsval_regexp_named_group_capture_index(region, regexp,
							replacement + i + 2,
							group_end - (i + 2),
							&capture_index) == 0) {
						if (jsval_replace_append_capture(out, offset_ptr,
								subject, subject_len, offsets,
								slot_count, capture_index, build) < 0) {
							return -1;
						}
						i = group_end + 1;
						break;
					}
					if (errno != ENOENT) {
						return -1;
					}
				}
			}
			if (replacement[i + 1] >= '1' && replacement[i + 1] <= '9') {
				size_t capture_index = (size_t)(replacement[i + 1] - '0');
				size_t consumed = 1;

				if (i + 2 < replacement_len && replacement[i + 2] >= '0'
						&& replacement[i + 2] <= '9') {
					size_t capture_two = capture_index * 10
							+ (size_t)(replacement[i + 2] - '0');

					if (capture_two < slot_count) {
						capture_index = capture_two;
						consumed = 2;
					} else if (capture_index >= slot_count) {
						capture_index = 0;
					}
				} else if (capture_index >= slot_count) {
					capture_index = 0;
				}

				if (capture_index != 0) {
					if (jsval_replace_append_capture(out, offset_ptr, subject,
							subject_len, offsets, slot_count, capture_index,
							build) < 0) {
						return -1;
					}
					i += 1 + consumed;
					break;
				}
			}
			if (jsval_replace_append(out, offset_ptr, replacement + i, 1,
					build) < 0) {
				return -1;
			}
			i++;
			break;
		}
	}
	return 0;
}

static int
jsval_regexp_replace_walk(jsval_region_t *region, jsval_t regexp_value,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *replacement, size_t replacement_len,
		int build_string, jsstr16_t *out, size_t *len_ptr,
		jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	size_t offsets_cap;
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;
	int global;

	if (regexp == NULL || len_ptr == NULL || (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	global = (regexp->flags & JSREGEX_FLAG_GLOBAL) != 0;
	if (global) {
		regexp->last_index = 0;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		for (;;) {
			if (jsval_regexp_exec_raw(region, regexp_value, subject, subject_len,
					offsets, offsets_cap, &exec_result, error) < 0) {
				return -1;
			}
			if (!exec_result.matched) {
				break;
			}

			matched_any = 1;
			if (jsval_replace_append(out, &offset, subject + cursor,
					exec_result.start - cursor, build_string) < 0) {
				return -1;
			}
			if (jsval_replace_substitution(out, &offset, replacement,
					replacement_len, subject, subject_len, exec_result.start,
					exec_result.end, offsets, exec_result.slot_count,
					regexp, region, build_string) < 0) {
				return -1;
			}
			cursor = exec_result.end;
			if (!global) {
				break;
			}
			if (exec_result.start == exec_result.end) {
				size_t next_index;

				if (jsregex_advance_string_index_utf16(subject, subject_len,
						regexp->last_index,
						(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
						&next_index) < 0) {
					return -1;
				}
				regexp->last_index = next_index;
			}
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset, subject, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset, subject + cursor,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_regexp_replace_count_parts(jsval_region_t *region, jsval_t regexp_value,
		const uint16_t *subject, size_t subject_len, size_t *count_ptr,
		jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	size_t offsets_cap;
	size_t count = 0;
	int global;

	if (regexp == NULL || count_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	global = (regexp->flags & JSREGEX_FLAG_GLOBAL) != 0;
	if (global) {
		regexp->last_index = 0;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		for (;;) {
			if (jsval_regexp_exec_raw(region, regexp_value, subject, subject_len,
					offsets, offsets_cap, &exec_result, error) < 0) {
				return -1;
			}
			if (!exec_result.matched) {
				break;
			}
			if (count == SIZE_MAX) {
				errno = EOVERFLOW;
				return -1;
			}
			count++;
			if (!global) {
				break;
			}
			if (exec_result.start == exec_result.end) {
				size_t next_index;

				if (jsregex_advance_string_index_utf16(subject, subject_len,
						regexp->last_index,
						(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
						&next_index) < 0) {
					return -1;
				}
				regexp->last_index = next_index;
			}
		}
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_regexp_replace_fill_parts(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	size_t offsets_cap;
	size_t total_len = 0;
	size_t cursor = 0;
	size_t write_index = 0;
	int global;

	if (regexp == NULL || callback == NULL || total_len_ptr == NULL
			|| (subject_len > 0 && subject == NULL)
			|| (part_count > 0 && parts == NULL)) {
		errno = EINVAL;
		return -1;
	}

	global = (regexp->flags & JSREGEX_FLAG_GLOBAL) != 0;
	if (global) {
		regexp->last_index = 0;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		for (;;) {
			jsval_replace_call_t call;
			jsval_t replacement_string;
			jsval_native_string_t *replacement_native;
			jsval_t capture_values[(size_t)regexp->capture_count + 1];
			size_t capture_count;

			if (jsval_regexp_exec_raw(region, regexp_value, subject, subject_len,
					offsets, offsets_cap, &exec_result, error) < 0) {
				return -1;
			}
			if (!exec_result.matched) {
				break;
			}
			if (write_index >= part_count) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_replace_append(NULL, &total_len,
					exec_result.start > cursor ? subject + cursor : NULL,
					exec_result.start - cursor, 0) < 0) {
				return -1;
			}
			if (jsval_regexp_capture_values(region, subject, offsets,
					exec_result.slot_count, capture_values) < 0) {
				return -1;
			}
			capture_count = exec_result.slot_count > 0
					? exec_result.slot_count - 1 : 0;
			call.match = capture_values[0];
			call.capture_count = capture_count;
			call.offset = exec_result.start;
			call.input = input_value;
			call.captures = capture_count > 0 ? capture_values + 1 : NULL;
			if (jsval_regexp_groups_object(region, regexp, capture_values,
					exec_result.slot_count, &call.groups) < 0) {
				return -1;
			}
			if (jsval_replace_callback_stringify(region, callback,
					callback_ctx, &call, &replacement_string, error) < 0) {
				return -1;
			}
			replacement_native = jsval_native_string(region, replacement_string);
			if (replacement_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			parts[write_index].match_start = exec_result.start;
			parts[write_index].match_end = exec_result.end;
			parts[write_index].replacement = replacement_string;
			write_index++;
			if (jsval_replace_append(NULL, &total_len,
					replacement_native->len > 0
						? jsval_native_string_units(replacement_native) : NULL,
					replacement_native->len,
					0) < 0) {
				return -1;
			}
			cursor = exec_result.end;
			if (!global) {
				break;
			}
			if (exec_result.start == exec_result.end) {
				size_t next_index;

				if (jsregex_advance_string_index_utf16(subject, subject_len,
						regexp->last_index,
						(regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
						&next_index) < 0) {
					return -1;
				}
				regexp->last_index = next_index;
			}
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_replace_append(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_regexp_split_walk(jsval_region_t *region, jsval_t regexp_value,
		const uint16_t *subject, size_t subject_len, size_t limit,
		int build_array, jsval_t array, size_t *count_ptr,
		jsmethod_error_t *error)
{
	jsval_native_regexp_t *regexp = jsval_native_regexp(region, regexp_value);
	size_t offsets_cap;
	size_t emitted = 0;

	if (regexp == NULL || count_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	offsets_cap = ((size_t)regexp->capture_count + 1) * 2;
	{
		size_t offsets[offsets_cap ? offsets_cap : 2];
		jsregex_exec_result_t exec_result;

		if (subject_len == 0) {
			regexp->last_index = 0;
			if (jsval_regexp_exec_raw(region, regexp_value, subject, subject_len,
					offsets, offsets_cap, &exec_result, error) < 0) {
				return -1;
			}
			if (!exec_result.matched) {
				if (build_array) {
					static const uint16_t empty_unit = 0;
					jsval_t piece;

					if (jsval_string_new_utf16(region, &empty_unit, 0, &piece) < 0) {
						return -1;
					}
					if (jsval_array_set(region, array, 0, piece) < 0) {
						return -1;
					}
				}
				*count_ptr = 1;
				return 0;
			}
			*count_ptr = 0;
			return 0;
		}

		{
			size_t p = 0;
			size_t q = 0;

			while (q < subject_len) {
				size_t e;
				size_t i;

				regexp->last_index = q;
				if (jsval_regexp_exec_raw(region, regexp_value, subject,
						subject_len, offsets, offsets_cap, &exec_result,
						error) < 0) {
					return -1;
				}
				if (!exec_result.matched) {
					q++;
					continue;
				}

				e = exec_result.end;
				if (e == p) {
					if (jsregex_advance_string_index_utf16(subject, subject_len,
							q, (regexp->flags & JSREGEX_FLAG_UNICODE) != 0,
							&q) < 0) {
						return -1;
					}
					continue;
				}

				if (emitted >= limit) {
					*count_ptr = emitted;
					return 0;
				}
				if (build_array) {
					jsval_t piece;
					static const uint16_t empty_unit = 0;

					if (jsval_string_new_utf16(region,
							exec_result.start > p ? subject + p : &empty_unit,
							exec_result.start - p, &piece) < 0) {
						return -1;
					}
					if (jsval_array_set(region, array, emitted, piece) < 0) {
						return -1;
					}
				}
				emitted++;

				for (i = 1; i < exec_result.slot_count && emitted < limit; i++) {
					if (build_array) {
						jsval_t capture = jsval_undefined();

						if (offsets[i * 2] != SIZE_MAX) {
							if (jsval_string_new_utf16(region,
									subject + offsets[i * 2],
									offsets[i * 2 + 1] - offsets[i * 2],
									&capture) < 0) {
								return -1;
							}
						}
						if (jsval_array_set(region, array, emitted, capture) < 0) {
							return -1;
						}
					}
					emitted++;
				}
				if (emitted >= limit) {
					*count_ptr = emitted;
					return 0;
				}

				p = e;
				q = p;
			}

			if (emitted < limit) {
				if (build_array) {
					jsval_t piece;
					static const uint16_t empty_unit = 0;

					if (jsval_string_new_utf16(region,
							subject_len > p ? subject + p : &empty_unit,
							subject_len - p, &piece) < 0) {
						return -1;
					}
					if (jsval_array_set(region, array, emitted, piece) < 0) {
						return -1;
					}
				}
				emitted++;
			}
		}
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_surrogate_replace_walk(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_surrogate_replace_count_parts(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, int replace_all,
		size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, 0, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_surrogate_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		uint16_t surrogate_unit, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_surrogate_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		uint16_t surrogate_unit, size_t limit, int build_array,
		jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_surrogate_find_match(subject, subject_len,
				surrogate_unit, q, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_sequence_replace_walk(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || pattern == NULL || pattern_len == 0
			|| (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_sequence_replace_count_parts(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		int replace_all, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || pattern == NULL || pattern_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_sequence_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| pattern == NULL || pattern_len == 0
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_sequence_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len,
		size_t limit, int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL || pattern == NULL
			|| pattern_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_sequence_find_match(subject, subject_len,
				pattern, pattern_len, q, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_predefined_class_replace_walk(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind,
				cursor, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_predefined_class_replace_count_parts(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		int replace_all, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind, 0,
				&matched, &match_start, &match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind,
				cursor, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_predefined_class_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		jsregex_u_predefined_class_kind_t kind, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind,
				cursor, &matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_predefined_class_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		jsregex_u_predefined_class_kind_t kind, size_t limit,
		int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_predefined_class_find_match(subject, subject_len, kind, q,
				&matched, &match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_class_replace_walk(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_class_replace_count_parts(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		int replace_all, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_class_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		const uint16_t *members, size_t members_len, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| members == NULL || members_len == 0
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_class_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *members, size_t members_len,
		size_t limit, int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL || members == NULL
			|| members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_class_find_match(subject, subject_len,
				members, members_len, q, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_range_class_replace_walk(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_range_class_replace_count_parts(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		int replace_all, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_range_class_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		const uint16_t *ranges, size_t range_count, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| ranges == NULL || range_count == 0
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_range_class_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *ranges, size_t range_count, size_t limit,
		int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL || ranges == NULL
			|| range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_range_class_find_match(subject, subject_len,
				ranges, range_count, q, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_negated_range_class_replace_walk(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, cursor, &matched,
				&match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_negated_range_class_replace_count_parts(
		const uint16_t *subject, size_t subject_len,
		const uint16_t *ranges, size_t range_count, int replace_all,
		size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || ranges == NULL || range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, cursor, &matched,
				&match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_negated_range_class_replace_fill_parts(
		jsval_region_t *region, jsval_t input_value,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *ranges, size_t range_count, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| ranges == NULL || range_count == 0
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, cursor, &matched,
				&match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_negated_range_class_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *ranges, size_t range_count, size_t limit,
		int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL || ranges == NULL
			|| range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_range_class_find_match(subject,
				subject_len, ranges, range_count, q, &matched,
				&match_start, &match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_u_literal_negated_class_replace_walk(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		const uint16_t *replacement, size_t replacement_len, int replace_all,
		int build_string, jsstr16_t *out, size_t *len_ptr)
{
	size_t offset = 0;
	size_t cursor = 0;
	int matched_any = 0;

	if (len_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)
			|| (replacement_len > 0 && replacement == NULL)
			|| (build_string && out == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		size_t offsets[2];

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}

		matched_any = 1;
		if (jsval_replace_append(out, &offset,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, build_string) < 0) {
			return -1;
		}
		offsets[0] = match_start;
		offsets[1] = match_end;
		if (jsval_replace_substitution(out, &offset, replacement,
				replacement_len, subject, subject_len, match_start,
				match_end, offsets, 1, NULL, NULL, build_string) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (!matched_any) {
		if (jsval_replace_append(out, &offset,
				subject_len > 0 ? subject : NULL, subject_len,
				build_string) < 0) {
			return -1;
		}
	} else if (jsval_replace_append(out, &offset,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, build_string) < 0) {
		return -1;
	}

	*len_ptr = offset;
	return 0;
}

static int
jsval_u_literal_negated_class_replace_count_parts(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		int replace_all, size_t *count_ptr)
{
	size_t count = 0;
	size_t cursor = 0;

	if (count_ptr == NULL || members == NULL || members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!replace_all) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, 0, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		*count_ptr = matched ? 1 : 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (count == SIZE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		count++;
		cursor = match_end;
	}

	*count_ptr = count;
	return 0;
}

static int
jsval_u_literal_negated_class_replace_fill_parts(jsval_region_t *region,
		jsval_t input_value, const uint16_t *subject, size_t subject_len,
		const uint16_t *members, size_t members_len, int replace_all,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_replace_part_t *parts, size_t part_count,
		size_t *total_len_ptr, jsmethod_error_t *error)
{
	size_t write_index = 0;
	size_t total_len = 0;
	size_t cursor = 0;

	if (region == NULL || callback == NULL || total_len_ptr == NULL
			|| members == NULL || members_len == 0
			|| (part_count > 0 && parts == NULL)
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;
		jsval_replace_call_t call;
		jsval_t match_value;
		jsval_t replacement_string;
		jsval_native_string_t *replacement_native;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, cursor, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (write_index >= part_count) {
			errno = EINVAL;
			return -1;
		}
		if (jsval_append_segment(NULL, &total_len,
				match_start > cursor ? subject + cursor : NULL,
				match_start - cursor, 0) < 0) {
			return -1;
		}

		call.capture_count = 0;
		call.captures = NULL;
		call.offset = match_start;
		call.input = input_value;
		call.groups = jsval_undefined();
		if (jsval_string_new_utf16(region, subject + match_start,
				match_end - match_start, &match_value) < 0) {
			return -1;
		}
		call.match = match_value;
		if (jsval_replace_callback_stringify(region, callback, callback_ctx,
				&call, &replacement_string, error) < 0) {
			return -1;
		}
		replacement_native = jsval_native_string(region, replacement_string);
		if (replacement_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		parts[write_index].match_start = match_start;
		parts[write_index].match_end = match_end;
		parts[write_index].replacement = replacement_string;
		write_index++;
		if (jsval_append_segment(NULL, &total_len,
				replacement_native->len > 0
					? jsval_native_string_units(replacement_native) : NULL,
				replacement_native->len, 0) < 0) {
			return -1;
		}
		cursor = match_end;
		if (!replace_all) {
			break;
		}
	}

	if (write_index != part_count) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_append_segment(NULL, &total_len,
			subject_len > cursor ? subject + cursor : NULL,
			subject_len - cursor, 0) < 0) {
		return -1;
	}
	*total_len_ptr = total_len;
	return 0;
}

static int
jsval_u_literal_negated_class_split_walk(jsval_region_t *region,
		const uint16_t *subject, size_t subject_len,
		const uint16_t *members, size_t members_len,
		size_t limit, int build_array, jsval_t array, size_t *count_ptr)
{
	static const uint16_t empty_unit = 0;
	size_t emitted = 0;
	size_t p = 0;
	size_t q = 0;

	if (region == NULL || count_ptr == NULL || members == NULL
			|| members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (limit == 0) {
		*count_ptr = 0;
		return 0;
	}

	for (;;) {
		int matched;
		size_t match_start;
		size_t match_end;

		if (jsval_u_literal_negated_class_find_match(subject, subject_len,
				members, members_len, q, &matched, &match_start,
				&match_end) < 0) {
			return -1;
		}
		if (!matched) {
			break;
		}
		if (emitted >= limit) {
			*count_ptr = emitted;
			return 0;
		}
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					match_start > p ? subject + p : &empty_unit,
					match_start - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
		p = match_end;
		q = p;
	}

	if (emitted < limit) {
		if (build_array) {
			jsval_t piece;

			if (jsval_string_new_utf16(region,
					subject_len > p ? subject + p : &empty_unit,
					subject_len - p, &piece) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, emitted, piece) < 0) {
				return -1;
			}
		}
		emitted++;
	}

	*count_ptr = emitted;
	return 0;
}

static int
jsval_method_string_replace_u_literal_surrogate_common(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_surrogate_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_surrogate_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_surrogate_fn_common(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_surrogate_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_surrogate_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, replace_all, callback, callback_ctx, parts,
			part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_surrogate_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, limit, 0, jsval_undefined(), &part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_surrogate_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			surrogate_unit, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_literal_sequence_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL || pattern == NULL
			|| pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_sequence_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_sequence_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_sequence_fn_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL
			|| pattern == NULL || pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_sequence_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_sequence_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len, replace_all, callback, callback_ctx, parts,
			part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_sequence(jsval_region_t *region,
		jsval_t this_value, const uint16_t *pattern, size_t pattern_len,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL || pattern == NULL
			|| pattern_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_sequence_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len, limit, 0, jsval_undefined(),
			&part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_sequence_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			pattern, pattern_len, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_predefined_class_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_predefined_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len, kind,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_predefined_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len, kind,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_predefined_class_fn_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_predefined_class_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len, kind,
			replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_predefined_class_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len, kind,
			replace_all, callback, callback_ctx, parts, part_count,
			&total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

static int
jsval_method_string_split_u_predefined_class_common(
		jsval_region_t *region, jsval_t this_value,
		jsregex_u_predefined_class_kind_t kind, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_predefined_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len, kind,
			limit, 0, jsval_undefined(), &part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_predefined_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len, kind,
			limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_literal_class_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_class_fn_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL
			|| members == NULL || members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_class_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_class_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, replace_all, callback, callback_ctx, parts,
			part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, limit, 0, jsval_undefined(),
			&part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_literal_range_class_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_range_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_range_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_range_class_fn_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL
			|| ranges == NULL || range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_range_class_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_range_class_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, replace_all, callback, callback_ctx, parts,
			part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_range_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *ranges, size_t range_count,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_range_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, limit, 0, jsval_undefined(),
			&part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_range_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_literal_negated_class_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_negated_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_negated_class_fn_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL
			|| members == NULL || members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_class_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_negated_class_replace_fill_parts(region, input_string,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, replace_all, callback, callback_ctx, parts,
			part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_negated_class(jsval_region_t *region,
		jsval_t this_value, const uint16_t *members, size_t members_len,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL || members == NULL
			|| members_len == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_negated_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, limit, 0, jsval_undefined(),
			&part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_negated_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			members, members_len, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_u_literal_negated_range_class_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, int replace_all, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_range_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 0, NULL, &result_len) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_u_literal_negated_range_class_replace_walk(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count,
			jsval_native_string_units(replacement_native),
			replacement_native->len, replace_all, 1, &out, &result_len) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_u_literal_negated_range_class_fn_common(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL
			|| ranges == NULL || range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_u_literal_negated_range_class_replace_count_parts(
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, replace_all, &part_count) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_u_literal_negated_range_class_replace_fill_parts(region,
			input_string, jsval_native_string_units(input_native),
			input_native->len, ranges, range_count, replace_all, callback,
			callback_ctx, parts, part_count, &total_len, error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}

int
jsval_method_string_split_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL || ranges == NULL
			|| range_count == 0) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_u_literal_negated_range_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, limit, 0, jsval_undefined(),
			&part_count) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_u_literal_negated_range_class_split_walk(region,
			jsval_native_string_units(input_native), input_native->len,
			ranges, range_count, limit, 1, array, &part_count) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_split_regex(jsval_region_t *region, jsval_t this_value,
		jsval_t regexp_value, int have_limit, jsval_t limit_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t split_regex;
	jsval_t array;
	size_t limit;
	size_t part_count;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_split_limit(region, have_limit, limit_value, &limit) < 0) {
		return -1;
	}
	if (jsval_regexp_clone_for_split(region, regexp_value, &split_regex,
			error) < 0) {
		return -1;
	}
	if (jsval_regexp_split_walk(region, split_regex,
			jsval_native_string_units(input_native), input_native->len,
			limit, 0, jsval_undefined(), &part_count, error) < 0) {
		return -1;
	}
	if (jsval_array_new(region, part_count, &array) < 0) {
		return -1;
	}
	if (jsval_regexp_split_walk(region, split_regex,
			jsval_native_string_units(input_native), input_native->len,
			limit, 1, array, &part_count, error) < 0) {
		return -1;
	}
	*value_ptr = array;
	return 0;
}

static int
jsval_method_string_replace_regex_measure_bridge(jsval_region_t *region,
		jsval_t this_value, jsval_t regexp_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t replace_regex;
	size_t result_len = 0;

	if (region == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	sizes->result_len = 0;
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_clone_for_replace(region, regexp_value, &replace_regex,
			error) < 0) {
		return -1;
	}
	if (jsval_regexp_replace_walk(region, replace_regex,
			jsval_native_string_units(input_native), input_native->len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, 0, NULL, &result_len, error) < 0) {
		return -1;
	}
	sizes->result_len = result_len;
	return 0;
}

static int
jsval_method_string_replace_regex(jsval_region_t *region, jsval_t this_value,
		jsval_t regexp_value, jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	jsmethod_string_replace_sizes_t sizes;
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t replacement_string;
	jsval_native_string_t *replacement_native;
	jsval_t replace_regex;
	jsval_t result;
	jsval_native_string_t *result_string;
	jsstr16_t out;
	size_t result_len = 0;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_method_string_replace_regex_measure_bridge(region, this_value,
			regexp_value, replacement_value, &sizes, error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, replacement_value, 0,
			&replacement_string, error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	replacement_native = jsval_native_string(region, replacement_string);
	if (input_native == NULL || replacement_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_clone_for_replace(region, regexp_value, &replace_regex,
			error) < 0) {
		return -1;
	}
	if (jsval_string_reserve_utf16(region, sizes.result_len, &result,
			&result_string) < 0) {
		return -1;
	}
	jsstr16_init_from_buf(&out,
			(const char *)jsval_native_string_units(result_string),
			result_string->cap * sizeof(uint16_t));
	if (jsval_regexp_replace_walk(region, replace_regex,
			jsval_native_string_units(input_native), input_native->len,
			jsval_native_string_units(replacement_native),
			replacement_native->len, 1, &out, &result_len, error) < 0) {
		return -1;
	}
	result_string->len = result_len;
	*value_ptr = result;
	return 0;
}

static int
jsval_method_string_replace_regex_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t regexp_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		int replace_all, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t input_string;
	jsval_native_string_t *input_native;
	jsval_t count_regex;
	jsval_t fill_regex;
	jsval_replace_part_t *parts = NULL;
	size_t part_count = 0;
	size_t total_len = 0;

	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsmethod_error_clear(error);
	if (replace_all
			&& jsval_method_string_replace_all_regex_check(region, regexp_value,
			error) < 0) {
		return -1;
	}
	if (jsval_stringify_value_to_native(region, this_value, 1, &input_string,
			error) < 0) {
		return -1;
	}
	input_native = jsval_native_string(region, input_string);
	if (input_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_regexp_clone_for_replace(region, regexp_value, &count_regex,
			error) < 0) {
		return -1;
	}
	if (jsval_regexp_replace_count_parts(region, count_regex,
			jsval_native_string_units(input_native), input_native->len,
			&part_count, error) < 0) {
		return -1;
	}
	if (part_count > 0) {
		if (jsval_region_reserve(region, part_count * sizeof(*parts),
				JSVAL_ALIGN, NULL, (void **)&parts) < 0) {
			return -1;
		}
	}
	if (jsval_regexp_clone_for_replace(region, regexp_value, &fill_regex,
			error) < 0) {
		return -1;
	}
	if (jsval_regexp_replace_fill_parts(region, fill_regex, input_string,
			jsval_native_string_units(input_native), input_native->len,
			callback, callback_ctx, parts, part_count, &total_len,
			error) < 0) {
		return -1;
	}
	return jsval_replace_build_parts(region,
			jsval_native_string_units(input_native), input_native->len,
			parts, part_count, total_len, value_ptr);
}
#endif

int jsval_json_parse(jsval_region_t *region, const uint8_t *json, size_t len, unsigned int token_cap, jsval_t *value_ptr)
{
	jsval_json_doc_t *doc;
	jsval_off_t doc_off;
	jsval_off_t json_off;
	jsval_off_t tokens_off;
	uint8_t *json_copy;
	jsmntok_t *tokens;
	jsmn_parser parser;
	int rc;

	if (token_cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_reserve(region, sizeof(*doc), JSVAL_ALIGN, &doc_off, (void **)&doc) < 0) {
		return -1;
	}
	if (jsval_region_reserve(region, len + 1, 1, &json_off, (void **)&json_copy) < 0) {
		return -1;
	}
	if (jsval_region_reserve(region, token_cap * sizeof(jsmntok_t), JSVAL_ALIGN, &tokens_off, (void **)&tokens) < 0) {
		return -1;
	}

	memcpy(json_copy, json, len);
	json_copy[len] = '\0';
	memset(tokens, 0, token_cap * sizeof(jsmntok_t));

	jsmn_init(&parser);
	rc = jsmn_parse(&parser, (const char *)json_copy, len, tokens, token_cap);
	if (rc < 0) {
		return rc;
	}

	doc->json_off = json_off;
	doc->json_len = len;
	doc->tokens_off = tokens_off;
	doc->tokcap = token_cap;
	doc->tokused = parser.toknext;
	doc->root_i = 0;

	*value_ptr = jsval_undefined();
	value_ptr->repr = JSVAL_REPR_JSON;
	value_ptr->off = doc_off;
	value_ptr->as.index = 0;
	value_ptr->kind = jsval_json_token_kind(region, doc, 0);
	jsval_region_set_root(region, *value_ptr);
	return 0;
}

int jsval_copy_json(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	jsval_json_emit_state_t state;

	memset(&state, 0, sizeof(state));
	state.buf = buf;
	state.cap = cap;
	if (jsval_json_emit_value(region, value, &state) < 0) {
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = state.len;
	}
	return 0;
}

int jsval_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr)
{
	if (value.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}

	if (value.repr == JSVAL_REPR_NATIVE) {
		return jsval_native_string_copy_utf8(region, value, buf, cap, len_ptr);
	}
	if (value.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, value);
		return jsval_json_string_copy_utf8_internal(region, doc, value.as.index, buf, cap, len_ptr);
	}

	errno = EINVAL;
	return -1;
}

size_t jsval_object_size(jsval_region_t *region, jsval_t object)
{
	if (object.kind != JSVAL_KIND_OBJECT) {
		return 0;
	}
	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		return native == NULL ? 0 : native->len;
	}
	if (object.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
		return tokens[object.as.index].size;
	}
	return 0;
}

size_t jsval_array_length(jsval_region_t *region, jsval_t array)
{
	if (array.kind != JSVAL_KIND_ARRAY) {
		return 0;
	}
	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);
		return native == NULL ? 0 : native->len;
	}
	if (array.repr == JSVAL_REPR_JSON) {
		jsval_json_doc_t *doc = jsval_json_doc(region, array);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);
		return tokens[array.as.index].size;
	}
	return 0;
}

static int jsval_native_object_find_utf8(jsval_region_t *region,
		jsval_native_object_t *native, const uint8_t *key, size_t key_len,
		size_t *index_ptr)
{
	size_t i;
	jsval_native_prop_t *props;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	props = jsval_native_object_props(native);
	for (i = 0; i < native->len; i++) {
		jsval_t name = jsval_undefined();

		name.kind = JSVAL_KIND_STRING;
		name.repr = JSVAL_REPR_NATIVE;
		name.off = props[i].name_off;
		if (jsval_native_string_eq_utf8(region, name, key, key_len)) {
			if (index_ptr != NULL) {
				*index_ptr = i;
			}
			return 1;
		}
	}

	return 0;
}

static int
jsval_native_object_find_key(jsval_region_t *region, jsval_native_object_t *native,
		jsval_t key, size_t *index_ptr)
{
	size_t i;
	jsval_native_prop_t *props;

	if (native == NULL || key.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}

	props = jsval_native_object_props(native);
	for (i = 0; i < native->len; i++) {
		jsval_t name = jsval_undefined();

		name.kind = JSVAL_KIND_STRING;
		name.repr = JSVAL_REPR_NATIVE;
		name.off = props[i].name_off;
		if (jsval_strict_eq(region, name, key) == 1) {
			if (index_ptr != NULL) {
				*index_ptr = i;
			}
			return 1;
		}
	}

	return 0;
}

static int
jsval_object_copy_find_planned_key(jsval_region_t *region,
		jsval_object_copy_action_t *actions, size_t action_len, jsval_t key,
		size_t *index_ptr)
{
	size_t i;

	if (key.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < action_len; i++) {
		if (!actions[i].append) {
			continue;
		}
		if (jsval_strict_eq(region, actions[i].key, key) == 1) {
			if (index_ptr != NULL) {
				*index_ptr = actions[i].index;
			}
			return 1;
		}
	}

	return 0;
}

int jsval_object_has_own_utf8(jsval_region_t *region, jsval_t object,
		const uint8_t *key, size_t key_len, int *has_ptr)
{
	if (has_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		int found = jsval_native_object_find_utf8(region, native, key, key_len, NULL);

		if (found < 0) {
			return -1;
		}
		*has_ptr = found;
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);

			if (value_index < 0) {
				break;
			}
			if (jsval_json_string_eq_utf8(region, doc, key_index, key, key_len)) {
				*has_ptr = 1;
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*has_ptr = 0;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_object_get_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t *value_ptr)
{
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		jsval_native_prop_t *props;
		size_t index;
		int found;

		found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
		if (found < 0) {
			return -1;
		}
		props = jsval_native_object_props(native);
		if (found) {
			*value_ptr = props[index].value;
			return 0;
		}

		*value_ptr = jsval_undefined();
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			if (value_index < 0) {
				break;
			}
			if (jsval_json_string_eq_utf8(region, doc, key_index, key, key_len)) {
				*value_ptr = jsval_json_value(region, object, (uint32_t)value_index);
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*value_ptr = jsval_undefined();
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_object_key_at(jsval_region_t *region, jsval_t object, size_t index,
		jsval_t *key_ptr)
{
	if (key_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		jsval_native_prop_t *props;
		jsval_t key = jsval_undefined();

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (index >= native->len) {
			*key_ptr = jsval_undefined();
			return 0;
		}
		props = jsval_native_object_props(native);
		key.kind = JSVAL_KIND_STRING;
		key.repr = JSVAL_REPR_NATIVE;
		key.off = props[index].name_off;
		*key_ptr = key;
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);

			if (value_index < 0) {
				break;
			}
			if ((size_t)i == index) {
				*key_ptr = jsval_json_value(region, object,
						(uint32_t)key_index);
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*key_ptr = jsval_undefined();
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_object_value_at(jsval_region_t *region, jsval_t object, size_t index,
		jsval_t *value_ptr)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);
		jsval_native_prop_t *props;

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (index >= native->len) {
			*value_ptr = jsval_undefined();
			return 0;
		}
		props = jsval_native_object_props(native);
		*value_ptr = props[index].value;
		return 0;
	}

	if (object.repr == JSVAL_REPR_JSON) {
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);

			if (value_index < 0) {
				break;
			}
			if ((size_t)i == index) {
				*value_ptr = jsval_json_value(region, object,
						(uint32_t)value_index);
				return 0;
			}
			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				break;
			}
		}

		*value_ptr = jsval_undefined();
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int
jsval_object_copy_own(jsval_region_t *region, jsval_t dst, jsval_t src)
{
	jsval_native_object_t *dst_native;
	jsval_native_prop_t *dst_props;
	jsval_object_copy_action_t *actions = NULL;
	size_t dst_len;
	size_t src_len;
	size_t append_count = 0;
	size_t i;

	if (region == NULL || dst.kind != JSVAL_KIND_OBJECT
			|| src.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (dst.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}
	if (src.repr != JSVAL_REPR_NATIVE && src.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	dst_native = jsval_native_object(region, dst);
	if (dst_native == NULL) {
		errno = EINVAL;
		return -1;
	}

	dst_len = dst_native->len;
	src_len = jsval_object_size(region, src);
	if (src_len > 0) {
		if (jsval_region_reserve(region, src_len * sizeof(*actions),
				JSVAL_ALIGN, NULL, (void **)&actions) < 0) {
			return -1;
		}
	}

	for (i = 0; i < src_len; i++) {
		jsval_t key;
		jsval_t value;
		size_t index;
		int found;

		if (jsval_object_key_at(region, src, i, &key) < 0
				|| jsval_object_value_at(region, src, i, &value) < 0) {
			return -1;
		}
		if (key.kind != JSVAL_KIND_STRING) {
			errno = EINVAL;
			return -1;
		}

		found = jsval_native_object_find_key(region, dst_native, key, &index);
		if (found < 0) {
			return -1;
		}
		if (!found) {
			found = jsval_object_copy_find_planned_key(region, actions, i, key,
					&index);
			if (found < 0) {
				return -1;
			}
		}
		if (!found) {
			index = dst_len + append_count++;
			actions[i].append = 1;
		} else {
			actions[i].append = 0;
		}
		actions[i].key = key;
		actions[i].value = value;
		actions[i].index = index;
		actions[i].name_off = 0;
	}

	if (dst_len + append_count > dst_native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	for (i = 0; i < src_len; i++) {
		if (!actions[i].append) {
			continue;
		}
		if (actions[i].key.repr == JSVAL_REPR_NATIVE) {
			actions[i].name_off = actions[i].key.off;
			continue;
		}

		{
			size_t key_len = 0;
			jsval_t name;

			if (jsval_string_copy_utf8(region, actions[i].key, NULL, 0,
					&key_len) < 0) {
				return -1;
			}
			{
				uint8_t key_buf[key_len ? key_len : 1];

				if (key_len > 0 && jsval_string_copy_utf8(region,
						actions[i].key, key_buf, key_len, NULL) < 0) {
					return -1;
				}
				if (jsval_string_new_utf8(region, key_buf, key_len, &name) < 0) {
					return -1;
				}
			}
			if (name.kind != JSVAL_KIND_STRING || name.repr != JSVAL_REPR_NATIVE) {
				errno = EINVAL;
				return -1;
			}
			actions[i].name_off = name.off;
		}
	}

	dst_props = jsval_native_object_props(dst_native);
	for (i = 0; i < src_len; i++) {
		if (actions[i].append) {
			dst_props[actions[i].index].name_off = actions[i].name_off;
		}
		dst_props[actions[i].index].value = actions[i].value;
	}
	dst_native->len = dst_len + append_count;
	return 0;
}

int
jsval_object_clone_own(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr)
{
	jsval_t clone;
	size_t src_len;

	if (region == NULL || value_ptr == NULL || src.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (src.repr != JSVAL_REPR_NATIVE && src.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	src_len = jsval_object_size(region, src);
	if (capacity < src_len) {
		errno = ENOBUFS;
		return -1;
	}

	if (jsval_object_new(region, capacity, &clone) < 0) {
		return -1;
	}
	if (jsval_object_copy_own(region, clone, src) < 0) {
		return -1;
	}

	*value_ptr = clone;
	return 0;
}

int jsval_object_set_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t value)
{
	size_t index;
	jsval_native_object_t *native;
	jsval_native_prop_t *props;
	int found;

	if (object.kind != JSVAL_KIND_OBJECT || object.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_object(region, object);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	props = jsval_native_object_props(native);
	found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
	if (found < 0) {
		return -1;
	}
	if (found) {
		props[index].value = value;
		return 0;
	}

	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	{
		jsval_t name;
		if (jsval_string_new_utf8(region, key, key_len, &name) < 0) {
			return -1;
		}
		props[native->len].name_off = name.off;
		props[native->len].value = value;
		native->len++;
	}

	return 0;
}

int jsval_object_delete_utf8(jsval_region_t *region, jsval_t object,
		const uint8_t *key, size_t key_len, int *deleted_ptr)
{
	jsval_native_object_t *native;
	jsval_native_prop_t *props;
	size_t index;
	size_t i;
	int found;

	if (deleted_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (object.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_object(region, object);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	found = jsval_native_object_find_utf8(region, native, key, key_len, &index);
	if (found < 0) {
		return -1;
	}
	if (!found) {
		*deleted_ptr = 0;
		return 0;
	}

	props = jsval_native_object_props(native);
	for (i = index + 1; i < native->len; i++) {
		props[i - 1] = props[i];
	}
	props[native->len - 1].name_off = 0;
	props[native->len - 1].value = jsval_undefined();
	native->len--;
	*deleted_ptr = 1;
	return 0;
}

int jsval_array_clone_dense(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr)
{
	jsval_t out;
	size_t len;
	size_t i;

	if (region == NULL || value_ptr == NULL || src.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (src.repr != JSVAL_REPR_NATIVE && src.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_array_length(region, src);
	if (capacity < len) {
		errno = ENOBUFS;
		return -1;
	}

	if (jsval_array_new(region, capacity, &out) < 0) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		jsval_t child;

		if (jsval_array_get(region, src, i, &child) < 0) {
			return -1;
		}
		if (jsval_array_set(region, out, i, child) < 0) {
			return -1;
		}
	}

	*value_ptr = out;
	return 0;
}

int jsval_array_get(jsval_region_t *region, jsval_t array, size_t index, jsval_t *value_ptr)
{
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);
		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (index >= native->len) {
			*value_ptr = jsval_undefined();
			return 0;
		}
		*value_ptr = jsval_native_array_values(native)[index];
		return 0;
	}

	if (array.repr == JSVAL_REPR_JSON) {
		int cursor;
		size_t i;
		jsval_json_doc_t *doc = jsval_json_doc(region, array);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (index >= (size_t)tokens[array.as.index].size) {
			*value_ptr = jsval_undefined();
			return 0;
		}

		cursor = (int)array.as.index + 1;
		for (i = 0; i < index; i++) {
			cursor = jsval_json_next(region, doc, cursor);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = jsval_json_value(region, array, (uint32_t)cursor);
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_array_set(jsval_region_t *region, jsval_t array, size_t index, jsval_t value)
{
	size_t i;
	jsval_native_array_t *native;
	jsval_t *values;

	if (array.kind != JSVAL_KIND_ARRAY || array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (index >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	values = jsval_native_array_values(native);
	for (i = native->len; i < index; i++) {
		values[i] = jsval_undefined();
	}
	values[index] = value;
	if (index >= native->len) {
		native->len = index + 1;
	}
	return 0;
}

int jsval_array_push(jsval_region_t *region, jsval_t array, jsval_t value)
{
	size_t len;

	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	len = jsval_array_length(region, array);
	return jsval_array_set(region, array, len, value);
}

int jsval_array_splice_dense(jsval_region_t *region, jsval_t array, size_t start,
		size_t delete_count, const jsval_t *insert_values, size_t insert_count,
		jsval_t *removed_ptr)
{
	jsval_native_array_t *native;
	jsval_t *values;
	jsval_t removed;
	size_t len;
	size_t effective_start;
	size_t effective_delete_count;
	size_t suffix_count;
	size_t new_len;
	size_t i;
	jsval_t inserts[insert_count ? insert_count : 1];

	if (region == NULL || removed_ptr == NULL || array.kind != JSVAL_KIND_ARRAY
			|| (insert_count > 0 && insert_values == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (insert_count > 0) {
		memcpy(inserts, insert_values, insert_count * sizeof(*inserts));
	}

	len = native->len;
	effective_start = start > len ? len : start;
	effective_delete_count = delete_count;
	if (effective_delete_count > len - effective_start) {
		effective_delete_count = len - effective_start;
	}
	if (insert_count > SIZE_MAX - (len - effective_delete_count)) {
		errno = ENOBUFS;
		return -1;
	}
	new_len = len - effective_delete_count + insert_count;
	if (new_len > native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (jsval_array_new(region, effective_delete_count, &removed) < 0) {
		return -1;
	}

	values = jsval_native_array_values(native);
	for (i = 0; i < effective_delete_count; i++) {
		if (jsval_array_set(region, removed, i,
				values[effective_start + i]) < 0) {
			return -1;
		}
	}

	suffix_count = len - effective_start - effective_delete_count;
	if (insert_count != effective_delete_count && suffix_count > 0) {
		memmove(values + effective_start + insert_count,
				values + effective_start + effective_delete_count,
				suffix_count * sizeof(*values));
	}
	for (i = 0; i < insert_count; i++) {
		values[effective_start + i] = inserts[i];
	}
	if (new_len < len) {
		for (i = new_len; i < len; i++) {
			values[i] = jsval_undefined();
		}
	}

	native->len = new_len;
	*removed_ptr = removed;
	return 0;
}

int jsval_array_pop(jsval_region_t *region, jsval_t array, jsval_t *value_ptr)
{
	jsval_native_array_t *native;
	jsval_t *values;
	size_t index;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->len == 0) {
		*value_ptr = jsval_undefined();
		return 0;
	}

	index = native->len - 1;
	values = jsval_native_array_values(native);
	*value_ptr = values[index];
	values[index] = jsval_undefined();
	native->len = index;
	return 0;
}

int jsval_array_shift(jsval_region_t *region, jsval_t array, jsval_t *value_ptr)
{
	jsval_native_array_t *native;
	jsval_t *values;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->len == 0) {
		*value_ptr = jsval_undefined();
		return 0;
	}

	values = jsval_native_array_values(native);
	*value_ptr = values[0];
	if (native->len > 1) {
		memmove(values, values + 1, (native->len - 1) * sizeof(*values));
	}
	native->len--;
	values[native->len] = jsval_undefined();
	return 0;
}

int jsval_array_unshift(jsval_region_t *region, jsval_t array, jsval_t value)
{
	jsval_native_array_t *native;
	jsval_t *values;

	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->len >= native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	values = jsval_native_array_values(native);
	if (native->len > 0) {
		memmove(values + 1, values, native->len * sizeof(*values));
	}
	values[0] = value;
	native->len++;
	return 0;
}

int jsval_array_set_length(jsval_region_t *region, jsval_t array, size_t new_len)
{
	jsval_native_array_t *native;
	jsval_t *values;
	size_t i;

	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}
	if (array.repr != JSVAL_REPR_NATIVE) {
		errno = ENOTSUP;
		return -1;
	}

	native = jsval_native_array(region, array);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (new_len > native->cap) {
		errno = ENOBUFS;
		return -1;
	}

	values = jsval_native_array_values(native);
	if (new_len < native->len) {
		for (i = new_len; i < native->len; i++) {
			values[i] = jsval_undefined();
		}
	} else if (new_len > native->len) {
		for (i = native->len; i < new_len; i++) {
			values[i] = jsval_undefined();
		}
	}
	native->len = new_len;
	return 0;
}

typedef int (*jsval_url_mutator_fn)(jsurl_t *url, void *opaque);
typedef int (*jsval_url_search_params_mutator_fn)(jsurl_search_params_t *params,
		void *opaque);

typedef struct jsval_url_string_ctx_s {
	jsstr8_t value;
} jsval_url_string_ctx_t;

typedef struct jsval_url_search_params_pair_ctx_s {
	jsstr8_t name;
	jsstr8_t value;
} jsval_url_search_params_pair_ctx_t;

typedef struct jsval_url_new_with_base_ctx_s {
	jsval_region_t *region;
	jsstr8_t input;
	jsval_t *value_ptr;
} jsval_url_new_with_base_ctx_t;

static int jsval_url_jsstr_make_string(jsval_region_t *region, jsstr8_t value,
		jsval_t *value_ptr)
{
	static const uint8_t empty[1] = {0};

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_string_new_utf8(region, value.len > 0 ? value.bytes : empty,
			value.len, value_ptr);
}

static jsval_t jsval_native_make_value(jsval_region_t *region, void *ptr,
		jsval_kind_t kind)
{
	jsval_t value = jsval_undefined();

	if (region == NULL || ptr == NULL) {
		return value;
	}
	value.kind = kind;
	value.repr = JSVAL_REPR_NATIVE;
	value.off = (jsval_off_t)((uint8_t *)ptr - region->base);
	return value;
}

static int jsval_url_attach_search_params(jsval_region_t *region,
		jsval_t url_value, jsval_native_url_t *native, jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *params_native;
	jsval_t params_value;
	void *ptr = NULL;

	if (region == NULL || native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->search_params_value.kind == JSVAL_KIND_URL_SEARCH_PARAMS
			&& native->search_params_value.repr == JSVAL_REPR_NATIVE) {
		if (value_ptr != NULL) {
			*value_ptr = native->search_params_value;
		}
		return 0;
	}
	if (jsval_region_alloc(region, sizeof(*params_native),
			_Alignof(jsval_native_url_search_params_t), &ptr) < 0) {
		return -1;
	}
	params_native = (jsval_native_url_search_params_t *)ptr;
	memset(params_native, 0, sizeof(*params_native));
	params_native->params_cap = native->search_params_cap;
	params_native->owner_off = url_value.off;
	params_native->attached = 1;
	params_value = jsval_native_make_value(region, params_native,
			JSVAL_KIND_URL_SEARCH_PARAMS);
	native->search_params_value = params_value;
	if (value_ptr != NULL) {
		*value_ptr = params_value;
	}
	return 0;
}

static int jsval_url_alloc_native(jsval_region_t *region,
		const jsurl_sizes_t *caps, jsval_t *value_ptr,
		jsval_native_url_t **native_ptr)
{
	jsval_native_url_t *native;
	void *ptr = NULL;
	jsval_t value;

	if (region == NULL || caps == NULL || value_ptr == NULL
			|| native_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_alloc(region, sizeof(*native),
			_Alignof(jsval_native_url_t), &ptr) < 0) {
		return -1;
	}
	native = (jsval_native_url_t *)ptr;
	memset(native, 0, sizeof(*native));
	if (jsval_url_field_alloc(region, caps->protocol_cap, &native->protocol) < 0
			|| jsval_url_field_alloc(region, caps->username_cap,
				&native->username) < 0
			|| jsval_url_field_alloc(region, caps->password_cap,
				&native->password) < 0
			|| jsval_url_field_alloc(region, caps->hostname_cap,
				&native->hostname) < 0
			|| jsval_url_field_alloc(region, caps->port_cap, &native->port) < 0
			|| jsval_url_field_alloc(region, caps->pathname_cap,
				&native->pathname) < 0
			|| jsval_url_field_alloc(region, caps->search_cap,
				&native->search) < 0
			|| jsval_url_field_alloc(region, caps->hash_cap, &native->hash) < 0
			|| jsval_url_field_alloc(region, caps->host_cap, &native->host) < 0
			|| jsval_url_field_alloc(region, caps->origin_cap,
				&native->origin) < 0
			|| jsval_url_field_alloc(region, caps->href_cap, &native->href) < 0) {
		return -1;
	}
	native->search_params_cap = caps->search_params_cap;
	native->search_params_value = jsval_undefined();
	value = jsval_native_make_value(region, native, JSVAL_KIND_URL);
	*value_ptr = value;
	*native_ptr = native;
	return 0;
}

static int jsval_url_search_params_alloc_detached(jsval_region_t *region,
		size_t body_cap, size_t params_cap, jsval_t *value_ptr,
		jsval_native_url_search_params_t **native_ptr)
{
	jsval_native_url_search_params_t *native;
	void *ptr = NULL;
	jsval_t value;

	if (region == NULL || value_ptr == NULL || native_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_region_alloc(region, sizeof(*native),
			_Alignof(jsval_native_url_search_params_t), &ptr) < 0) {
		return -1;
	}
	native = (jsval_native_url_search_params_t *)ptr;
	memset(native, 0, sizeof(*native));
	if (jsval_url_field_alloc(region, body_cap, &native->body) < 0) {
		return -1;
	}
	native->params_cap = params_cap;
	native->attached = 0;
	value = jsval_native_make_value(region, native,
			JSVAL_KIND_URL_SEARCH_PARAMS);
	*value_ptr = value;
	*native_ptr = native;
	return 0;
}

static int jsval_url_search_params_current_body(jsval_region_t *region,
		jsval_native_url_search_params_t *native, jsstr8_t *body_ptr,
		jsval_native_url_t **owner_native_ptr)
{
	if (region == NULL || native == NULL || body_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->attached) {
		jsval_t owner_value = jsval_undefined();
		jsval_native_url_t *owner_native;

		owner_value.kind = JSVAL_KIND_URL;
		owner_value.repr = JSVAL_REPR_NATIVE;
		owner_value.off = native->owner_off;
		owner_native = jsval_native_url(region, owner_value);
		if (owner_native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (owner_native_ptr != NULL) {
			*owner_native_ptr = owner_native;
		}
		*body_ptr = jsval_url_query_body(jsval_url_field_value(region,
				&owner_native->search));
		return 0;
	}
	if (owner_native_ptr != NULL) {
		*owner_native_ptr = NULL;
	}
	*body_ptr = jsval_url_field_value(region, &native->body);
	return 0;
}

static int jsval_url_search_params_init_temp(jsurl_search_params_t *params,
		jsurl_param_t *params_buf, size_t params_cap, uint8_t *storage_buf,
		size_t storage_cap, jsstr8_t body)
{
	jsurl_search_params_storage_t storage;

	if (params == NULL || (params_cap > 0 && params_buf == NULL)
			|| (storage_cap > 0 && storage_buf == NULL)) {
		errno = EINVAL;
		return -1;
	}
	memset(&storage, 0, sizeof(storage));
	storage.params = params_cap > 0 ? params_buf : NULL;
	storage.params_cap = params_cap;
	jsstr8_init_from_buf(&storage.storage, (const char *)storage_buf,
			storage_cap);
	if (jsurl_search_params_init(params, &storage) < 0) {
		return -1;
	}
	return jsurl_search_params_parse(params, body);
}

static int jsval_url_search_params_parse_current(jsval_region_t *region,
		jsval_native_url_search_params_t *native, jsurl_search_params_t *params,
		jsurl_param_t *params_buf, size_t params_cap, uint8_t *storage_buf,
		size_t storage_cap)
{
	jsstr8_t body;

	if (jsval_url_search_params_current_body(region, native, &body, NULL) < 0) {
		return -1;
	}
	return jsval_url_search_params_init_temp(params, params_buf, params_cap,
			storage_buf, storage_cap, body);
}

static int jsval_url_search_params_stringify(jsval_region_t *region,
		jsval_native_url_search_params_t *native, jsval_t *value_ptr)
{
	jsstr8_t body;
	size_t params_cap;
	size_t storage_cap;

	if (region == NULL || native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body, NULL) < 0) {
		return -1;
	}
	params_cap = body.len > 0 ? body.len + 1 : 1;
	storage_cap = body.len > 0 ? body.len : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_search_params_t params;
		size_t out_len = 0;
		static const uint8_t empty[1] = {0};

		if (jsval_url_search_params_init_temp(&params, params_buf, params_cap,
				storage_buf, storage_cap, body) < 0) {
			return -1;
		}
		if (jsurl_search_params_measure(&params, &out_len) < 0) {
			return -1;
		}
		{
			uint8_t out_buf[out_len > 0 ? out_len : 1];
			jsstr8_t out;

			jsstr8_init_from_buf(&out, (const char *)out_buf, sizeof(out_buf));
			if (jsurl_search_params_serialize(&params, &out) < 0) {
				return -1;
			}
			return jsval_string_new_utf8(region, out.len > 0 ? out.bytes : empty,
					out.len, value_ptr);
		}
	}
}

static int jsval_url_mutate(jsval_region_t *region, jsval_t url_value,
		jsval_url_mutator_fn callback, void *opaque)
{
	jsval_native_url_t *native;
	size_t params_cap;
	size_t storage_cap;

	if (region == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	native = jsval_native_url(region, url_value);
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	params_cap = native->search_params_cap > 0 ? native->search_params_cap : 1;
	storage_cap = native->search.cap > 0 ? native->search.cap : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_t url;

		if (jsval_url_load_current(region, native, &url, params_buf, storage_buf,
				storage_cap) < 0) {
			return -1;
		}
		if (callback(&url, opaque) < 0) {
			return -1;
		}
		jsval_url_sync_native(native, &url);
	}
	return 0;
}

static int jsval_url_mutate_attached_search_params(jsval_region_t *region,
		jsval_native_url_search_params_t *native,
		jsval_url_search_params_mutator_fn callback, void *opaque)
{
	jsval_native_url_t *owner_native;
	jsstr8_t body;
	size_t params_cap;
	size_t storage_cap;

	if (region == NULL || native == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body,
			&owner_native) < 0) {
		return -1;
	}
	(void)body;
	if (owner_native == NULL) {
		errno = EINVAL;
		return -1;
	}
	params_cap = owner_native->search_params_cap > 0
		? owner_native->search_params_cap : 1;
	storage_cap = owner_native->search.cap > 0 ? owner_native->search.cap : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_t url;

		if (jsval_url_load_current(region, owner_native, &url, params_buf,
				storage_buf, storage_cap) < 0) {
			return -1;
		}
		if (callback(&url.search_params, opaque) < 0) {
			return -1;
		}
		jsval_url_sync_native(owner_native, &url);
	}
	return 0;
}

static int jsval_url_mutate_detached_search_params(jsval_region_t *region,
		jsval_native_url_search_params_t *native,
		jsval_url_search_params_mutator_fn callback, void *opaque)
{
	size_t params_cap;
	size_t storage_cap;
	jsstr8_t body_storage;

	if (region == NULL || native == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	params_cap = native->params_cap > 0 ? native->params_cap : 1;
	storage_cap = native->body.cap > 0 ? native->body.cap : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_search_params_t params;

		if (jsval_url_search_params_parse_current(region, native, &params,
				params_buf, params_cap, storage_buf, storage_cap) < 0) {
			return -1;
		}
		if (callback(&params, opaque) < 0) {
			return -1;
		}
		body_storage = jsval_url_field_storage(region, &native->body);
		body_storage.len = 0;
		if (jsurl_search_params_serialize(&params, &body_storage) < 0) {
			return -1;
		}
		native->body.len = body_storage.len;
	}
	return 0;
}

static int jsval_url_mutate_search_params(jsval_region_t *region,
		jsval_native_url_search_params_t *native,
		jsval_url_search_params_mutator_fn callback, void *opaque)
{
	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (native->attached) {
		return jsval_url_mutate_attached_search_params(region, native,
				callback, opaque);
	}
	return jsval_url_mutate_detached_search_params(region, native, callback,
			opaque);
}

static int jsval_url_set_href_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_parse_copy_with_base(url, ctx->value, url);
}

static int jsval_url_set_protocol_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_protocol(url, ctx->value);
}

static int jsval_url_set_username_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_username(url, ctx->value);
}

static int jsval_url_set_password_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_password(url, ctx->value);
}

static int jsval_url_split_host(jsstr8_t host, jsstr8_t *hostname_ptr,
		jsstr8_t *port_ptr)
{
	ssize_t last_colon = -1;
	size_t i;

	if (hostname_ptr == NULL || port_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*hostname_ptr = host;
	*port_ptr = jsstr8_empty;
	if (host.len == 0) {
		return 0;
	}
	for (i = 0; i < host.len; i++) {
		if (host.bytes[i] == ':') {
			last_colon = (ssize_t)i;
		}
	}
	if (last_colon < 0 || (size_t)last_colon + 1 >= host.len) {
		return 0;
	}
	for (i = (size_t)last_colon + 1; i < host.len; i++) {
		if (host.bytes[i] < '0' || host.bytes[i] > '9') {
			return 0;
		}
	}
	jsstr8_init(hostname_ptr);
	hostname_ptr->bytes = host.bytes;
	hostname_ptr->len = (size_t)last_colon;
	hostname_ptr->cap = hostname_ptr->len;
	jsstr8_init(port_ptr);
	port_ptr->bytes = host.bytes + last_colon + 1;
	port_ptr->len = host.len - ((size_t)last_colon + 1);
	port_ptr->cap = port_ptr->len;
	return 0;
}

static int jsval_url_set_host_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;
	jsstr8_t hostname;
	jsstr8_t port;

	if (jsval_url_split_host(ctx->value, &hostname, &port) < 0) {
		return -1;
	}
	if (jsurl_set_hostname(url, hostname) < 0) {
		return -1;
	}
	return jsurl_set_port(url, port);
}

static int jsval_url_set_hostname_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_hostname(url, ctx->value);
}

static int jsval_url_set_port_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_port(url, ctx->value);
}

static int jsval_url_set_pathname_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_pathname(url, ctx->value);
}

static int jsval_url_set_search_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_search(url, ctx->value);
}

static int jsval_url_set_hash_cb(jsurl_t *url, void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_set_hash(url, ctx->value);
}

static int jsval_url_search_params_append_cb(jsurl_search_params_t *params,
		void *opaque)
{
	const jsval_url_search_params_pair_ctx_t *ctx = opaque;

	return jsurl_search_params_append(params, ctx->name, ctx->value);
}

static int jsval_url_search_params_delete_cb(jsurl_search_params_t *params,
		void *opaque)
{
	const jsval_url_string_ctx_t *ctx = opaque;

	return jsurl_search_params_delete(params, ctx->value);
}

static int jsval_url_search_params_set_cb(jsurl_search_params_t *params,
		void *opaque)
{
	const jsval_url_search_params_pair_ctx_t *ctx = opaque;

	return jsurl_search_params_set(params, ctx->name, ctx->value);
}

static int jsval_url_search_params_sort_cb(jsurl_search_params_t *params,
		void *opaque)
{
	(void)opaque;
	return jsurl_search_params_sort(params);
}

static int jsval_url_construct_from_view(jsval_region_t *region,
		const jsurl_view_t *view, jsval_t *value_ptr)
{
	jsurl_sizes_t exact;
	jsurl_sizes_t caps;
	jsval_native_url_t *native;
	jsval_t value;
	size_t params_cap;
	size_t storage_cap;

	if (region == NULL || view == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_copy_sizes(view, &exact) < 0) {
		return -1;
	}
	if (jsval_url_apply_default_caps(&exact, &caps) < 0) {
		return -1;
	}
	if (jsval_url_alloc_native(region, &caps, &value, &native) < 0) {
		return -1;
	}
	params_cap = native->search_params_cap > 0 ? native->search_params_cap : 1;
	storage_cap = native->search.cap > 0 ? native->search.cap : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_t url;

		if (jsval_url_init_storage(region, native, &url, params_buf, storage_buf,
				storage_cap) < 0) {
			return -1;
		}
		if (jsurl_copy_from_view(&url, view) < 0) {
			return -1;
		}
		jsval_url_sync_native(native, &url);
	}
	if (jsval_url_attach_search_params(region, value, native, NULL) < 0) {
		return -1;
	}
	*value_ptr = value;
	return 0;
}

static int jsval_url_construct_from_base(jsval_region_t *region, jsstr8_t input,
		const jsurl_t *base, jsval_t *value_ptr)
{
	jsurl_sizes_t exact;
	jsurl_sizes_t caps;
	jsval_native_url_t *native;
	jsval_t value;
	size_t params_cap;
	size_t storage_cap;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_copy_sizes_with_base(input, base, &exact) < 0) {
		return -1;
	}
	if (jsval_url_apply_default_caps(&exact, &caps) < 0) {
		return -1;
	}
	if (jsval_url_alloc_native(region, &caps, &value, &native) < 0) {
		return -1;
	}
	params_cap = native->search_params_cap > 0 ? native->search_params_cap : 1;
	storage_cap = native->search.cap > 0 ? native->search.cap : 1;
	{
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_t url;

		if (jsval_url_init_storage(region, native, &url, params_buf, storage_buf,
				storage_cap) < 0) {
			return -1;
		}
		if (jsurl_parse_copy_with_base(&url, input, base) < 0) {
			return -1;
		}
		jsval_url_sync_native(native, &url);
	}
	if (jsval_url_attach_search_params(region, value, native, NULL) < 0) {
		return -1;
	}
	*value_ptr = value;
	return 0;
}

static int jsval_url_new_with_base_cb(const jsurl_t *base, void *opaque)
{
	jsval_url_new_with_base_ctx_t *ctx = opaque;

	if (ctx == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_construct_from_base(ctx->region, ctx->input, base,
			ctx->value_ptr);
}

int jsval_url_new(jsval_region_t *region, jsval_t input_value, int have_base,
		jsval_t base_value, jsval_t *value_ptr)
{
	size_t input_len = 0;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, input_value, &input_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t input_buf[input_len ? input_len : 1];
		jsstr8_t input;

		jsstr8_init_from_buf(&input, (const char *)input_buf,
				sizeof(input_buf));
		if (jsval_url_copy_string_utf8(region, input_value, &input.len, input_buf,
				sizeof(input_buf)) < 0) {
			return -1;
		}
		if (!have_base) {
			jsurl_view_t view;

			if (jsurl_view_parse(&view, input) < 0) {
				return -1;
			}
			return jsval_url_construct_from_view(region, &view, value_ptr);
		}
		if (base_value.kind == JSVAL_KIND_URL) {
			jsval_native_url_t *base_native = jsval_native_url(region, base_value);
			jsval_url_new_with_base_ctx_t ctx;

			if (base_native == NULL) {
				errno = EINVAL;
				return -1;
			}
			ctx.region = region;
			ctx.input = input;
			ctx.value_ptr = value_ptr;
			return jsval_url_with_bound_native(region, base_native,
					jsval_url_new_with_base_cb, &ctx);
		}
		{
			size_t base_len = 0;

			if (jsval_url_copy_string_utf8(region, base_value, &base_len, NULL,
					0) < 0) {
				return -1;
			}
			{
				uint8_t base_buf[base_len ? base_len : 1];
				jsstr8_t base_input;
				jsval_url_new_with_base_ctx_t ctx;

				jsstr8_init_from_buf(&base_input, (const char *)base_buf,
						sizeof(base_buf));
				if (jsval_url_copy_string_utf8(region, base_value,
						&base_input.len, base_buf,
						sizeof(base_buf)) < 0) {
					return -1;
				}
				ctx.region = region;
				ctx.input = input;
				ctx.value_ptr = value_ptr;
				return jsval_url_with_temp_parsed(base_input,
						jsval_url_new_with_base_cb, &ctx);
			}
		}
	}
}

int jsval_url_href(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->href, value_ptr);
}

int jsval_url_origin(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->origin, value_ptr);
}

int jsval_url_origin_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	return jsval_url_make_callback_string(region, url_value,
			jsurl_origin_display_measure, jsurl_origin_display_serialize,
			value_ptr);
}

int jsval_url_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->protocol, value_ptr);
}

int jsval_url_username(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->username, value_ptr);
}

int jsval_url_password(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->password, value_ptr);
}

int jsval_url_host(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->host, value_ptr);
}

int jsval_url_host_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	return jsval_url_make_callback_string(region, url_value,
			jsurl_host_display_measure, jsurl_host_display_serialize, value_ptr);
}

int jsval_url_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->hostname, value_ptr);
}

int jsval_url_hostname_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	return jsval_url_make_callback_string(region, url_value,
			jsurl_hostname_display_measure,
			jsurl_hostname_display_serialize, value_ptr);
}

int jsval_url_port(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->port, value_ptr);
}

int jsval_url_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->pathname, value_ptr);
}

int jsval_url_search(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->search, value_ptr);
}

int jsval_url_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_field_make_string(region, &native->hash, value_ptr);
}

int jsval_url_search_params(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	jsval_native_url_t *native = jsval_native_url(region, url_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_attach_search_params(region, url_value, native, value_ptr) < 0) {
		return -1;
	}
	return 0;
}

int jsval_url_set_href(jsval_region_t *region, jsval_t url_value,
		jsval_t href_value)
{
	size_t href_len = 0;

	if (jsval_url_copy_string_utf8(region, href_value, &href_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t href_buf[href_len ? href_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)href_buf,
				sizeof(href_buf));
		if (jsval_url_copy_string_utf8(region, href_value, &ctx.value.len,
				href_buf, sizeof(href_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_href_cb, &ctx);
	}
}

int jsval_url_set_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t protocol_value)
{
	size_t protocol_len = 0;

	if (jsval_url_copy_string_utf8(region, protocol_value, &protocol_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t protocol_buf[protocol_len ? protocol_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)protocol_buf,
				sizeof(protocol_buf));
		if (jsval_url_copy_string_utf8(region, protocol_value, &ctx.value.len,
				protocol_buf, sizeof(protocol_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_protocol_cb,
				&ctx);
	}
}

int jsval_url_set_username(jsval_region_t *region, jsval_t url_value,
		jsval_t username_value)
{
	size_t username_len = 0;

	if (jsval_url_copy_string_utf8(region, username_value, &username_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t username_buf[username_len ? username_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)username_buf,
				sizeof(username_buf));
		if (jsval_url_copy_string_utf8(region, username_value, &ctx.value.len,
				username_buf, sizeof(username_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_username_cb,
				&ctx);
	}
}

int jsval_url_set_password(jsval_region_t *region, jsval_t url_value,
		jsval_t password_value)
{
	size_t password_len = 0;

	if (jsval_url_copy_string_utf8(region, password_value, &password_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t password_buf[password_len ? password_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)password_buf,
				sizeof(password_buf));
		if (jsval_url_copy_string_utf8(region, password_value, &ctx.value.len,
				password_buf, sizeof(password_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_password_cb,
				&ctx);
	}
}

int jsval_url_set_host(jsval_region_t *region, jsval_t url_value,
		jsval_t host_value)
{
	size_t host_len = 0;

	if (jsval_url_copy_string_utf8(region, host_value, &host_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t host_buf[host_len ? host_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)host_buf,
				sizeof(host_buf));
		if (jsval_url_copy_string_utf8(region, host_value, &ctx.value.len,
				host_buf, sizeof(host_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_host_cb, &ctx);
	}
}

int jsval_url_set_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t hostname_value)
{
	size_t hostname_len = 0;

	if (jsval_url_copy_string_utf8(region, hostname_value, &hostname_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t hostname_buf[hostname_len ? hostname_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)hostname_buf,
				sizeof(hostname_buf));
		if (jsval_url_copy_string_utf8(region, hostname_value, &ctx.value.len,
				hostname_buf, sizeof(hostname_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_hostname_cb,
				&ctx);
	}
}

int jsval_url_set_port(jsval_region_t *region, jsval_t url_value,
		jsval_t port_value)
{
	size_t port_len = 0;

	if (jsval_url_copy_string_utf8(region, port_value, &port_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t port_buf[port_len ? port_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)port_buf,
				sizeof(port_buf));
		if (jsval_url_copy_string_utf8(region, port_value, &ctx.value.len,
				port_buf, sizeof(port_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_port_cb, &ctx);
	}
}

int jsval_url_set_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t pathname_value)
{
	size_t pathname_len = 0;

	if (jsval_url_copy_string_utf8(region, pathname_value, &pathname_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t pathname_buf[pathname_len ? pathname_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)pathname_buf,
				sizeof(pathname_buf));
		if (jsval_url_copy_string_utf8(region, pathname_value, &ctx.value.len,
				pathname_buf, sizeof(pathname_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_pathname_cb,
				&ctx);
	}
}

int jsval_url_set_search(jsval_region_t *region, jsval_t url_value,
		jsval_t search_value)
{
	size_t search_len = 0;

	if (jsval_url_copy_string_utf8(region, search_value, &search_len, NULL, 0)
			< 0) {
		return -1;
	}
	{
		uint8_t search_buf[search_len ? search_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)search_buf,
				sizeof(search_buf));
		if (jsval_url_copy_string_utf8(region, search_value, &ctx.value.len,
				search_buf, sizeof(search_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_search_cb,
				&ctx);
	}
}

int jsval_url_set_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t hash_value)
{
	size_t hash_len = 0;

	if (jsval_url_copy_string_utf8(region, hash_value, &hash_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t hash_buf[hash_len ? hash_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)hash_buf,
				sizeof(hash_buf));
		if (jsval_url_copy_string_utf8(region, hash_value, &ctx.value.len,
				hash_buf, sizeof(hash_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate(region, url_value, jsval_url_set_hash_cb, &ctx);
	}
}

int jsval_url_to_string(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	return jsval_url_href(region, url_value, value_ptr);
}

int jsval_url_to_json(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr)
{
	return jsval_url_href(region, url_value, value_ptr);
}

int jsval_url_search_params_new(jsval_region_t *region, jsval_t init_value,
		jsval_t *value_ptr)
{
	jsstr8_t input;
	jsurl_search_params_view_t view;
	size_t input_len = 0;
	size_t temp_params_cap;
	size_t temp_storage_cap;
	size_t body_len;
	size_t body_cap;
	size_t params_cap;
	jsval_native_url_search_params_t *native;
	jsval_t value;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (init_value.kind == JSVAL_KIND_UNDEFINED) {
		jsstr8_init(&input);
		input_len = 0;
	} else if (jsval_url_copy_string_utf8(region, init_value, &input_len, NULL,
			0) < 0) {
		return -1;
	}
	{
		uint8_t input_buf[input_len ? input_len : 1];

		if (input_len > 0) {
			jsstr8_init_from_buf(&input, (const char *)input_buf,
					sizeof(input_buf));
			if (jsval_url_copy_string_utf8(region, init_value, &input.len,
					input_buf, sizeof(input_buf)) < 0) {
				return -1;
			}
		}
		jsurl_search_params_view_init(&view, input);
		temp_params_cap = jsurl_search_params_view_size(&view);
		temp_storage_cap = input.len > 0 ? input.len : 1;
		{
			jsurl_param_t params_buf[temp_params_cap > 0 ? temp_params_cap : 1];
			uint8_t storage_buf[temp_storage_cap];
			jsurl_search_params_t params;

			if (jsval_url_search_params_init_temp(&params,
					temp_params_cap > 0 ? params_buf : NULL,
					temp_params_cap, storage_buf, sizeof(storage_buf), input) < 0) {
				return -1;
			}
			if (jsurl_search_params_measure(&params, &body_len) < 0) {
				return -1;
			}
			if (jsval_url_search_params_default_caps(body_len, params.len,
					&body_cap, &params_cap) < 0) {
				return -1;
			}
			if (jsval_url_search_params_alloc_detached(region, body_cap,
					params_cap, &value, &native) < 0) {
				return -1;
			}
			{
				jsstr8_t body_storage = jsval_url_field_storage(region,
						&native->body);

				body_storage.len = 0;
				if (jsurl_search_params_serialize(&params, &body_storage) < 0) {
					return -1;
				}
				native->body.len = body_storage.len;
			}
		}
	}
	*value_ptr = value;
	return 0;
}

int jsval_url_search_params_size(jsval_region_t *region, jsval_t params_value,
		jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	jsstr8_t body;
	jsurl_search_params_view_t view;

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body, NULL) < 0) {
		return -1;
	}
	jsurl_search_params_view_init(&view, body);
	*value_ptr = jsval_number((double)jsurl_search_params_view_size(&view));
	return 0;
}

int jsval_url_search_params_append(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;
	size_t value_len = 0;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0
			|| jsval_url_copy_string_utf8(region, value_value, &value_len, NULL,
				0) < 0) {
		return -1;
	}
	{
		uint8_t name_buf[name_len ? name_len : 1];
		uint8_t value_buf[value_len ? value_len : 1];
		jsval_url_search_params_pair_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.name, (const char *)name_buf,
				sizeof(name_buf));
		jsstr8_init_from_buf(&ctx.value, (const char *)value_buf,
				sizeof(value_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &ctx.name.len,
				name_buf, sizeof(name_buf)) < 0
				|| jsval_url_copy_string_utf8(region, value_value,
					&ctx.value.len, value_buf, sizeof(value_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate_search_params(region, native,
				jsval_url_search_params_append_cb, &ctx);
	}
}

int jsval_url_search_params_delete(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0) {
		return -1;
	}
	{
		uint8_t name_buf[name_len ? name_len : 1];
		jsval_url_string_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.value, (const char *)name_buf,
				sizeof(name_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &ctx.value.len,
				name_buf, sizeof(name_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate_search_params(region, native,
				jsval_url_search_params_delete_cb, &ctx);
	}
}

int jsval_url_search_params_get(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;
	size_t params_cap;
	size_t storage_cap;
	jsstr8_t body;
	jsval_native_url_t *owner_native = NULL;
	int found;
	jsval_t result;

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0) {
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body,
			&owner_native) < 0) {
		return -1;
	}
	params_cap = native->attached && owner_native != NULL
		? owner_native->search_params_cap : native->params_cap;
	storage_cap = native->attached && owner_native != NULL
		? owner_native->search.cap : native->body.cap;
	params_cap = params_cap > 0 ? params_cap : 1;
	storage_cap = storage_cap > 0 ? storage_cap : 1;
	{
		uint8_t name_buf[name_len ? name_len : 1];
		jsstr8_t name;
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_search_params_t params;
		jsstr8_t value;

		jsstr8_init_from_buf(&name, (const char *)name_buf, sizeof(name_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &name.len, name_buf,
				sizeof(name_buf)) < 0) {
			return -1;
		}
		if (jsval_url_search_params_init_temp(&params, params_buf, params_cap,
				storage_buf, storage_cap, body) < 0) {
			return -1;
		}
		if (jsurl_search_params_get(&params, name, &value, &found) < 0) {
			return -1;
		}
		if (found) {
			if (jsval_url_jsstr_make_string(region, value, &result) < 0) {
				return -1;
			}
		} else {
			result = jsval_null();
		}
	}
	*value_ptr = result;
	return 0;
}

int jsval_url_search_params_get_all(jsval_region_t *region,
		jsval_t params_value, jsval_t name_value, jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;
	size_t params_cap;
	size_t storage_cap;
	jsstr8_t body;
	jsval_native_url_t *owner_native = NULL;

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0) {
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body,
			&owner_native) < 0) {
		return -1;
	}
	params_cap = native->attached && owner_native != NULL
		? owner_native->search_params_cap : native->params_cap;
	storage_cap = native->attached && owner_native != NULL
		? owner_native->search.cap : native->body.cap;
	params_cap = params_cap > 0 ? params_cap : 1;
	storage_cap = storage_cap > 0 ? storage_cap : 1;
	{
		uint8_t name_buf[name_len ? name_len : 1];
		jsstr8_t name;
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_search_params_t params;
		size_t out_len;
		size_t found_len;
		jsval_t array;
		size_t i;

		jsstr8_init_from_buf(&name, (const char *)name_buf, sizeof(name_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &name.len, name_buf,
				sizeof(name_buf)) < 0) {
			return -1;
		}
		if (jsval_url_search_params_init_temp(&params, params_buf, params_cap,
				storage_buf, storage_cap, body) < 0) {
			return -1;
		}
		out_len = params.len;
		{
			jsstr8_t values[out_len > 0 ? out_len : 1];

			if (jsurl_search_params_get_all(&params, name,
					out_len > 0 ? values : NULL, &out_len, &found_len) < 0) {
				return -1;
			}
			if (jsval_array_new(region, found_len, &array) < 0) {
				return -1;
			}
			for (i = 0; i < found_len; i++) {
				jsval_t entry;

				if (jsval_url_jsstr_make_string(region, values[i], &entry) < 0) {
					return -1;
				}
				if (jsval_array_push(region, array, entry) < 0) {
					return -1;
				}
			}
		}
		*value_ptr = array;
	}
	return 0;
}

int jsval_url_search_params_has(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;
	size_t params_cap;
	size_t storage_cap;
	jsstr8_t body;
	jsval_native_url_t *owner_native = NULL;
	int found = 0;

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0) {
		return -1;
	}
	if (jsval_url_search_params_current_body(region, native, &body,
			&owner_native) < 0) {
		return -1;
	}
	params_cap = native->attached && owner_native != NULL
		? owner_native->search_params_cap : native->params_cap;
	storage_cap = native->attached && owner_native != NULL
		? owner_native->search.cap : native->body.cap;
	params_cap = params_cap > 0 ? params_cap : 1;
	storage_cap = storage_cap > 0 ? storage_cap : 1;
	{
		uint8_t name_buf[name_len ? name_len : 1];
		jsstr8_t name;
		jsurl_param_t params_buf[params_cap];
		uint8_t storage_buf[storage_cap];
		jsurl_search_params_t params;

		jsstr8_init_from_buf(&name, (const char *)name_buf, sizeof(name_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &name.len, name_buf,
				sizeof(name_buf)) < 0) {
			return -1;
		}
		if (jsval_url_search_params_init_temp(&params, params_buf, params_cap,
				storage_buf, storage_cap, body) < 0) {
			return -1;
		}
		if (jsurl_search_params_has(&params, name, &found) < 0) {
			return -1;
		}
	}
	*value_ptr = jsval_bool(found);
	return 0;
}

int jsval_url_search_params_set(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);
	size_t name_len = 0;
	size_t value_len = 0;

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_url_copy_string_utf8(region, name_value, &name_len, NULL, 0) < 0
			|| jsval_url_copy_string_utf8(region, value_value, &value_len, NULL,
				0) < 0) {
		return -1;
	}
	{
		uint8_t name_buf[name_len ? name_len : 1];
		uint8_t value_buf[value_len ? value_len : 1];
		jsval_url_search_params_pair_ctx_t ctx;

		jsstr8_init_from_buf(&ctx.name, (const char *)name_buf,
				sizeof(name_buf));
		jsstr8_init_from_buf(&ctx.value, (const char *)value_buf,
				sizeof(value_buf));
		if (jsval_url_copy_string_utf8(region, name_value, &ctx.name.len,
				name_buf, sizeof(name_buf)) < 0
				|| jsval_url_copy_string_utf8(region, value_value,
					&ctx.value.len, value_buf, sizeof(value_buf)) < 0) {
			return -1;
		}
		return jsval_url_mutate_search_params(region, native,
				jsval_url_search_params_set_cb, &ctx);
	}
}

int jsval_url_search_params_sort(jsval_region_t *region, jsval_t params_value)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);

	if (native == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_mutate_search_params(region, native,
			jsval_url_search_params_sort_cb, NULL);
}

int jsval_url_search_params_to_string(jsval_region_t *region,
		jsval_t params_value, jsval_t *value_ptr)
{
	jsval_native_url_search_params_t *native =
		jsval_native_url_search_params(region, params_value);

	if (native == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_url_search_params_stringify(region, native, value_ptr);
}

int jsval_is_nullish(jsval_t value)
{
	return value.kind == JSVAL_KIND_UNDEFINED || value.kind == JSVAL_KIND_NULL;
}

int jsval_typeof(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	const uint8_t *text;
	size_t len;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		text = (const uint8_t *)"undefined";
		len = 9;
		break;
	case JSVAL_KIND_NULL:
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		text = (const uint8_t *)"object";
		len = 6;
		break;
	case JSVAL_KIND_BOOL:
		text = (const uint8_t *)"boolean";
		len = 7;
		break;
	case JSVAL_KIND_NUMBER:
		text = (const uint8_t *)"number";
		len = 6;
		break;
	case JSVAL_KIND_STRING:
		text = (const uint8_t *)"string";
		len = 6;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return jsval_string_new_utf8(region, text, len, value_ptr);
}

int jsval_truthy(jsval_region_t *region, jsval_t value)
{
	jsval_json_doc_t *doc;
	size_t len;
	double number;
	int boolean;

	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
	case JSVAL_KIND_NULL:
		return 0;
	case JSVAL_KIND_BOOL:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
				return 0;
			}
			return boolean;
		}
		return value.as.boolean != 0;
	case JSVAL_KIND_NUMBER:
		if (value.repr == JSVAL_REPR_JSON) {
			doc = jsval_json_doc(region, value);
			if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
				return 0;
			}
			return number != 0 && number == number;
		}
		return value.as.number != 0 && value.as.number == value.as.number;
	case JSVAL_KIND_STRING:
		if (value.repr == JSVAL_REPR_NATIVE) {
			jsval_native_string_t *string = jsval_native_string(region, value);
			return string != NULL && string->len > 0;
		}
		doc = jsval_json_doc(region, value);
		if (jsval_json_string_copy_utf16(region, doc, value.as.index, NULL, 0, &len) < 0) {
			return 0;
		}
		return len > 0;
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		return 1;
	default:
		return 0;
	}
}

int jsval_strict_eq(jsval_region_t *region, jsval_t left, jsval_t right)
{
	if (left.kind != right.kind) {
		return 0;
	}

	switch (left.kind) {
	case JSVAL_KIND_UNDEFINED:
	case JSVAL_KIND_NULL:
		return 1;
	case JSVAL_KIND_BOOL:
		return jsval_truthy(region, left) == jsval_truthy(region, right);
	case JSVAL_KIND_NUMBER:
	{
		double left_number;
		double right_number;

		if (jsval_to_number(region, left, &left_number) < 0 || jsval_to_number(region, right, &right_number) < 0) {
			return 0;
		}
		if (left_number != left_number || right_number != right_number) {
			return 0;
		}
		return left_number == right_number;
	}
	case JSVAL_KIND_STRING:
	{
		size_t left_len;
		size_t right_len;

		if (jsval_value_utf16_len(region, left, &left_len) < 0 || jsval_value_utf16_len(region, right, &right_len) < 0) {
			return 0;
		}
		if (left_len != right_len) {
			return 0;
		}

		{
			uint16_t left_buf[left_len ? left_len : 1];
			uint16_t right_buf[right_len ? right_len : 1];

			if (jsval_value_copy_utf16(region, left, left_buf, left_len, NULL) < 0 ||
			    jsval_value_copy_utf16(region, right, right_buf, right_len, NULL) < 0) {
				return 0;
			}
			return memcmp(left_buf, right_buf, left_len * sizeof(uint16_t)) == 0;
		}
	}
	case JSVAL_KIND_OBJECT:
	case JSVAL_KIND_ARRAY:
	case JSVAL_KIND_REGEXP:
	case JSVAL_KIND_MATCH_ITERATOR:
	case JSVAL_KIND_URL:
	case JSVAL_KIND_URL_SEARCH_PARAMS:
	case JSVAL_KIND_SET:
	case JSVAL_KIND_MAP:
	case JSVAL_KIND_ITERATOR:
		if (left.repr != right.repr) {
			return 0;
		}
		if (left.repr == JSVAL_REPR_JSON) {
			return left.off == right.off && left.as.index == right.as.index;
		}
		return left.off == right.off;
	default:
		return 0;
	}
}

int jsval_abstract_eq(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	double left_number;
	double right_number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind == JSVAL_KIND_OBJECT || left.kind == JSVAL_KIND_ARRAY
			|| left.kind == JSVAL_KIND_REGEXP
			|| left.kind == JSVAL_KIND_MATCH_ITERATOR
			|| left.kind == JSVAL_KIND_URL
			|| left.kind == JSVAL_KIND_URL_SEARCH_PARAMS
			|| left.kind == JSVAL_KIND_SET
			|| left.kind == JSVAL_KIND_MAP
			|| left.kind == JSVAL_KIND_ITERATOR
			|| right.kind == JSVAL_KIND_OBJECT
			|| right.kind == JSVAL_KIND_ARRAY
			|| right.kind == JSVAL_KIND_REGEXP
			|| right.kind == JSVAL_KIND_MATCH_ITERATOR
			|| right.kind == JSVAL_KIND_URL
			|| right.kind == JSVAL_KIND_URL_SEARCH_PARAMS
			|| right.kind == JSVAL_KIND_SET
			|| right.kind == JSVAL_KIND_MAP
			|| right.kind == JSVAL_KIND_ITERATOR) {
		errno = ENOTSUP;
		return -1;
	}
	if (left.kind == right.kind) {
		*result_ptr = jsval_strict_eq(region, left, right);
		return 0;
	}
	if ((left.kind == JSVAL_KIND_UNDEFINED && right.kind == JSVAL_KIND_NULL)
			|| (left.kind == JSVAL_KIND_NULL
				&& right.kind == JSVAL_KIND_UNDEFINED)) {
		*result_ptr = 1;
		return 0;
	}
	if (left.kind == JSVAL_KIND_UNDEFINED || left.kind == JSVAL_KIND_NULL
			|| right.kind == JSVAL_KIND_UNDEFINED
			|| right.kind == JSVAL_KIND_NULL) {
		*result_ptr = 0;
		return 0;
	}
	if ((left.kind == JSVAL_KIND_BOOL || left.kind == JSVAL_KIND_NUMBER
				|| left.kind == JSVAL_KIND_STRING)
			&& (right.kind == JSVAL_KIND_BOOL || right.kind == JSVAL_KIND_NUMBER
				|| right.kind == JSVAL_KIND_STRING)) {
		if (jsval_to_number(region, left, &left_number) < 0
				|| jsval_to_number(region, right, &right_number) < 0) {
			return -1;
		}
		if (left_number != left_number || right_number != right_number) {
			*result_ptr = 0;
			return 0;
		}
		*result_ptr = left_number == right_number;
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int jsval_abstract_ne(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	int result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_abstract_eq(region, left, right, &result) < 0) {
		return -1;
	}
	*result_ptr = !result;
	return 0;
}

static int
jsval_string_compare(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	size_t left_len;
	size_t right_len;
	size_t min_len;
	int result = 0;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind != JSVAL_KIND_STRING || right.kind != JSVAL_KIND_STRING) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, left, &left_len) < 0
			|| jsval_value_utf16_len(region, right, &right_len) < 0) {
		return -1;
	}

	{
		uint16_t left_buf[left_len ? left_len : 1];
		uint16_t right_buf[right_len ? right_len : 1];

		if (jsval_value_copy_utf16(region, left, left_buf, left_len, NULL) < 0
				|| jsval_value_copy_utf16(region, right, right_buf, right_len,
					NULL) < 0) {
			return -1;
		}

		min_len = left_len < right_len ? left_len : right_len;
		for (size_t i = 0; i < min_len; i++) {
			if (left_buf[i] < right_buf[i]) {
				result = -1;
				goto done;
			}
			if (left_buf[i] > right_buf[i]) {
				result = 1;
				goto done;
			}
		}
	}

	if (left_len < right_len) {
		result = -1;
	} else if (left_len > right_len) {
		result = 1;
	}

done:
	*result_ptr = result;
	return 0;
}

typedef enum jsval_relop_e {
	JSVAL_RELOP_LT,
	JSVAL_RELOP_LE,
	JSVAL_RELOP_GT,
	JSVAL_RELOP_GE
} jsval_relop_t;

static int
jsval_relop(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_relop_t op, int *result_ptr)
{
	double left_number;
	double right_number;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (left.kind == JSVAL_KIND_STRING && right.kind == JSVAL_KIND_STRING) {
		int cmp;

		if (jsval_string_compare(region, left, right, &cmp) < 0) {
			return -1;
		}
		switch (op) {
		case JSVAL_RELOP_LT:
			*result_ptr = cmp < 0;
			return 0;
		case JSVAL_RELOP_LE:
			*result_ptr = cmp <= 0;
			return 0;
		case JSVAL_RELOP_GT:
			*result_ptr = cmp > 0;
			return 0;
		case JSVAL_RELOP_GE:
			*result_ptr = cmp >= 0;
			return 0;
		default:
			errno = EINVAL;
			return -1;
		}
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}
	if (left_number != left_number || right_number != right_number) {
		*result_ptr = 0;
		return 0;
	}

	switch (op) {
	case JSVAL_RELOP_LT:
		*result_ptr = left_number < right_number;
		return 0;
	case JSVAL_RELOP_LE:
		*result_ptr = left_number <= right_number;
		return 0;
	case JSVAL_RELOP_GT:
		*result_ptr = left_number > right_number;
		return 0;
	case JSVAL_RELOP_GE:
		*result_ptr = left_number >= right_number;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int jsval_less_than(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_LT, result_ptr);
}

int jsval_less_equal(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_LE, result_ptr);
}

int jsval_greater_than(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_GT, result_ptr);
}

int jsval_greater_equal(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr)
{
	return jsval_relop(region, left, right, JSVAL_RELOP_GE, result_ptr);
}

int jsval_add(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr)
{
	if (left.kind == JSVAL_KIND_STRING || right.kind == JSVAL_KIND_STRING) {
		size_t left_len;
		size_t right_len;
		uint16_t *buf;
		size_t total_len;
		jsval_native_string_t *string;
		jsval_off_t off;

		if (jsval_value_utf16_len(region, left, &left_len) < 0 || jsval_value_utf16_len(region, right, &right_len) < 0) {
			return -1;
		}

		total_len = left_len + right_len;
		if (jsval_region_reserve(region, sizeof(*string) + total_len * sizeof(uint16_t), JSVAL_ALIGN, &off, (void **)&string) < 0) {
			return -1;
		}

		string->len = total_len;
		string->cap = total_len;
		buf = jsval_native_string_units(string);
		if (jsval_value_copy_utf16(region, left, buf, left_len, NULL) < 0) {
			return -1;
		}
		if (jsval_value_copy_utf16(region, right, buf + left_len, right_len, NULL) < 0) {
			return -1;
		}

		*value_ptr = jsval_undefined();
		value_ptr->kind = JSVAL_KIND_STRING;
		value_ptr->repr = JSVAL_REPR_NATIVE;
		value_ptr->off = off;
		return 0;
	}

	{
		double left_number;
		double right_number;

		if (jsval_to_number(region, left, &left_number) < 0 || jsval_to_number(region, right, &right_number) < 0) {
			return -1;
		}

		*value_ptr = jsval_number(left_number + right_number);
		return 0;
	}
}

int jsval_unary_plus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	double number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(number);
	return 0;
}

int jsval_unary_minus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	double number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, value, &number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(-number);
	return 0;
}

int jsval_bitwise_not(jsval_region_t *region, jsval_t value,
		jsval_t *value_ptr)
{
	int32_t integer;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_int32(region, value, &integer) < 0) {
		return -1;
	}

	*value_ptr = jsval_number((double)(~integer));
	return 0;
}

typedef enum jsval_bitwise_op_e {
	JSVAL_BITWISE_AND,
	JSVAL_BITWISE_OR,
	JSVAL_BITWISE_XOR
} jsval_bitwise_op_t;

typedef enum jsval_shift_op_e {
	JSVAL_SHIFT_LEFT,
	JSVAL_SHIFT_RIGHT,
	JSVAL_SHIFT_RIGHT_UNSIGNED
} jsval_shift_op_t;

static int jsval_bitwise(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_bitwise_op_t op, jsval_t *value_ptr)
{
	int32_t left_integer;
	int32_t right_integer;
	int32_t result;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_int32(region, left, &left_integer) < 0
			|| jsval_to_int32(region, right, &right_integer) < 0) {
		return -1;
	}

	switch (op) {
	case JSVAL_BITWISE_AND:
		result = left_integer & right_integer;
		break;
	case JSVAL_BITWISE_OR:
		result = left_integer | right_integer;
		break;
	case JSVAL_BITWISE_XOR:
		result = left_integer ^ right_integer;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	*value_ptr = jsval_number((double)result);
	return 0;
}

static int jsval_shift(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_shift_op_t op, jsval_t *value_ptr)
{
	uint32_t right_integer;
	uint32_t shift_count;
	uint32_t bits;
	int32_t left_integer;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_uint32(region, right, &right_integer) < 0) {
		return -1;
	}

	shift_count = right_integer & 0x1f;
	switch (op) {
	case JSVAL_SHIFT_LEFT:
		if (jsval_to_int32(region, left, &left_integer) < 0) {
			return -1;
		}
		bits = ((uint32_t)left_integer) << shift_count;
		*value_ptr = jsval_number((double)(int32_t)bits);
		return 0;
	case JSVAL_SHIFT_RIGHT:
		if (jsval_to_int32(region, left, &left_integer) < 0) {
			return -1;
		}
		if (shift_count == 0) {
			*value_ptr = jsval_number((double)left_integer);
			return 0;
		}

		bits = ((uint32_t)left_integer) >> shift_count;
		if (left_integer < 0) {
			bits |= (~(uint32_t)0) << (32 - shift_count);
		}
		*value_ptr = jsval_number((double)(int32_t)bits);
		return 0;
	case JSVAL_SHIFT_RIGHT_UNSIGNED:
		if (jsval_to_uint32(region, left, &bits) < 0) {
			return -1;
		}
		*value_ptr = jsval_number((double)(bits >> shift_count));
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int jsval_subtract(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number - right_number);
	return 0;
}

int jsval_bitwise_and(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_AND, value_ptr);
}

int jsval_bitwise_or(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_OR, value_ptr);
}

int jsval_bitwise_xor(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_bitwise(region, left, right, JSVAL_BITWISE_XOR, value_ptr);
}

int jsval_shift_left(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_LEFT, value_ptr);
}

int jsval_shift_right(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_RIGHT, value_ptr);
}

int jsval_shift_right_unsigned(jsval_region_t *region, jsval_t left,
		jsval_t right, jsval_t *value_ptr)
{
	return jsval_shift(region, left, right, JSVAL_SHIFT_RIGHT_UNSIGNED,
			value_ptr);
}

int jsval_multiply(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number * right_number);
	return 0;
}

int jsval_divide(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(left_number / right_number);
	return 0;
}

int jsval_remainder(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr)
{
	double left_number;
	double right_number;

	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_to_number(region, left, &left_number) < 0
			|| jsval_to_number(region, right, &right_number) < 0) {
		return -1;
	}

	*value_ptr = jsval_number(jsnum_remainder(left_number, right_number));
	return 0;
}

int jsval_method_string_to_lower_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value,
			JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_lower_case, value_ptr, error);
}

int jsval_method_string_to_upper_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value,
			JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_upper_case, value_ptr, error);
}

int jsval_method_string_to_locale_lower_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_locale_bridge(region, this_value, have_locale,
			locale_value, JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_locale_lower_case, value_ptr, error);
}

int jsval_method_string_to_locale_upper_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_locale_bridge(region, this_value, have_locale,
			locale_value, JSVAL_METHOD_CASE_EXPANSION_MAX,
			jsmethod_string_to_locale_upper_case, value_ptr, error);
}

int jsval_method_string_to_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_to_well_formed, value_ptr, error);
}

int jsval_method_string_is_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsmethod_value_t method_value;
	int is_well_formed;
	size_t storage_cap;

	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_value_utf16_len(region, this_value, &storage_cap) < 0) {
		return -1;
	}
	if (storage_cap == 0) {
		storage_cap = 1;
	}

	{
		uint16_t storage[storage_cap];

		if (jsval_method_value_from_jsval(region, this_value, storage,
				storage_cap, &method_value) < 0) {
			return -1;
		}
		if (jsmethod_string_is_well_formed(&is_well_formed, method_value,
				storage, storage_cap, error) < 0) {
			return -1;
		}
	}

	*value_ptr = jsval_bool(is_well_formed);
	return 0;
}

int jsval_method_string_trim(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim, value_ptr, error);
}

int jsval_method_string_trim_start(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_start, value_ptr, error);
}

int jsval_method_string_trim_end(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_end, value_ptr, error);
}

int jsval_method_string_trim_left(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_left, value_ptr, error);
}

int jsval_method_string_trim_right(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_unary_bridge(region, this_value, 1,
			jsmethod_string_trim_right, value_ptr, error);
}

int
jsval_method_string_concat(jsval_region_t *region, jsval_t this_value,
		size_t arg_count, const jsval_t *args, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_concat_bridge(region, this_value, arg_count,
			args, jsmethod_string_concat_measure, jsmethod_string_concat,
			value_ptr, error);
}

#if JSMX_WITH_REGEX
#define JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(name, kind_value) \
int jsval_method_string_search_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error) \
{ \
	return jsval_method_string_search_u_predefined_class_common(region, \
			this_value, kind_value, value_ptr, error); \
} \
 \
int jsval_method_string_match_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error) \
{ \
	return jsval_method_string_match_all_u_predefined_class_common(region, \
			this_value, kind_value, value_ptr, error); \
} \
 \
int jsval_method_string_match_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int global, \
		jsval_t *value_ptr, jsmethod_error_t *error) \
{ \
	return jsval_method_string_match_u_predefined_class_common(region, \
			this_value, kind_value, global, value_ptr, error); \
} \
 \
int jsval_method_string_replace_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error) \
{ \
	return jsval_method_string_replace_u_predefined_class_common(region, \
			this_value, kind_value, replacement_value, 0, value_ptr, \
			error); \
} \
 \
int jsval_method_string_replace_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error) \
{ \
	return jsval_method_string_replace_u_predefined_class_common(region, \
			this_value, kind_value, replacement_value, 1, value_ptr, \
			error); \
} \
 \
int jsval_method_string_replace_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error) \
{ \
	return jsval_method_string_replace_u_predefined_class_fn_common(region, \
			this_value, kind_value, callback, callback_ctx, 0, value_ptr, \
			error); \
} \
 \
int jsval_method_string_replace_all_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error) \
{ \
	return jsval_method_string_replace_u_predefined_class_fn_common(region, \
			this_value, kind_value, callback, callback_ctx, 1, value_ptr, \
			error); \
} \
 \
int jsval_method_string_split_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int have_limit, \
		jsval_t limit_value, jsval_t *value_ptr, \
		jsmethod_error_t *error) \
{ \
	return jsval_method_string_split_u_predefined_class_common(region, \
			this_value, kind_value, have_limit, limit_value, value_ptr, \
			error); \
}

JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(digit,
		JSREGEX_U_PREDEFINED_CLASS_DIGIT)
JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(negated_digit,
		JSREGEX_U_PREDEFINED_CLASS_NOT_DIGIT)
JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(whitespace,
		JSREGEX_U_PREDEFINED_CLASS_WHITESPACE)
JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(negated_whitespace,
		JSREGEX_U_PREDEFINED_CLASS_NOT_WHITESPACE)
JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(word,
		JSREGEX_U_PREDEFINED_CLASS_WORD)
JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS(negated_word,
		JSREGEX_U_PREDEFINED_CLASS_NOT_WORD)

#undef JSVAL_DEFINE_U_PREDEFINED_CLASS_WRAPPERS

int
jsval_method_string_replace_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_sequence_common(region,
			this_value, pattern, pattern_len, replacement_value, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_sequence_common(region,
			this_value, pattern, pattern_len, replacement_value, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_sequence_fn_common(region,
			this_value, pattern, pattern_len, callback, callback_ctx, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_sequence_fn_common(region,
			this_value, pattern, pattern_len, callback, callback_ctx, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_class_common(region,
			this_value, members, members_len, replacement_value, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_class_common(region,
			this_value, members, members_len, replacement_value, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_class_fn_common(region,
			this_value, members, members_len, callback, callback_ctx, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_class_fn_common(region,
			this_value, members, members_len, callback, callback_ctx, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_range_class_common(region,
			this_value, ranges, range_count, replacement_value, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_range_class_common(region,
			this_value, ranges, range_count, replacement_value, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_range_class_fn_common(
			region, this_value, ranges, range_count, callback,
			callback_ctx, 0, value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_range_class_fn_common(
			region, this_value, ranges, range_count, callback,
			callback_ctx, 1, value_ptr, error);
}

int
jsval_method_string_replace_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_class_common(region,
			this_value, members, members_len, replacement_value, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_class_common(region,
			this_value, members, members_len, replacement_value, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_class_fn_common(
			region, this_value, members, members_len, callback,
			callback_ctx, 0, value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_class_fn_common(
			region, this_value, members, members_len, callback,
			callback_ctx, 1, value_ptr, error);
}

int
jsval_method_string_replace_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_range_class_common(
			region, this_value, ranges, range_count, replacement_value, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_range_class_common(
			region, this_value, ranges, range_count, replacement_value, 1,
			value_ptr, error);
}

int
jsval_method_string_replace_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_range_class_fn_common(
			region, this_value, ranges, range_count, callback,
			callback_ctx, 0, value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_negated_range_class_fn_common(
			region, this_value, ranges, range_count, callback,
			callback_ctx, 1, value_ptr, error);
}

int
jsval_method_string_replace_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_surrogate_common(region,
			this_value, surrogate_unit, replacement_value, 0, value_ptr,
			error);
}

int
jsval_method_string_replace_all_u_literal_surrogate(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_surrogate_common(region,
			this_value, surrogate_unit, replacement_value, 1, value_ptr,
			error);
}

int
jsval_method_string_replace_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_surrogate_fn_common(region,
			this_value, surrogate_unit, callback, callback_ctx, 0,
			value_ptr, error);
}

int
jsval_method_string_replace_all_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_replace_u_literal_surrogate_fn_common(region,
			this_value, surrogate_unit, callback, callback_ctx, 1,
			value_ptr, error);
}
#endif

int
jsval_method_string_replace(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		return jsval_method_string_replace_regex(region, this_value,
				search_value, replacement_value, value_ptr, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_bridge(region, this_value,
			search_value, replacement_value,
			jsmethod_string_replace_measure, jsmethod_string_replace,
			value_ptr, error);
}

int
jsval_method_string_replace_fn(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, jsval_replace_callback_fn callback,
		void *callback_ctx, jsval_t *value_ptr, jsmethod_error_t *error)
{
	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		return jsval_method_string_replace_regex_fn(region, this_value,
				search_value, callback, callback_ctx, 0, value_ptr, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_fn(region, this_value,
			search_value, callback, callback_ctx, 0, value_ptr, error);
}

int
jsval_method_string_replace_all(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	if (region == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		if (jsval_method_string_replace_all_regex_check(region, search_value,
				error) < 0) {
			return -1;
		}
		return jsval_method_string_replace_regex(region, this_value,
				search_value, replacement_value, value_ptr, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_bridge(region, this_value,
			search_value, replacement_value,
			jsmethod_string_replace_all_measure,
			jsmethod_string_replace_all, value_ptr, error);
}

int
jsval_method_string_replace_all_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	if (region == NULL || callback == NULL || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
#if JSMX_WITH_REGEX
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		return jsval_method_string_replace_regex_fn(region, this_value,
				search_value, callback, callback_ctx, 1, value_ptr, error);
	}
#else
	if (search_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_replace_string_fn(region, this_value,
			search_value, callback, callback_ctx, 1, value_ptr, error);
}

int jsval_method_string_repeat(jsval_region_t *region, jsval_t this_value,
		int have_count, jsval_t count_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_repeat_bridge(region, this_value, have_count,
			count_value, jsmethod_string_repeat, value_ptr, error);
}

int
jsval_method_string_pad_start(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_pad_bridge(region, this_value,
			have_max_length, max_length_value, have_fill_string,
			fill_string_value, jsmethod_string_pad_start_measure,
			jsmethod_string_pad_start, value_ptr, error);
}

int
jsval_method_string_pad_end(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_pad_bridge(region, this_value,
			have_max_length, max_length_value, have_fill_string,
			fill_string_value, jsmethod_string_pad_end_measure,
			jsmethod_string_pad_end, value_ptr, error);
}

int
jsval_method_string_char_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_position_bridge(region, this_value,
			have_position, position_value, jsmethod_string_char_at,
			value_ptr, error);
}

int
jsval_method_string_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_optional_bridge(region, this_value,
			have_position, position_value, jsmethod_string_at,
			value_ptr, error);
}

int
jsval_method_string_char_code_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_double_bridge(region, this_value,
			have_position, position_value, jsmethod_string_char_code_at,
			value_ptr, error);
}

int
jsval_method_string_code_point_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_optional_double_bridge(region, this_value,
			have_position, position_value, jsmethod_string_code_point_at,
			value_ptr, error);
}

int
jsval_method_string_slice(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_range_bridge(region, this_value,
			have_start, start_value, have_end, end_value,
			jsmethod_string_slice, value_ptr, error);
}

int
jsval_method_string_substring(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_range_bridge(region, this_value,
			have_start, start_value, have_end, end_value,
			jsmethod_string_substring, value_ptr, error);
}

int
jsval_method_string_substr(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_length, jsval_t length_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_range_bridge(region, this_value,
			have_start, start_value, have_length, length_value,
			jsmethod_string_substr, value_ptr, error);
}

int
jsval_method_string_split(jsval_region_t *region, jsval_t this_value,
		int have_separator, jsval_t separator_value,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
#if JSMX_WITH_REGEX
	if (have_separator && separator_value.kind == JSVAL_KIND_REGEXP) {
		return jsval_method_string_split_regex(region, this_value,
				separator_value, have_limit, limit_value, value_ptr, error);
	}
#else
	if (have_separator && separator_value.kind == JSVAL_KIND_REGEXP) {
		errno = ENOTSUP;
		return -1;
	}
#endif
	return jsval_method_string_split_string(region, this_value,
			have_separator, separator_value, have_limit, limit_value,
			value_ptr, error);
}

int
jsval_method_string_index_of(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_index_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_index_of,
			value_ptr, error);
}

int
jsval_method_string_last_index_of(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_index_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_last_index_of,
			value_ptr, error);
}

int
jsval_method_string_includes(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_includes,
			value_ptr, error);
}

int
jsval_method_string_starts_with(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_position, position_value, jsmethod_string_starts_with,
			value_ptr, error);
}

int
jsval_method_string_ends_with(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_end_position,
		jsval_t end_position_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_bool_bridge(region, this_value, search_value,
			have_end_position, end_position_value,
			jsmethod_string_ends_with, value_ptr, error);
}

#if JSMX_WITH_REGEX
int
jsval_method_string_search_regex(jsval_region_t *region,
		jsval_t this_value, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error)
{
	return jsval_method_string_regex_index_bridge(region, this_value,
			pattern_value, have_flags, flags_value,
			jsmethod_string_search_regex, value_ptr, error);
}
#endif

int jsval_promote_object_shallow_measure(jsval_region_t *region, jsval_t object,
		size_t prop_cap, size_t *bytes_ptr)
{
	size_t start_used;
	size_t used;
	size_t len;

	if (!jsval_region_valid(region) || bytes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_object_size(region, object);
	if (len > prop_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (prop_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*bytes_ptr = 0;
		return 0;
	}
	if (object.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	if (prop_cap > (SIZE_MAX - sizeof(jsval_native_object_t)) /
			sizeof(jsval_native_prop_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	used = region->pages->used_len;
	start_used = used;
	if (jsval_region_measure_reserve(region, &used,
			sizeof(jsval_native_object_t) + prop_cap * sizeof(jsval_native_prop_t),
			JSVAL_ALIGN) < 0) {
		return -1;
	}

	{
		int cursor;
		unsigned int i;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_utf16_len;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf16(region, doc, (uint32_t)key_index, NULL,
					0, &key_utf16_len) < 0) {
				return -1;
			}
			if (jsval_string_measure_utf16(region, &used, key_utf16_len) < 0) {
				return -1;
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}
	}

	*bytes_ptr = used - start_used;
	return 0;
}

int jsval_promote_object_shallow(jsval_region_t *region, jsval_t object,
		size_t prop_cap, jsval_t *value_ptr)
{
	size_t len;

	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (object.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_object_size(region, object);
	if (len > prop_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (object.repr == JSVAL_REPR_NATIVE) {
		jsval_native_object_t *native = jsval_native_object(region, object);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (prop_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*value_ptr = object;
		return 0;
	}
	if (object.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	{
		size_t needed;
		int cursor;
		unsigned int i;
		jsval_t out;
		jsval_json_doc_t *doc = jsval_json_doc(region, object);
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (jsval_promote_object_shallow_measure(region, object, prop_cap,
				&needed) < 0) {
			return -1;
		}
		(void)needed;
		if (jsval_object_new(region, prop_cap, &out) < 0) {
			return -1;
		}

		cursor = (int)object.as.index + 1;
		for (i = 0; i < (unsigned int)tokens[object.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_len;
			jsval_t child;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf8_internal(region, doc,
					(uint32_t)key_index, NULL, 0, &key_len) < 0) {
				return -1;
			}

			{
				uint8_t key_buf[key_len ? key_len : 1];

				if (jsval_json_string_copy_utf8_internal(region, doc,
						(uint32_t)key_index, key_buf, key_len, NULL) < 0) {
					return -1;
				}
				child = jsval_json_value(region, object, (uint32_t)value_index);
				if (jsval_object_set_utf8(region, out, key_buf, key_len, child) < 0) {
					return -1;
				}
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = out;
		return 0;
	}
}

int jsval_promote_object_shallow_in_place(jsval_region_t *region,
		jsval_t *value_ptr, size_t prop_cap)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	return jsval_promote_object_shallow(region, *value_ptr, prop_cap, value_ptr);
}

int jsval_promote_array_shallow_measure(jsval_region_t *region, jsval_t array,
		size_t elem_cap, size_t *bytes_ptr)
{
	size_t len;
	size_t used;
	size_t start_used;

	if (!jsval_region_valid(region) || bytes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_array_length(region, array);
	if (len > elem_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (elem_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*bytes_ptr = 0;
		return 0;
	}
	if (array.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}
	if (elem_cap > (SIZE_MAX - sizeof(jsval_native_array_t)) / sizeof(jsval_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	used = region->pages->used_len;
	start_used = used;
	if (jsval_region_measure_reserve(region, &used,
			sizeof(jsval_native_array_t) + elem_cap * sizeof(jsval_t),
			JSVAL_ALIGN) < 0) {
		return -1;
	}
	*bytes_ptr = used - start_used;
	return 0;
}

int jsval_promote_array_shallow(jsval_region_t *region, jsval_t array,
		size_t elem_cap, jsval_t *value_ptr)
{
	size_t len;

	if (!jsval_region_valid(region) || value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (array.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	len = jsval_array_length(region, array);
	if (len > elem_cap) {
		errno = ENOBUFS;
		return -1;
	}

	if (array.repr == JSVAL_REPR_NATIVE) {
		jsval_native_array_t *native = jsval_native_array(region, array);

		if (native == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (elem_cap > native->cap) {
			errno = ENOBUFS;
			return -1;
		}

		*value_ptr = array;
		return 0;
	}
	if (array.repr != JSVAL_REPR_JSON) {
		errno = EINVAL;
		return -1;
	}

	{
		size_t needed;
		size_t i;
		jsval_t out;

		if (jsval_promote_array_shallow_measure(region, array, elem_cap,
				&needed) < 0) {
			return -1;
		}
		(void)needed;
		if (jsval_array_new(region, elem_cap, &out) < 0) {
			return -1;
		}
		for (i = 0; i < len; i++) {
			jsval_t child;

			if (jsval_array_get(region, array, i, &child) < 0) {
				return -1;
			}
			if (jsval_array_set(region, out, i, child) < 0) {
				return -1;
			}
		}

		*value_ptr = out;
		return 0;
	}
}

int jsval_promote_array_shallow_in_place(jsval_region_t *region,
		jsval_t *value_ptr, size_t elem_cap)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	return jsval_promote_array_shallow(region, *value_ptr, elem_cap, value_ptr);
}

int jsval_promote(jsval_region_t *region, jsval_t value, jsval_t *value_ptr)
{
	jsval_json_doc_t *doc;

	if (value.repr != JSVAL_REPR_JSON) {
		*value_ptr = value;
		return 0;
	}

	doc = jsval_json_doc(region, value);
	switch (value.kind) {
	case JSVAL_KIND_UNDEFINED:
		*value_ptr = jsval_undefined();
		return 0;
	case JSVAL_KIND_NULL:
		*value_ptr = jsval_null();
		return 0;
	case JSVAL_KIND_BOOL:
	{
		int boolean;
		if (jsval_json_bool_value(region, doc, value.as.index, &boolean) < 0) {
			return -1;
		}
		*value_ptr = jsval_bool(boolean);
		return 0;
	}
	case JSVAL_KIND_NUMBER:
	{
		double number;
		if (jsval_json_number_value(region, doc, value.as.index, &number) < 0) {
			return -1;
		}
		*value_ptr = jsval_number(number);
		return 0;
	}
	case JSVAL_KIND_STRING:
	{
		size_t utf8_len;
		if (jsval_json_string_copy_utf8_internal(region, doc, value.as.index, NULL, 0, &utf8_len) < 0) {
			return -1;
		}
		{
			uint8_t buf[utf8_len ? utf8_len : 1];
			if (jsval_json_string_copy_utf8_internal(region, doc, value.as.index, buf, utf8_len, NULL) < 0) {
				return -1;
			}
			return jsval_string_new_utf8(region, buf, utf8_len, value_ptr);
		}
	}
	case JSVAL_KIND_ARRAY:
	{
		size_t len = jsval_array_length(region, value);
		size_t i;
		jsval_t array;

		if (jsval_array_new(region, len, &array) < 0) {
			return -1;
		}
		for (i = 0; i < len; i++) {
			jsval_t child;
			jsval_t promoted;

			if (jsval_array_get(region, value, i, &child) < 0) {
				return -1;
			}
			if (jsval_promote(region, child, &promoted) < 0) {
				return -1;
			}
			if (jsval_array_set(region, array, i, promoted) < 0) {
				return -1;
			}
		}

		*value_ptr = array;
		return 0;
	}
	case JSVAL_KIND_OBJECT:
	{
		size_t len = jsval_object_size(region, value);
		size_t i;
		int cursor;
		jsval_t object_out;
		jsmntok_t *tokens = jsval_json_doc_tokens(region, doc);

		if (jsval_object_new(region, len, &object_out) < 0) {
			return -1;
		}

		cursor = (int)value.as.index + 1;
		for (i = 0; i < (size_t)tokens[value.as.index].size; i++) {
			int key_index = cursor;
			int value_index = jsval_json_next(region, doc, key_index);
			size_t key_len;
			jsval_t child;
			jsval_t promoted;

			if (value_index < 0) {
				errno = EINVAL;
				return -1;
			}
			if (jsval_json_string_copy_utf8_internal(region, doc, (uint32_t)key_index, NULL, 0, &key_len) < 0) {
				return -1;
			}

			{
				uint8_t key_buf[key_len ? key_len : 1];
				if (jsval_json_string_copy_utf8_internal(region, doc, (uint32_t)key_index, key_buf, key_len, NULL) < 0) {
					return -1;
				}
				child = jsval_json_value(region, value, (uint32_t)value_index);
				if (jsval_promote(region, child, &promoted) < 0) {
					return -1;
				}
				if (jsval_object_set_utf8(region, object_out, key_buf, key_len, promoted) < 0) {
					return -1;
				}
			}

			cursor = jsval_json_next(region, doc, value_index);
			if (cursor < 0) {
				errno = EINVAL;
				return -1;
			}
		}

		*value_ptr = object_out;
		return 0;
	}
	default:
		errno = ENOTSUP;
		return -1;
	}
}

int jsval_promote_in_place(jsval_region_t *region, jsval_t *value_ptr)
{
	if (value_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (!jsval_is_json_backed(*value_ptr)) {
		return 0;
	}

	return jsval_promote(region, *value_ptr, value_ptr);
}

int jsval_region_promote_root(jsval_region_t *region, jsval_t *value_ptr)
{
	jsval_t root;

	if (jsval_region_root(region, &root) < 0) {
		return -1;
	}
	if (jsval_promote_in_place(region, &root) < 0) {
		return -1;
	}
	if (jsval_region_set_root(region, root) < 0) {
		return -1;
	}
	if (value_ptr != NULL) {
		*value_ptr = root;
	}
	return 0;
}
