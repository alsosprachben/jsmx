#ifndef JSURL_H
#define JSURL_H

#include <stddef.h>
#include <stdint.h>

#include "jsstr.h"

struct jsval_region_s;
typedef struct jsval_region_s jsval_region_t;

typedef struct jsurl_param_s {
	jsstr8_t name;
	jsstr8_t value;
} jsurl_param_t;

typedef struct jsurl_search_params_view_s {
	jsstr8_t search;
} jsurl_search_params_view_t;

typedef struct jsurl_view_s {
	jsstr8_t href;
	jsstr8_t protocol;
	jsstr8_t username;
	jsstr8_t password;
	jsstr8_t hostname;
	jsstr8_t port;
	jsstr8_t pathname;
	jsstr8_t search;
	jsstr8_t hash;
	jsurl_search_params_view_t search_params;
	int has_authority;
} jsurl_view_t;

struct jsurl_s;

typedef struct jsurl_search_params_s {
	struct jsurl_s *owner;
	jsurl_param_t *params;
	size_t len;
	size_t cap;
	jsstr8_t storage;
	int syncing;
} jsurl_search_params_t;

typedef struct jsurl_search_params_storage_s {
	jsurl_param_t *params;
	size_t params_cap;
	jsstr8_t storage;
} jsurl_search_params_storage_t;

typedef struct jsurl_storage_s {
	jsstr8_t protocol;
	jsstr8_t username;
	jsstr8_t password;
	jsstr8_t hostname;
	jsstr8_t port;
	jsstr8_t pathname;
	jsstr8_t search;
	jsstr8_t hash;
	jsstr8_t host;
	jsstr8_t origin;
	jsstr8_t href;
	jsurl_search_params_storage_t search_params;
} jsurl_storage_t;

typedef struct jsurl_sizes_s {
	size_t protocol_cap;
	size_t username_cap;
	size_t password_cap;
	size_t hostname_cap;
	size_t port_cap;
	size_t pathname_cap;
	size_t search_cap;
	size_t hash_cap;
	size_t host_cap;
	size_t origin_cap;
	size_t href_cap;
	size_t search_params_cap;
	size_t search_params_storage_cap;
} jsurl_sizes_t;

typedef struct jsurl_s {
	jsstr8_t protocol;
	jsstr8_t username;
	jsstr8_t password;
	jsstr8_t hostname;
	jsstr8_t port;
	jsstr8_t pathname;
	jsstr8_t search;
	jsstr8_t hash;
	jsstr8_t host;
	jsstr8_t origin;
	jsstr8_t href;
	jsurl_search_params_t search_params;
	int has_authority;
} jsurl_t;

void jsurl_view_clear(jsurl_view_t *view);
int jsurl_view_parse(jsurl_view_t *view, jsstr8_t input);
int jsurl_view_hostname_display_measure(const jsurl_view_t *view,
		size_t *len_ptr);
int jsurl_view_hostname_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_host_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_host_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr);
int jsurl_view_host_display_measure(const jsurl_view_t *view,
		size_t *len_ptr);
int jsurl_view_host_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_origin_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_origin_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr);
int jsurl_view_origin_display_measure(const jsurl_view_t *view,
		size_t *len_ptr);
int jsurl_view_origin_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_pathname_wire_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_pathname_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_search_wire_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_search_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_hash_wire_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_hash_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr);
int jsurl_view_href_measure(const jsurl_view_t *view, size_t *len_ptr);
int jsurl_view_href_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr);
jsstr8_t jsurl_view_to_json(const jsurl_view_t *view);
int jsurl_copy_sizes(const jsurl_view_t *view, jsurl_sizes_t *sizes_ptr);
int jsurl_copy_sizes_with_base(jsstr8_t input, const jsurl_t *base,
		jsurl_sizes_t *sizes_ptr);

void jsurl_search_params_view_init(jsurl_search_params_view_t *view,
		jsstr8_t search);
size_t jsurl_search_params_view_size(const jsurl_search_params_view_t *view);
int jsurl_search_params_view_measure(const jsurl_search_params_view_t *view,
		size_t *len_ptr);
int jsurl_search_params_view_serialize(const jsurl_search_params_view_t *view,
		jsstr8_t *result_ptr);

int jsurl_search_params_init(jsurl_search_params_t *params,
		const jsurl_search_params_storage_t *storage);
void jsurl_search_params_clear(jsurl_search_params_t *params);
size_t jsurl_search_params_size(const jsurl_search_params_t *params);
int jsurl_search_params_parse(jsurl_search_params_t *params, jsstr8_t search);
int jsurl_search_params_append(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value);
int jsurl_search_params_delete(jsurl_search_params_t *params, jsstr8_t name);
int jsurl_search_params_delete_value(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value);
int jsurl_search_params_get(const jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t *value_ptr, int *found_ptr);
int jsurl_search_params_get_all(const jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t *values, size_t *len_ptr, size_t *found_ptr);
int jsurl_search_params_has(const jsurl_search_params_t *params, jsstr8_t name,
		int *found_ptr);
int jsurl_search_params_set(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value);
int jsurl_search_params_sort(jsurl_search_params_t *params);
int jsurl_search_params_measure(const jsurl_search_params_t *params,
		size_t *len_ptr);
int jsurl_search_params_serialize(const jsurl_search_params_t *params,
		jsstr8_t *result_ptr);

int jsurl_init(jsurl_t *url, const jsurl_storage_t *storage);
void jsurl_clear(jsurl_t *url);
int jsurl_copy_from_view(jsurl_t *url, const jsurl_view_t *view);
int jsurl_parse_copy(jsurl_t *url, jsstr8_t input);
int jsurl_parse_copy_with_base(jsurl_t *url, jsstr8_t input, const jsurl_t *base);
int jsurl_region_parse_copy_measure(const jsval_region_t *region, jsstr8_t input,
		size_t *bytes_ptr);
int jsurl_region_parse_copy(jsval_region_t *region, jsstr8_t input,
		jsurl_t *url_ptr);
int jsurl_region_parse_copy_with_base_measure(const jsval_region_t *region,
		jsstr8_t input, const jsurl_t *base, size_t *bytes_ptr);
int jsurl_region_parse_copy_with_base(jsval_region_t *region, jsstr8_t input,
		const jsurl_t *base, jsurl_t *url_ptr);
int jsurl_parse(jsurl_t *url, jsstr8_t input);
int jsurl_parse_with_base(jsurl_t *url, jsstr8_t input, const jsurl_t *base);

int jsurl_set_protocol(jsurl_t *url, jsstr8_t protocol);
int jsurl_set_username(jsurl_t *url, jsstr8_t username);
int jsurl_set_password(jsurl_t *url, jsstr8_t password);
int jsurl_set_hostname(jsurl_t *url, jsstr8_t hostname);
int jsurl_set_port(jsurl_t *url, jsstr8_t port);
int jsurl_set_pathname(jsurl_t *url, jsstr8_t pathname);
int jsurl_set_search(jsurl_t *url, jsstr8_t search);
int jsurl_set_hash(jsurl_t *url, jsstr8_t hash);

int jsurl_pathname_wire_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_pathname_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_search_wire_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_search_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_hash_wire_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_hash_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_href_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_href_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_hostname_display_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_hostname_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_host_display_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_host_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_origin_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_origin_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
int jsurl_origin_display_measure(const jsurl_t *url, size_t *len_ptr);
int jsurl_origin_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr);
jsstr8_t jsurl_to_json(const jsurl_t *url);

#endif
