#include <string.h>
#include "jsval.h"

static int
copy_component(jsval_region_t *region, jsval_t str_val,
               char *buf, size_t cap, size_t *out_len)
{
	size_t len = 0;
	if (jsval_string_copy_utf8(region, str_val, NULL, 0, &len) < 0)
		return -1;
	if (len >= cap)
		return -1;
	if (len > 0) {
		if (jsval_string_copy_utf8(region, str_val,
				(uint8_t *)buf, len, NULL) < 0)
			return -1;
	}
	buf[len] = '\0';
	if (out_len)
		*out_len = len;
	return 0;
}

int jsval_url_extract(jsval_region_t *region, jsval_t url_string,
                      struct jsval_url_parts *parts)
{
	jsval_t url_obj;
	jsval_t val;
	size_t path_len = 0, search_len = 0;

	memset(parts, 0, sizeof(*parts));

	if (jsval_url_new(region, url_string, 0,
			jsval_undefined(), &url_obj) != 0)
		return -1;

	/* protocol (e.g. "https:") */
	if (jsval_url_protocol(region, url_obj, &val) != 0)
		return -1;
	if (copy_component(region, val, parts->scheme,
			sizeof(parts->scheme), NULL) < 0)
		return -1;
	{
		size_t slen = strlen(parts->scheme);
		if (slen > 0 && parts->scheme[slen - 1] == ':')
			parts->scheme[slen - 1] = '\0';
	}
	parts->is_tls = (strcmp(parts->scheme, "https") == 0);

	/* hostname */
	if (jsval_url_hostname(region, url_obj, &val) != 0)
		return -1;
	if (copy_component(region, val, parts->host,
			sizeof(parts->host), NULL) < 0)
		return -1;

	/* port (may be empty → use default) */
	if (jsval_url_port(region, url_obj, &val) != 0)
		return -1;
	if (copy_component(region, val, parts->port,
			sizeof(parts->port), NULL) < 0)
		return -1;
	if (parts->port[0] == '\0') {
		if (parts->is_tls) {
			parts->port[0] = '4'; parts->port[1] = '4';
			parts->port[2] = '3'; parts->port[3] = '\0';
		} else {
			parts->port[0] = '8'; parts->port[1] = '0';
			parts->port[2] = '\0';
		}
	}

	/* pathname + search → combined path */
	if (jsval_url_pathname(region, url_obj, &val) != 0)
		return -1;
	if (copy_component(region, val, parts->path,
			sizeof(parts->path), &path_len) < 0)
		return -1;

	if (jsval_url_search(region, url_obj, &val) != 0)
		return -1;
	{
		size_t slen = 0;
		if (jsval_string_copy_utf8(region, val, NULL, 0, &slen) < 0)
			return -1;
		if (slen > 0 && path_len + slen < sizeof(parts->path)) {
			if (jsval_string_copy_utf8(region, val,
					(uint8_t *)(parts->path + path_len),
					slen, &search_len) < 0)
				return -1;
			parts->path[path_len + search_len] = '\0';
		}
	}

	return 0;
}
