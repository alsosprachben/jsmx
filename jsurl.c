#include <errno.h>
#include <string.h>

#include "jsurl.h"
#include "utf8.h"

typedef struct jsurl_parts_s {
	jsstr8_t protocol;
	jsstr8_t username;
	jsstr8_t password;
	jsstr8_t hostname;
	jsstr8_t port;
	jsstr8_t pathname;
	jsstr8_t search;
	jsstr8_t hash;
	int has_authority;
} jsurl_parts_t;

static int jsurl_search_params_requirements(jsstr8_t search, size_t *params_len_ptr,
		size_t *storage_len_ptr);

static jsstr8_t jsurl_slice(jsstr8_t src, size_t start_i, ssize_t stop_i)
{
	jsstr8_t out;
	size_t stop_pos;

	jsstr8_init(&out);
	if (start_i > src.len) {
		start_i = src.len;
	}
	stop_pos = stop_i < 0 ? src.len : (size_t) stop_i;
	if (stop_pos > src.len) {
		stop_pos = src.len;
	}
	if (stop_pos < start_i) {
		stop_pos = start_i;
	}
	out.bytes = src.bytes ? src.bytes + start_i : NULL;
	out.len = stop_pos - start_i;
	out.cap = out.len;
	return out;
}

static int jsurl_str_cmp(jsstr8_t left, jsstr8_t right)
{
	size_t i;
	size_t min_len = left.len < right.len ? left.len : right.len;

	for (i = 0; i < min_len; i++) {
		if (left.bytes[i] != right.bytes[i]) {
			return left.bytes[i] - right.bytes[i];
		}
	}
	if (left.len < right.len) {
		return -1;
	}
	if (left.len > right.len) {
		return 1;
	}
	return 0;
}

static int jsurl_str_eq(jsstr8_t left, jsstr8_t right)
{
	return jsurl_str_cmp(left, right) == 0;
}

static int jsurl_str_eq_literal(jsstr8_t left, const char *literal)
{
	jsstr8_t right;

	jsstr8_init_from_str(&right, literal);
	return jsurl_str_eq(left, right);
}

static ssize_t jsurl_find_byte(jsstr8_t src, uint8_t needle, size_t start_i)
{
	size_t i;

	for (i = start_i; i < src.len; i++) {
		if (src.bytes[i] == needle) {
			return (ssize_t) i;
		}
	}
	return -1;
}

static ssize_t jsurl_find_last_byte(jsstr8_t src, uint8_t needle)
{
	size_t i;

	for (i = src.len; i > 0; i--) {
		if (src.bytes[i - 1] == needle) {
			return (ssize_t) (i - 1);
		}
	}
	return -1;
}

static ssize_t jsurl_find_first_of(jsstr8_t src, const char *needles,
		size_t start_i)
{
	size_t i;

	for (i = start_i; i < src.len; i++) {
		const char *needle;

		for (needle = needles; *needle != '\0'; needle++) {
			if (src.bytes[i] == (uint8_t) *needle) {
				return (ssize_t) i;
			}
		}
	}
	return -1;
}

static int jsurl_storage_valid(jsstr8_t storage)
{
	return storage.cap == 0 || storage.bytes != NULL;
}

static int jsurl_buffer_copy(jsstr8_t *dst, jsstr8_t src)
{
	if (dst == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (src.len > dst->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (src.len > 0) {
		memmove(dst->bytes, src.bytes, src.len);
	}
	dst->len = src.len;
	return 0;
}

static int jsurl_buffer_copy_literal(jsstr8_t *dst, const char *literal)
{
	jsstr8_t src;

	jsstr8_init_from_str(&src, literal);
	return jsurl_buffer_copy(dst, src);
}

static void jsurl_parts_clear(jsurl_parts_t *parts)
{
	memset(parts, 0, sizeof(*parts));
}

static void jsurl_view_parts(const jsurl_view_t *view, jsurl_parts_t *parts)
{
	parts->protocol = view->protocol;
	parts->username = view->username;
	parts->password = view->password;
	parts->hostname = view->hostname;
	parts->port = view->port;
	parts->pathname = view->pathname;
	parts->search = view->search;
	parts->hash = view->hash;
	parts->has_authority = view->has_authority;
}

static void jsurl_current_parts(const jsurl_t *url, jsurl_parts_t *parts)
{
	parts->protocol = url->protocol;
	parts->username = url->username;
	parts->password = url->password;
	parts->hostname = url->hostname;
	parts->port = url->port;
	parts->pathname = url->pathname;
	parts->search = url->search;
	parts->hash = url->hash;
	parts->has_authority = url->has_authority;
}

static int jsurl_is_alpha(uint8_t c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int jsurl_is_digit(uint8_t c)
{
	return c >= '0' && c <= '9';
}

static int jsurl_is_scheme_char(uint8_t c)
{
	return jsurl_is_alpha(c) || jsurl_is_digit(c)
		|| c == '+' || c == '-' || c == '.';
}

static ssize_t jsurl_find_scheme_end(jsstr8_t input)
{
	size_t i;

	if (input.len == 0 || !jsurl_is_alpha(input.bytes[0])) {
		return -1;
	}
	for (i = 1; i < input.len; i++) {
		if (input.bytes[i] == ':') {
			return (ssize_t) i;
		}
		if (input.bytes[i] == '/' || input.bytes[i] == '?'
				|| input.bytes[i] == '#') {
			return -1;
		}
		if (!jsurl_is_scheme_char(input.bytes[i])) {
			return -1;
		}
	}
	return -1;
}

static int jsurl_protocol_is_special_origin(jsstr8_t protocol)
{
	return jsurl_str_eq_literal(protocol, "http:")
		|| jsurl_str_eq_literal(protocol, "https:")
		|| jsurl_str_eq_literal(protocol, "ws:")
		|| jsurl_str_eq_literal(protocol, "wss:")
		|| jsurl_str_eq_literal(protocol, "ftp:");
}

static size_t jsurl_host_len(const jsurl_parts_t *parts)
{
	if (parts->hostname.len == 0) {
		return 0;
	}
	return parts->hostname.len + (parts->port.len > 0 ? 1 + parts->port.len : 0);
}

static size_t jsurl_href_path_len(const jsurl_parts_t *parts)
{
	if (parts->has_authority && parts->pathname.len == 0) {
		return 1;
	}
	return parts->pathname.len;
}

static int jsurl_measure_derived(const jsurl_parts_t *parts, size_t *host_len_ptr,
		size_t *origin_len_ptr, size_t *href_len_ptr)
{
	size_t host_len = jsurl_host_len(parts);
	size_t origin_len;
	size_t href_len = parts->protocol.len + parts->search.len + parts->hash.len;

	if (parts->has_authority) {
		href_len += 2;
		if (parts->username.len > 0 || parts->password.len > 0) {
			href_len += parts->username.len;
			if (parts->password.len > 0) {
				href_len += 1 + parts->password.len;
			}
			href_len += 1;
		}
		href_len += host_len;
		href_len += jsurl_href_path_len(parts);
	} else {
		href_len += parts->pathname.len;
	}

	if (jsurl_protocol_is_special_origin(parts->protocol) && parts->has_authority) {
		origin_len = parts->protocol.len + 2 + host_len;
	} else {
		origin_len = 4;
	}

	if (host_len_ptr != NULL) {
		*host_len_ptr = host_len;
	}
	if (origin_len_ptr != NULL) {
		*origin_len_ptr = origin_len;
	}
	if (href_len_ptr != NULL) {
		*href_len_ptr = href_len;
	}
	return 0;
}

static int jsurl_copy_sizes_from_parts(const jsurl_parts_t *parts,
		jsurl_sizes_t *sizes_ptr)
{
	size_t params_need;
	size_t storage_need;

	if (sizes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_measure_derived(parts, &sizes_ptr->host_cap,
			&sizes_ptr->origin_cap, &sizes_ptr->href_cap) < 0) {
		return -1;
	}
	if (jsurl_search_params_requirements(parts->search, &params_need,
			&storage_need) < 0) {
		return -1;
	}
	sizes_ptr->protocol_cap = parts->protocol.len;
	sizes_ptr->username_cap = parts->username.len;
	sizes_ptr->password_cap = parts->password.len;
	sizes_ptr->hostname_cap = parts->hostname.len;
	sizes_ptr->port_cap = parts->port.len;
	sizes_ptr->pathname_cap = parts->pathname.len;
	sizes_ptr->search_cap = parts->search.len;
	sizes_ptr->hash_cap = parts->hash.len;
	sizes_ptr->search_params_cap = params_need;
	sizes_ptr->search_params_storage_cap = storage_need;
	return 0;
}

static int jsurl_is_hex_digit(uint8_t c)
{
	return (c >= '0' && c <= '9')
		|| (c >= 'A' && c <= 'F')
		|| (c >= 'a' && c <= 'f');
}

static uint8_t jsurl_hex_value(uint8_t c)
{
	if (c >= '0' && c <= '9') {
		return (uint8_t) (c - '0');
	}
	if (c >= 'A' && c <= 'F') {
		return (uint8_t) (10 + c - 'A');
	}
	return (uint8_t) (10 + c - 'a');
}

static size_t jsurl_form_decoded_len(jsstr8_t src)
{
	size_t i = 0;
	size_t out_len = 0;

	while (i < src.len) {
		if (src.bytes[i] == '%' && i + 2 < src.len
				&& jsurl_is_hex_digit(src.bytes[i + 1])
				&& jsurl_is_hex_digit(src.bytes[i + 2])) {
			i += 3;
			out_len += 1;
		} else {
			i += 1;
			out_len += 1;
		}
	}
	return out_len;
}

static int jsurl_form_decode_into(jsstr8_t src, uint8_t *dst, size_t *len_ptr)
{
	size_t i = 0;
	size_t out_len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	while (i < src.len) {
		if (src.bytes[i] == '+') {
			dst[out_len++] = ' ';
			i++;
			continue;
		}
		if (src.bytes[i] == '%' && i + 2 < src.len
				&& jsurl_is_hex_digit(src.bytes[i + 1])
				&& jsurl_is_hex_digit(src.bytes[i + 2])) {
			dst[out_len++] = (uint8_t) ((jsurl_hex_value(src.bytes[i + 1]) << 4)
				| jsurl_hex_value(src.bytes[i + 2]));
			i += 3;
			continue;
		}
		dst[out_len++] = src.bytes[i++];
	}
	*len_ptr = out_len;
	return 0;
}

static int jsurl_form_needs_encode(uint8_t c)
{
	if (jsurl_is_alpha(c) || jsurl_is_digit(c)) {
		return 0;
	}
	return !(c == '*' || c == '-' || c == '.' || c == '_');
}

static size_t jsurl_form_encoded_len(jsstr8_t src)
{
	size_t i;
	size_t out_len = 0;

	for (i = 0; i < src.len; i++) {
		if (src.bytes[i] == ' ') {
			out_len += 1;
		} else if (jsurl_form_needs_encode(src.bytes[i])) {
			out_len += 3;
		} else {
			out_len += 1;
		}
	}
	return out_len;
}

static int jsurl_form_encode_into(jsstr8_t src, uint8_t *dst, size_t *len_ptr)
{
	size_t i;
	size_t out_len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < src.len; i++) {
		if (src.bytes[i] == ' ') {
			dst[out_len++] = '+';
		} else if (jsurl_form_needs_encode(src.bytes[i])) {
			dst[out_len++] = '%';
			dst[out_len++] = VALHEX((src.bytes[i] >> 4) & 0x0f);
			dst[out_len++] = VALHEX(src.bytes[i] & 0x0f);
		} else {
			dst[out_len++] = src.bytes[i];
		}
	}
	*len_ptr = out_len;
	return 0;
}

static jsstr8_t jsurl_query_body(jsstr8_t search)
{
	if (search.len > 0 && search.bytes[0] == '?') {
		return jsurl_slice(search, 1, -1);
	}
	return search;
}

static int jsurl_search_params_requirements(jsstr8_t search, size_t *params_len_ptr,
		size_t *storage_len_ptr)
{
	jsstr8_t body = jsurl_query_body(search);
	size_t params_len = 0;
	size_t storage_len = 0;
	size_t start_i = 0;

	if (params_len_ptr == NULL || storage_len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	while (start_i <= body.len && body.len > 0) {
		ssize_t amp_i = jsurl_find_byte(body, '&', start_i);
		size_t end_i = amp_i >= 0 ? (size_t) amp_i : body.len;
		jsstr8_t segment = jsurl_slice(body, start_i, (ssize_t) end_i);

		if (segment.len > 0) {
			ssize_t eq_i = jsurl_find_byte(segment, '=', 0);
			jsstr8_t name = eq_i >= 0
				? jsurl_slice(segment, 0, eq_i)
				: segment;
			jsstr8_t value = eq_i >= 0
				? jsurl_slice(segment, (size_t) eq_i + 1, -1)
				: jsstr8_empty;

			params_len += 1;
			storage_len += jsurl_form_decoded_len(name);
			storage_len += jsurl_form_decoded_len(value);
		}
		if (amp_i < 0) {
			break;
		}
		start_i = (size_t) amp_i + 1;
	}
	*params_len_ptr = params_len;
	*storage_len_ptr = storage_len;
	return 0;
}

static size_t jsurl_storage_offset(const jsurl_search_params_t *params, jsstr8_t value)
{
	if (params->storage.bytes == NULL || value.bytes == NULL) {
		return 0;
	}
	return (size_t) (value.bytes - params->storage.bytes);
}

static int jsurl_search_params_copy_state(jsurl_search_params_t *dst,
		const jsurl_search_params_t *src)
{
	size_t i;

	if (dst == NULL || src == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (src->len > dst->cap || src->storage.len > dst->storage.cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (src->storage.len > 0) {
		memmove(dst->storage.bytes, src->storage.bytes, src->storage.len);
	}
	dst->storage.len = src->storage.len;
	dst->len = src->len;
	dst->owner = src->owner;
	dst->syncing = src->syncing;
	for (i = 0; i < src->len; i++) {
		size_t name_off = jsurl_storage_offset(src, src->params[i].name);
		size_t value_off = jsurl_storage_offset(src, src->params[i].value);

		dst->params[i].name.bytes = dst->storage.bytes != NULL
			? dst->storage.bytes + name_off
			: NULL;
		dst->params[i].name.len = src->params[i].name.len;
		dst->params[i].name.cap = src->params[i].name.len;

		dst->params[i].value.bytes = dst->storage.bytes != NULL
			? dst->storage.bytes + value_off
			: NULL;
		dst->params[i].value.len = src->params[i].value.len;
		dst->params[i].value.cap = src->params[i].value.len;
	}
	return 0;
}

static int jsurl_search_params_append_no_sync(jsurl_search_params_t *params,
		jsstr8_t name, jsstr8_t value)
{
	jsurl_param_t *param;

	if (params->len >= params->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (params->storage.len + name.len + value.len > params->storage.cap) {
		errno = ENOBUFS;
		return -1;
	}

	param = &params->params[params->len];
	param->name.bytes = params->storage.bytes != NULL
		? params->storage.bytes + params->storage.len
		: NULL;
	if (name.len > 0) {
		memmove(param->name.bytes, name.bytes, name.len);
	}
	param->name.len = name.len;
	param->name.cap = name.len;
	params->storage.len += name.len;

	param->value.bytes = params->storage.bytes != NULL
		? params->storage.bytes + params->storage.len
		: NULL;
	if (value.len > 0) {
		memmove(param->value.bytes, value.bytes, value.len);
	}
	param->value.len = value.len;
	param->value.cap = value.len;
	params->storage.len += value.len;

	params->len += 1;
	return 0;
}

static int jsurl_search_params_parse_no_sync(jsurl_search_params_t *params,
		jsstr8_t search)
{
	jsstr8_t body = jsurl_query_body(search);
	size_t params_need;
	size_t storage_need;
	size_t start_i = 0;

	if (jsurl_search_params_requirements(search, &params_need, &storage_need) < 0) {
		return -1;
	}
	if (params_need > params->cap || storage_need > params->storage.cap) {
		errno = ENOBUFS;
		return -1;
	}

	jsurl_search_params_clear(params);
	while (start_i <= body.len && body.len > 0) {
		ssize_t amp_i = jsurl_find_byte(body, '&', start_i);
		size_t end_i = amp_i >= 0 ? (size_t) amp_i : body.len;
		jsstr8_t segment = jsurl_slice(body, start_i, (ssize_t) end_i);

		if (segment.len > 0) {
			ssize_t eq_i = jsurl_find_byte(segment, '=', 0);
			jsstr8_t name_src = eq_i >= 0
				? jsurl_slice(segment, 0, eq_i)
				: segment;
			jsstr8_t value_src = eq_i >= 0
				? jsurl_slice(segment, (size_t) eq_i + 1, -1)
				: jsstr8_empty;
			size_t name_len = 0;
			size_t value_len = 0;
			jsstr8_t name_decoded;
			jsstr8_t value_decoded;

			name_decoded.bytes = params->storage.bytes != NULL
				? params->storage.bytes + params->storage.len
				: NULL;
			name_decoded.len = 0;
			name_decoded.cap = params->storage.cap - params->storage.len;
			if (jsurl_form_decode_into(name_src, name_decoded.bytes, &name_len) < 0) {
				return -1;
			}
			name_decoded.len = name_len;
			name_decoded.cap = name_len;
			params->storage.len += name_len;

			value_decoded.bytes = params->storage.bytes != NULL
				? params->storage.bytes + params->storage.len
				: NULL;
			value_decoded.len = 0;
			value_decoded.cap = params->storage.cap - params->storage.len;
			if (jsurl_form_decode_into(value_src, value_decoded.bytes, &value_len) < 0) {
				return -1;
			}
			value_decoded.len = value_len;
			value_decoded.cap = value_len;
			params->storage.len += value_len;

			params->params[params->len].name = name_decoded;
			params->params[params->len].value = value_decoded;
			params->len += 1;
		}
		if (amp_i < 0) {
			break;
		}
		start_i = (size_t) amp_i + 1;
	}
	return 0;
}

static int jsurl_preview_sync_search_len(const jsurl_t *url, size_t search_len)
{
	jsurl_parts_t parts;
	size_t host_len;
	size_t origin_len;
	size_t href_len;

	if (search_len > url->search.cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.search.len = search_len;
	if (jsurl_measure_derived(&parts, &host_len, &origin_len, &href_len) < 0) {
		return -1;
	}
	if (host_len > url->host.cap || origin_len > url->origin.cap
			|| href_len > url->href.cap) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

static int jsurl_search_params_preview_owner_sync(
		const jsurl_search_params_t *preview)
{
	size_t query_len;

	if (preview->owner == NULL) {
		return 0;
	}
	if (jsurl_search_params_measure(preview, &query_len) < 0) {
		return -1;
	}
	return jsurl_preview_sync_search_len(preview->owner,
		query_len > 0 ? query_len + 1 : 0);
}

static int jsurl_update_derived(jsurl_t *url)
{
	jsurl_parts_t parts;
	size_t host_len;
	size_t origin_len;
	size_t href_len;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	if (jsurl_measure_derived(&parts, &host_len, &origin_len, &href_len) < 0) {
		return -1;
	}
	if (host_len > url->host.cap || origin_len > url->origin.cap
			|| href_len > url->href.cap) {
		errno = ENOBUFS;
		return -1;
	}

	url->host.len = 0;
	if (url->hostname.len > 0) {
		jsurl_buffer_copy(&url->host, url->hostname);
		if (url->port.len > 0) {
			url->host.bytes[url->host.len++] = ':';
			memmove(url->host.bytes + url->host.len, url->port.bytes, url->port.len);
			url->host.len += url->port.len;
		}
	}

	url->origin.len = 0;
	if (jsurl_protocol_is_special_origin(url->protocol) && url->has_authority) {
		jsurl_buffer_copy(&url->origin, url->protocol);
		url->origin.bytes[url->origin.len++] = '/';
		url->origin.bytes[url->origin.len++] = '/';
		if (url->host.len > 0) {
			memmove(url->origin.bytes + url->origin.len, url->host.bytes,
				url->host.len);
			url->origin.len += url->host.len;
		}
	} else {
		jsurl_buffer_copy_literal(&url->origin, "null");
	}

	url->href.len = 0;
	if (url->protocol.len > 0) {
		memmove(url->href.bytes + url->href.len, url->protocol.bytes,
			url->protocol.len);
		url->href.len += url->protocol.len;
	}
	if (url->has_authority) {
		url->href.bytes[url->href.len++] = '/';
		url->href.bytes[url->href.len++] = '/';
		if (url->username.len > 0 || url->password.len > 0) {
			if (url->username.len > 0) {
				memmove(url->href.bytes + url->href.len, url->username.bytes,
					url->username.len);
				url->href.len += url->username.len;
			}
			if (url->password.len > 0) {
				url->href.bytes[url->href.len++] = ':';
				memmove(url->href.bytes + url->href.len, url->password.bytes,
					url->password.len);
				url->href.len += url->password.len;
			}
			url->href.bytes[url->href.len++] = '@';
		}
		if (url->host.len > 0) {
			memmove(url->href.bytes + url->href.len, url->host.bytes,
				url->host.len);
			url->href.len += url->host.len;
		}
		if (url->pathname.len > 0) {
			memmove(url->href.bytes + url->href.len, url->pathname.bytes,
				url->pathname.len);
			url->href.len += url->pathname.len;
		} else {
			url->href.bytes[url->href.len++] = '/';
		}
	} else if (url->pathname.len > 0) {
		memmove(url->href.bytes + url->href.len, url->pathname.bytes,
			url->pathname.len);
		url->href.len += url->pathname.len;
	}
	if (url->search.len > 0) {
		memmove(url->href.bytes + url->href.len, url->search.bytes,
			url->search.len);
		url->href.len += url->search.len;
	}
	if (url->hash.len > 0) {
		memmove(url->href.bytes + url->href.len, url->hash.bytes, url->hash.len);
		url->href.len += url->hash.len;
	}
	return 0;
}

static int jsurl_sync_search_from_params(jsurl_t *url)
{
	size_t query_len;
	size_t written_len;
	jsstr8_t query_out;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_measure(&url->search_params, &query_len) < 0) {
		return -1;
	}
	if (jsurl_preview_sync_search_len(url, query_len > 0 ? query_len + 1 : 0) < 0) {
		return -1;
	}

	if (query_len == 0) {
		url->search.len = 0;
	} else {
		url->search.bytes[0] = '?';
		query_out.bytes = url->search.bytes + 1;
		query_out.len = 0;
		query_out.cap = url->search.cap - 1;
		if (jsurl_search_params_serialize(&url->search_params, &query_out) < 0) {
			return -1;
		}
		written_len = query_out.len;
		url->search.len = written_len + 1;
	}
	return jsurl_update_derived(url);
}

static int jsurl_search_params_apply_preview(jsurl_search_params_t *params,
		const jsurl_search_params_t *preview)
{
	if (jsurl_search_params_copy_state(params, preview) < 0) {
		return -1;
	}
	if (params->owner != NULL) {
		return jsurl_sync_search_from_params(params->owner);
	}
	return 0;
}

static int jsurl_search_params_init_preview(jsurl_search_params_t *preview,
		const jsurl_search_params_t *params, jsurl_param_t *param_buf,
		uint8_t *storage_buf)
{
	jsurl_search_params_storage_t storage;

	storage.params = param_buf;
	storage.params_cap = params->cap;
	jsstr8_init_from_buf(&storage.storage, (const char *) storage_buf,
		params->storage.cap);
	if (jsurl_search_params_init(preview, &storage) < 0) {
		return -1;
	}
	preview->owner = params->owner;
	return 0;
}

static void jsurl_split_path_search_hash(jsstr8_t input, jsstr8_t *path_ptr,
		jsstr8_t *search_ptr, jsstr8_t *hash_ptr)
{
	ssize_t hash_i = jsurl_find_byte(input, '#', 0);
	ssize_t query_i = jsurl_find_byte(input, '?', 0);
	size_t path_end = input.len;
	size_t search_end = input.len;

	if (hash_i >= 0) {
		*hash_ptr = jsurl_slice(input, (size_t) hash_i, -1);
		path_end = (size_t) hash_i;
		search_end = (size_t) hash_i;
	} else {
		*hash_ptr = jsstr8_empty;
	}
	if (query_i >= 0 && (hash_i < 0 || query_i < hash_i)) {
		*search_ptr = jsurl_slice(input, (size_t) query_i, (ssize_t) search_end);
		path_end = (size_t) query_i;
	} else {
		*search_ptr = jsstr8_empty;
	}
	*path_ptr = jsurl_slice(input, 0, (ssize_t) path_end);
}

static int jsurl_parse_authority(jsstr8_t authority, jsurl_parts_t *parts)
{
	jsstr8_t hostport = authority;
	ssize_t at_i = jsurl_find_last_byte(authority, '@');

	parts->username = jsstr8_empty;
	parts->password = jsstr8_empty;
	parts->hostname = jsstr8_empty;
	parts->port = jsstr8_empty;

	if (at_i >= 0) {
		jsstr8_t userinfo = jsurl_slice(authority, 0, at_i);
		ssize_t colon_i = jsurl_find_byte(userinfo, ':', 0);

		hostport = jsurl_slice(authority, (size_t) at_i + 1, -1);
		if (colon_i >= 0) {
			parts->username = jsurl_slice(userinfo, 0, colon_i);
			parts->password = jsurl_slice(userinfo, (size_t) colon_i + 1, -1);
		} else {
			parts->username = userinfo;
		}
	}

	if (hostport.len > 0 && hostport.bytes[0] == '[') {
		ssize_t close_i = jsurl_find_byte(hostport, ']', 0);

		if (close_i >= 0) {
			parts->hostname = jsurl_slice(hostport, 0, close_i + 1);
			if ((size_t) close_i + 1 < hostport.len
					&& hostport.bytes[close_i + 1] == ':') {
				parts->port = jsurl_slice(hostport, (size_t) close_i + 2, -1);
			}
			return 0;
		}
	}

	{
		ssize_t colon_i = jsurl_find_last_byte(hostport, ':');

		if (colon_i >= 0) {
			parts->hostname = jsurl_slice(hostport, 0, colon_i);
			parts->port = jsurl_slice(hostport, (size_t) colon_i + 1, -1);
		} else {
			parts->hostname = hostport;
		}
	}
	return 0;
}

static int jsurl_parse_absolute_parts(jsstr8_t input, jsurl_parts_t *parts)
{
	ssize_t scheme_i = jsurl_find_scheme_end(input);
	size_t rest_i;

	if (scheme_i < 0) {
		errno = EINVAL;
		return -1;
	}
	jsurl_parts_clear(parts);
	parts->protocol = jsurl_slice(input, 0, scheme_i + 1);
	rest_i = (size_t) scheme_i + 1;

	if (rest_i + 1 < input.len && input.bytes[rest_i] == '/'
			&& input.bytes[rest_i + 1] == '/') {
		size_t authority_i = rest_i + 2;
		ssize_t authority_end = jsurl_find_first_of(input, "/?#", authority_i);
		jsstr8_t authority = authority_end >= 0
			? jsurl_slice(input, authority_i, authority_end)
			: jsurl_slice(input, authority_i, -1);
		jsstr8_t suffix = authority_end >= 0
			? jsurl_slice(input, (size_t) authority_end, -1)
			: jsstr8_empty;

		parts->has_authority = 1;
		if (jsurl_parse_authority(authority, parts) < 0) {
			return -1;
		}
		jsurl_split_path_search_hash(suffix, &parts->pathname, &parts->search,
			&parts->hash);
		if (parts->pathname.len == 0) {
			jsstr8_init_from_str(&parts->pathname, "/");
		}
		return 0;
	}

	parts->has_authority = 0;
	jsurl_split_path_search_hash(jsurl_slice(input, rest_i, -1), &parts->pathname,
		&parts->search, &parts->hash);
	return 0;
}

static size_t jsurl_base_dir_len(jsstr8_t pathname, int has_authority)
{
	ssize_t slash_i = jsurl_find_last_byte(pathname, '/');

	if (slash_i >= 0) {
		return (size_t) slash_i + 1;
	}
	return has_authority ? 1 : 0;
}

static int jsurl_normalize_path(jsstr8_t input, int absolute_default,
		jsstr8_t *result_ptr)
{
	size_t cap = input.len > 0 ? input.len : 1;
	size_t seg_count = input.len + 1;
	size_t starts[seg_count];
	size_t lens[seg_count];
	size_t count = 0;
	size_t i = 0;
	int absolute = input.len > 0 && input.bytes[0] == '/';
	int trailing_slash = input.len > 0 && input.bytes[input.len - 1] == '/';
	size_t out_len = 0;
	size_t out_i;

	while (i < input.len) {
		size_t start_i = i;
		size_t len;

		while (i < input.len && input.bytes[i] == '/') {
			i++;
		}
		start_i = i;
		while (i < input.len && input.bytes[i] != '/') {
			i++;
		}
		len = i - start_i;
		if (len == 0) {
			continue;
		}
		if (len == 1 && input.bytes[start_i] == '.') {
			continue;
		}
		if (len == 2 && input.bytes[start_i] == '.'
				&& input.bytes[start_i + 1] == '.') {
			if (count > 0
					&& !(lens[count - 1] == 2
					&& input.bytes[starts[count - 1]] == '.'
					&& input.bytes[starts[count - 1] + 1] == '.')) {
				count--;
			} else if (!absolute) {
				starts[count] = start_i;
				lens[count] = len;
				count++;
			}
			continue;
		}
		starts[count] = start_i;
		lens[count] = len;
		count++;
	}

	if (absolute) {
		out_len = 1;
	}
	for (i = 0; i < count; i++) {
		out_len += lens[i];
		if ((absolute && i + 1 <= count) || (!absolute && i > 0)) {
			out_len += 1;
		}
	}
	if (count > 0 && absolute) {
		out_len -= 1;
	}
	if (count > 0 && trailing_slash) {
		out_len += 1;
	}
	if (out_len == 0 && absolute_default) {
		out_len = 1;
	}
	if (out_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}

	out_i = 0;
	if (absolute) {
		result_ptr->bytes[out_i++] = '/';
	}
	for (i = 0; i < count; i++) {
		if (out_i > 0 && result_ptr->bytes[out_i - 1] != '/') {
			result_ptr->bytes[out_i++] = '/';
		}
		memmove(result_ptr->bytes + out_i, input.bytes + starts[i], lens[i]);
		out_i += lens[i];
	}
	if (count > 0 && trailing_slash && (out_i == 0 || result_ptr->bytes[out_i - 1] != '/')) {
		result_ptr->bytes[out_i++] = '/';
	}
	if (out_i == 0 && absolute_default) {
		result_ptr->bytes[out_i++] = '/';
	}
	result_ptr->len = out_i;
	if (result_ptr->len == 0 && cap == 1 && absolute_default && result_ptr->bytes != NULL) {
		result_ptr->bytes[0] = '/';
		result_ptr->len = 1;
	}
	return 0;
}

static int jsurl_normalize_protocol_copy(jsstr8_t *result_ptr, jsstr8_t input)
{
	size_t i;
	size_t len;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (input.len == 0) {
		errno = EINVAL;
		return -1;
	}
	len = input.len;
	if (input.bytes[input.len - 1] != ':') {
		len += 1;
	}
	if (len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (!jsurl_is_alpha(input.bytes[0])) {
		errno = EINVAL;
		return -1;
	}
	for (i = 1; i < input.len; i++) {
		if (input.bytes[i] == ':') {
			if (i != input.len - 1) {
				errno = EINVAL;
				return -1;
			}
			break;
		}
		if (!jsurl_is_scheme_char(input.bytes[i])) {
			errno = EINVAL;
			return -1;
		}
	}
	if (input.bytes[input.len - 1] == ':') {
		memmove(result_ptr->bytes, input.bytes, input.len);
		result_ptr->len = input.len;
	} else {
		memmove(result_ptr->bytes, input.bytes, input.len);
		result_ptr->bytes[input.len] = ':';
		result_ptr->len = input.len + 1;
	}
	return 0;
}

static int jsurl_normalize_search_copy(jsstr8_t *result_ptr, jsstr8_t input)
{
	jsstr8_t body = input.len > 0 && input.bytes[0] == '?'
		? jsurl_slice(input, 1, -1)
		: input;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (body.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	if (body.len + 1 > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->bytes[0] = '?';
	memmove(result_ptr->bytes + 1, body.bytes, body.len);
	result_ptr->len = body.len + 1;
	return 0;
}

static int jsurl_normalize_hash_copy(jsstr8_t *result_ptr, jsstr8_t input)
{
	jsstr8_t body = input.len > 0 && input.bytes[0] == '#'
		? jsurl_slice(input, 1, -1)
		: input;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (body.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	if (body.len + 1 > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->bytes[0] = '#';
	memmove(result_ptr->bytes + 1, body.bytes, body.len);
	result_ptr->len = body.len + 1;
	return 0;
}

static int jsurl_normalize_pathname_copy(jsstr8_t *result_ptr, jsstr8_t input,
		int has_authority)
{
	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (has_authority) {
		if (input.len == 0) {
			if (result_ptr->cap < 1) {
				errno = ENOBUFS;
				return -1;
			}
			result_ptr->bytes[0] = '/';
			result_ptr->len = 1;
			return 0;
		}
		if (input.bytes[0] != '/') {
			if (input.len + 1 > result_ptr->cap) {
				errno = ENOBUFS;
				return -1;
			}
			result_ptr->bytes[0] = '/';
			memmove(result_ptr->bytes + 1, input.bytes, input.len);
			result_ptr->len = input.len + 1;
			return 0;
		}
	}
	return jsurl_buffer_copy(result_ptr, input);
}

static int jsurl_commit_parts(jsurl_t *url, const jsurl_parts_t *parts)
{
	size_t params_need;
	size_t storage_need;
	size_t host_len;
	size_t origin_len;
	size_t href_len;

	if (url == NULL || parts == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (parts->protocol.len > url->protocol.cap
			|| parts->username.len > url->username.cap
			|| parts->password.len > url->password.cap
			|| parts->hostname.len > url->hostname.cap
			|| parts->port.len > url->port.cap
			|| parts->pathname.len > url->pathname.cap
			|| parts->search.len > url->search.cap
			|| parts->hash.len > url->hash.cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_search_params_requirements(parts->search, &params_need,
			&storage_need) < 0) {
		return -1;
	}
	if (params_need > url->search_params.cap
			|| storage_need > url->search_params.storage.cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_measure_derived(parts, &host_len, &origin_len, &href_len) < 0) {
		return -1;
	}
	if (host_len > url->host.cap || origin_len > url->origin.cap
			|| href_len > url->href.cap) {
		errno = ENOBUFS;
		return -1;
	}

	jsurl_buffer_copy(&url->protocol, parts->protocol);
	jsurl_buffer_copy(&url->username, parts->username);
	jsurl_buffer_copy(&url->password, parts->password);
	jsurl_buffer_copy(&url->hostname, parts->hostname);
	jsurl_buffer_copy(&url->port, parts->port);
	jsurl_buffer_copy(&url->pathname, parts->pathname);
	jsurl_buffer_copy(&url->search, parts->search);
	jsurl_buffer_copy(&url->hash, parts->hash);
	url->has_authority = parts->has_authority;

	jsurl_search_params_clear(&url->search_params);
	if (jsurl_search_params_parse_no_sync(&url->search_params, parts->search) < 0) {
		return -1;
	}
	return jsurl_update_derived(url);
}

void jsurl_view_clear(jsurl_view_t *view)
{
	if (view == NULL) {
		return;
	}
	memset(view, 0, sizeof(*view));
}

int jsurl_view_parse(jsurl_view_t *view, jsstr8_t input)
{
	jsurl_parts_t parts;

	if (view == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_parse_absolute_parts(input, &parts) < 0) {
		return -1;
	}
	view->href = input;
	view->protocol = parts.protocol;
	view->username = parts.username;
	view->password = parts.password;
	view->hostname = parts.hostname;
	view->port = parts.port;
	view->pathname = parts.pathname;
	view->search = parts.search;
	view->hash = parts.hash;
	view->has_authority = parts.has_authority;
	jsurl_search_params_view_init(&view->search_params, view->search);
	return 0;
}

int jsurl_view_host_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t parts;
	size_t host_len;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_view_parts(view, &parts);
	if (jsurl_measure_derived(&parts, &host_len, NULL, NULL) < 0) {
		return -1;
	}
	*len_ptr = host_len;
	return 0;
}

int jsurl_view_host_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_view_host_measure(view, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->len = 0;
	if (view->hostname.len > 0) {
		memmove(result_ptr->bytes, view->hostname.bytes, view->hostname.len);
		result_ptr->len = view->hostname.len;
		if (view->port.len > 0) {
			result_ptr->bytes[result_ptr->len++] = ':';
			memmove(result_ptr->bytes + result_ptr->len, view->port.bytes,
				view->port.len);
			result_ptr->len += view->port.len;
		}
	}
	return 0;
}

int jsurl_view_origin_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t parts;
	size_t origin_len;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_view_parts(view, &parts);
	if (jsurl_measure_derived(&parts, NULL, &origin_len, NULL) < 0) {
		return -1;
	}
	*len_ptr = origin_len;
	return 0;
}

int jsurl_view_origin_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_view_origin_measure(view, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_protocol_is_special_origin(view->protocol) && view->has_authority) {
		size_t host_len;

		if (jsurl_view_host_measure(view, &host_len) < 0) {
			return -1;
		}
		result_ptr->len = 0;
		memmove(result_ptr->bytes, view->protocol.bytes, view->protocol.len);
		result_ptr->len = view->protocol.len;
		result_ptr->bytes[result_ptr->len++] = '/';
		result_ptr->bytes[result_ptr->len++] = '/';
		if (host_len > 0) {
			jsstr8_t host_out;

			host_out.bytes = result_ptr->bytes + result_ptr->len;
			host_out.len = 0;
			host_out.cap = result_ptr->cap - result_ptr->len;
			if (jsurl_view_host_serialize(view, &host_out) < 0) {
				return -1;
			}
			result_ptr->len += host_out.len;
		}
		return 0;
	}
	return jsurl_buffer_copy_literal(result_ptr, "null");
}

int jsurl_view_href_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*len_ptr = view->href.len;
	return 0;
}

int jsurl_view_href_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_buffer_copy(result_ptr, view->href);
}

jsstr8_t jsurl_view_to_json(const jsurl_view_t *view)
{
	if (view == NULL) {
		return jsstr8_empty;
	}
	return view->href;
}

int jsurl_copy_sizes(const jsurl_view_t *view, jsurl_sizes_t *sizes_ptr)
{
	jsurl_parts_t parts;

	if (view == NULL || sizes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_view_parts(view, &parts);
	return jsurl_copy_sizes_from_parts(&parts, sizes_ptr);
}

void jsurl_search_params_view_init(jsurl_search_params_view_t *view,
		jsstr8_t search)
{
	if (view == NULL) {
		return;
	}
	view->search = jsurl_query_body(search);
}

size_t jsurl_search_params_view_size(const jsurl_search_params_view_t *view)
{
	size_t params_len = 0;
	size_t storage_len = 0;

	if (view == NULL) {
		return 0;
	}
	if (jsurl_search_params_requirements(view->search, &params_len, &storage_len) < 0) {
		return 0;
	}
	return params_len;
}

int jsurl_search_params_view_measure(const jsurl_search_params_view_t *view,
		size_t *len_ptr)
{
	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*len_ptr = jsurl_query_body(view->search).len;
	return 0;
}

int jsurl_search_params_view_serialize(const jsurl_search_params_view_t *view,
		jsstr8_t *result_ptr)
{
	jsstr8_t body;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	body = jsurl_query_body(view->search);
	return jsurl_buffer_copy(result_ptr, body);
}

size_t jsurl_search_params_size(const jsurl_search_params_t *params)
{
	return params != NULL ? params->len : 0;
}

int jsurl_search_params_init(jsurl_search_params_t *params,
		const jsurl_search_params_storage_t *storage)
{
	if (params == NULL || storage == NULL) {
		errno = EINVAL;
		return -1;
	}
	if ((storage->params_cap > 0 && storage->params == NULL)
			|| !jsurl_storage_valid(storage->storage)) {
		errno = EINVAL;
		return -1;
	}
	params->owner = NULL;
	params->params = storage->params;
	params->len = 0;
	params->cap = storage->params_cap;
	params->storage = storage->storage;
	params->storage.len = 0;
	params->syncing = 0;
	return 0;
}

void jsurl_search_params_clear(jsurl_search_params_t *params)
{
	if (params == NULL) {
		return;
	}
	params->len = 0;
	params->storage.len = 0;
}

int jsurl_search_params_parse(jsurl_search_params_t *params, jsstr8_t search)
{
	size_t param_cap;
	size_t storage_cap;
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	param_cap = params->cap > 0 ? params->cap : 1;
	storage_cap = params->storage.cap > 0 ? params->storage.cap : 1;
	(void) param_cap;
	(void) storage_cap;
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	if (jsurl_search_params_parse_no_sync(&preview, search) < 0) {
		return -1;
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_append(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value)
{
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	if (jsurl_search_params_copy_state(&preview, params) < 0) {
		return -1;
	}
	if (jsurl_search_params_append_no_sync(&preview, name, value) < 0) {
		return -1;
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_delete(jsurl_search_params_t *params, jsstr8_t name)
{
	size_t i;
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		if (!jsurl_str_eq(params->params[i].name, name)) {
			if (jsurl_search_params_append_no_sync(&preview, params->params[i].name,
					params->params[i].value) < 0) {
				return -1;
			}
		}
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_delete_value(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value)
{
	size_t i;
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		if (!(jsurl_str_eq(params->params[i].name, name)
				&& jsurl_str_eq(params->params[i].value, value))) {
			if (jsurl_search_params_append_no_sync(&preview, params->params[i].name,
					params->params[i].value) < 0) {
				return -1;
			}
		}
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_get(const jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t *value_ptr, int *found_ptr)
{
	size_t i;

	if (params == NULL || value_ptr == NULL || found_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*found_ptr = 0;
	*value_ptr = jsstr8_empty;
	for (i = 0; i < params->len; i++) {
		if (jsurl_str_eq(params->params[i].name, name)) {
			*found_ptr = 1;
			*value_ptr = params->params[i].value;
			return 0;
		}
	}
	return 0;
}

int jsurl_search_params_get_all(const jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t *values, size_t *len_ptr, size_t *found_ptr)
{
	size_t i;
	size_t out_len = 0;
	size_t found_len = 0;
	size_t cap;

	if (params == NULL || len_ptr == NULL || found_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	cap = *len_ptr;
	for (i = 0; i < params->len; i++) {
		if (jsurl_str_eq(params->params[i].name, name)) {
			if (out_len < cap && values != NULL) {
				values[out_len] = params->params[i].value;
			}
			if (out_len < cap) {
				out_len += 1;
			}
			found_len += 1;
		}
	}
	*len_ptr = out_len;
	*found_ptr = found_len;
	return 0;
}

int jsurl_search_params_has(const jsurl_search_params_t *params, jsstr8_t name,
		int *found_ptr)
{
	size_t i;

	if (params == NULL || found_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*found_ptr = 0;
	for (i = 0; i < params->len; i++) {
		if (jsurl_str_eq(params->params[i].name, name)) {
			*found_ptr = 1;
			break;
		}
	}
	return 0;
}

int jsurl_search_params_set(jsurl_search_params_t *params, jsstr8_t name,
		jsstr8_t value)
{
	size_t i;
	int found = 0;
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		if (jsurl_str_eq(params->params[i].name, name)) {
			if (!found) {
				if (jsurl_search_params_append_no_sync(&preview, name, value) < 0) {
					return -1;
				}
				found = 1;
			}
		} else if (jsurl_search_params_append_no_sync(&preview, params->params[i].name,
				params->params[i].value) < 0) {
			return -1;
		}
	}
	if (!found) {
		if (jsurl_search_params_append_no_sync(&preview, name, value) < 0) {
			return -1;
		}
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_sort(jsurl_search_params_t *params)
{
	size_t i;
	size_t j;
	jsurl_param_t temp_params[params != NULL && params->cap > 0 ? params->cap : 1];
	uint8_t temp_storage[params != NULL && params->storage.cap > 0
		? params->storage.cap : 1];
	jsurl_search_params_t preview;

	if (params == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_init_preview(&preview, params, temp_params,
			temp_storage) < 0) {
		return -1;
	}
	if (jsurl_search_params_copy_state(&preview, params) < 0) {
		return -1;
	}
	for (i = 1; i < preview.len; i++) {
		jsurl_param_t key = preview.params[i];

		for (j = i; j > 0; j--) {
			if (jsurl_str_cmp(preview.params[j - 1].name, key.name) <= 0) {
				break;
			}
			preview.params[j] = preview.params[j - 1];
		}
		preview.params[j] = key;
	}
	if (jsurl_search_params_preview_owner_sync(&preview) < 0) {
		return -1;
	}
	return jsurl_search_params_apply_preview(params, &preview);
}

int jsurl_search_params_measure(const jsurl_search_params_t *params,
		size_t *len_ptr)
{
	size_t i;
	size_t len = 0;

	if (params == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		if (i > 0) {
			len += 1;
		}
		len += jsurl_form_encoded_len(params->params[i].name);
		len += 1;
		len += jsurl_form_encoded_len(params->params[i].value);
	}
	*len_ptr = len;
	return 0;
}

int jsurl_search_params_serialize(const jsurl_search_params_t *params,
		jsstr8_t *result_ptr)
{
	size_t i;
	size_t needed_len;
	size_t out_len = 0;

	if (params == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_measure(params, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		size_t written_len;

		if (i > 0) {
			result_ptr->bytes[out_len++] = '&';
		}
		if (jsurl_form_encode_into(params->params[i].name,
				result_ptr->bytes + out_len, &written_len) < 0) {
			return -1;
		}
		out_len += written_len;
		result_ptr->bytes[out_len++] = '=';
		if (jsurl_form_encode_into(params->params[i].value,
				result_ptr->bytes + out_len, &written_len) < 0) {
			return -1;
		}
		out_len += written_len;
	}
	result_ptr->len = out_len;
	return 0;
}

int jsurl_init(jsurl_t *url, const jsurl_storage_t *storage)
{
	if (url == NULL || storage == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (!jsurl_storage_valid(storage->protocol)
			|| !jsurl_storage_valid(storage->username)
			|| !jsurl_storage_valid(storage->password)
			|| !jsurl_storage_valid(storage->hostname)
			|| !jsurl_storage_valid(storage->port)
			|| !jsurl_storage_valid(storage->pathname)
			|| !jsurl_storage_valid(storage->search)
			|| !jsurl_storage_valid(storage->hash)
			|| !jsurl_storage_valid(storage->host)
			|| !jsurl_storage_valid(storage->origin)
			|| !jsurl_storage_valid(storage->href)) {
		errno = EINVAL;
		return -1;
	}

	url->protocol = storage->protocol;
	url->username = storage->username;
	url->password = storage->password;
	url->hostname = storage->hostname;
	url->port = storage->port;
	url->pathname = storage->pathname;
	url->search = storage->search;
	url->hash = storage->hash;
	url->host = storage->host;
	url->origin = storage->origin;
	url->href = storage->href;
	url->protocol.len = 0;
	url->username.len = 0;
	url->password.len = 0;
	url->hostname.len = 0;
	url->port.len = 0;
	url->pathname.len = 0;
	url->search.len = 0;
	url->hash.len = 0;
	url->host.len = 0;
	url->origin.len = 0;
	url->href.len = 0;
	if (jsurl_search_params_init(&url->search_params, &storage->search_params) < 0) {
		return -1;
	}
	url->search_params.owner = url;
	url->has_authority = 0;
	return 0;
}

void jsurl_clear(jsurl_t *url)
{
	if (url == NULL) {
		return;
	}
	url->protocol.len = 0;
	url->username.len = 0;
	url->password.len = 0;
	url->hostname.len = 0;
	url->port.len = 0;
	url->pathname.len = 0;
	url->search.len = 0;
	url->hash.len = 0;
	url->host.len = 0;
	url->origin.len = 0;
	url->href.len = 0;
	url->has_authority = 0;
	jsurl_search_params_clear(&url->search_params);
}

int jsurl_copy_from_view(jsurl_t *url, const jsurl_view_t *view)
{
	jsurl_parts_t parts;

	if (url == NULL || view == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_view_parts(view, &parts);
	return jsurl_commit_parts(url, &parts);
}

int jsurl_parse_copy(jsurl_t *url, jsstr8_t input)
{
	jsurl_view_t view;

	if (jsurl_view_parse(&view, input) < 0) {
		return -1;
	}
	return jsurl_copy_from_view(url, &view);
}

int jsurl_parse_copy_with_base(jsurl_t *url, jsstr8_t input, const jsurl_t *base)
{
	jsurl_parts_t parts;
	jsurl_parts_t base_parts;
	ssize_t scheme_i = jsurl_find_scheme_end(input);

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (scheme_i >= 0) {
		if (jsurl_parse_absolute_parts(input, &parts) < 0) {
			return -1;
		}
		return jsurl_commit_parts(url, &parts);
	}
	if (base == NULL) {
		errno = EINVAL;
		return -1;
	}

	jsurl_current_parts(base, &base_parts);
	parts = base_parts;
	if (input.len == 0) {
		return jsurl_commit_parts(url, &parts);
	}
	if (input.len >= 2 && input.bytes[0] == '/' && input.bytes[1] == '/') {
		size_t abs_len = base->protocol.len + input.len;
		uint8_t abs_buf[abs_len > 0 ? abs_len : 1];
			jsstr8_t absolute;

		jsstr8_init_from_buf(&absolute, (const char *) abs_buf, abs_len);
		if (jsurl_buffer_copy(&absolute, base->protocol) < 0) {
			return -1;
		}
		memmove(absolute.bytes + absolute.len, input.bytes, input.len);
		absolute.len += input.len;
		if (jsurl_parse_absolute_parts(absolute, &parts) < 0) {
			return -1;
		}
		return jsurl_commit_parts(url, &parts);
	}
	if (input.bytes[0] == '?') {
		jsstr8_t path_part;

		jsurl_split_path_search_hash(input, &path_part, &parts.search, &parts.hash);
		(void) path_part;
		return jsurl_commit_parts(url, &parts);
	}
	if (input.bytes[0] == '#') {
		jsstr8_t path_part;
		jsstr8_t search_part;

		jsurl_split_path_search_hash(input, &path_part, &search_part, &parts.hash);
		(void) path_part;
		(void) search_part;
		return jsurl_commit_parts(url, &parts);
	}
	{
		jsstr8_t path_part;
		jsstr8_t search_part;
		jsstr8_t hash_part;

		jsurl_split_path_search_hash(input, &path_part, &search_part, &hash_part);
		parts.search = search_part;
		parts.hash = hash_part;
		if (path_part.len > 0 && path_part.bytes[0] == '/') {
			size_t path_cap = path_part.len + 1;
			uint8_t path_buf[path_cap > 0 ? path_cap : 1];
			jsstr8_t normalized_path;

			jsstr8_init_from_buf(&normalized_path, (const char *) path_buf, path_cap);
			if (jsurl_normalize_path(path_part, base_parts.has_authority,
					&normalized_path) < 0) {
				return -1;
			}
			parts.pathname = normalized_path;
			return jsurl_commit_parts(url, &parts);
		}
		{
			size_t dir_len = jsurl_base_dir_len(base_parts.pathname,
				base_parts.has_authority);
			jsstr8_t prefix;
			size_t merged_len;
			uint8_t *merged_bytes;
			jsstr8_t merged;
			size_t path_cap;
			uint8_t *path_bytes;
			jsstr8_t normalized_path;

			if (base_parts.pathname.len == 0 && base_parts.has_authority) {
				jsstr8_init_from_str(&prefix, "/");
			} else {
				prefix = jsurl_slice(base_parts.pathname, 0, (ssize_t) dir_len);
			}
			merged_len = prefix.len + path_part.len;
			uint8_t merged_buf[merged_len > 0 ? merged_len : 1];
			uint8_t path_buf[merged_len + 1 > 0 ? merged_len + 1 : 1];

			merged_bytes = merged_buf;
			path_bytes = path_buf;
			jsstr8_init_from_buf(&merged, (const char *) merged_bytes, merged_len);
			if (prefix.len > 0) {
				memmove(merged.bytes, prefix.bytes, prefix.len);
			}
			if (path_part.len > 0) {
				memmove(merged.bytes + prefix.len, path_part.bytes, path_part.len);
			}
			merged.len = merged_len;
			path_cap = merged_len + 1;
			jsstr8_init_from_buf(&normalized_path, (const char *) path_bytes,
				path_cap);
			if (jsurl_normalize_path(merged, base_parts.has_authority,
					&normalized_path) < 0) {
				return -1;
			}
			parts.pathname = normalized_path;
			return jsurl_commit_parts(url, &parts);
		}
	}
}

int jsurl_parse(jsurl_t *url, jsstr8_t input)
{
	return jsurl_parse_copy(url, input);
}

int jsurl_parse_with_base(jsurl_t *url, jsstr8_t input, const jsurl_t *base)
{
	return jsurl_parse_copy_with_base(url, input, base);
}

int jsurl_set_protocol(jsurl_t *url, jsstr8_t protocol)
{
	jsurl_parts_t parts;
	size_t cap = protocol.len + 1;
	uint8_t buf[cap > 0 ? cap : 1];
	jsstr8_t normalized;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized, (const char *) buf, cap);
	if (jsurl_normalize_protocol_copy(&normalized, protocol) < 0) {
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.protocol = normalized;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_username(jsurl_t *url, jsstr8_t username)
{
	jsurl_parts_t parts;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.username = username;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_password(jsurl_t *url, jsstr8_t password)
{
	jsurl_parts_t parts;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.password = password;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_hostname(jsurl_t *url, jsstr8_t hostname)
{
	jsurl_parts_t parts;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.hostname = hostname;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_port(jsurl_t *url, jsstr8_t port)
{
	jsurl_parts_t parts;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.port = port;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_pathname(jsurl_t *url, jsstr8_t pathname)
{
	jsurl_parts_t parts;
	size_t cap = pathname.len + 1;
	uint8_t buf[cap > 0 ? cap : 1];
	jsstr8_t normalized;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized, (const char *) buf, cap);
	if (jsurl_normalize_pathname_copy(&normalized, pathname, url->has_authority) < 0) {
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.pathname = normalized;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_search(jsurl_t *url, jsstr8_t search)
{
	jsurl_parts_t parts;
	size_t cap = search.len + 1;
	uint8_t buf[cap > 0 ? cap : 1];
	jsstr8_t normalized;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized, (const char *) buf, cap);
	if (jsurl_normalize_search_copy(&normalized, search) < 0) {
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.search = normalized;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_set_hash(jsurl_t *url, jsstr8_t hash)
{
	jsurl_parts_t parts;
	size_t cap = hash.len + 1;
	uint8_t buf[cap > 0 ? cap : 1];
	jsstr8_t normalized;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized, (const char *) buf, cap);
	if (jsurl_normalize_hash_copy(&normalized, hash) < 0) {
		return -1;
	}
	jsurl_current_parts(url, &parts);
	parts.hash = normalized;
	return jsurl_commit_parts(url, &parts);
}

int jsurl_href_measure(const jsurl_t *url, size_t *len_ptr)
{
	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*len_ptr = url->href.len;
	return 0;
}

int jsurl_href_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_buffer_copy(result_ptr, url->href);
}

int jsurl_origin_measure(const jsurl_t *url, size_t *len_ptr)
{
	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*len_ptr = url->origin.len;
	return 0;
}

int jsurl_origin_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_buffer_copy(result_ptr, url->origin);
}

jsstr8_t jsurl_to_json(const jsurl_t *url)
{
	if (url == NULL) {
		return jsstr8_empty;
	}
	return url->href;
}
