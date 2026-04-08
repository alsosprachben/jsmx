#include <errno.h>
#include <string.h>

#include "jsurl.h"
#include "unicode.h"
#include "jsval.h"
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

typedef enum jsurl_component_mode_e {
	JSURL_COMPONENT_PATHNAME = 0,
	JSURL_COMPONENT_SEARCH = 1,
	JSURL_COMPONENT_HASH = 2
} jsurl_component_mode_t;

static int jsurl_search_params_requirements(jsstr8_t search, size_t *params_len_ptr,
		size_t *storage_len_ptr);
static jsstr8_t jsurl_query_body(jsstr8_t search);
static int jsurl_component_wire_measure(jsstr8_t src,
		jsurl_component_mode_t mode, size_t *len_ptr);
static int jsurl_component_wire_serialize(jsstr8_t src,
		jsurl_component_mode_t mode, jsstr8_t *result_ptr);
static int jsurl_normalize_parts_copy(const jsurl_parts_t *parts,
		jsurl_parts_t *normalized_parts, jsstr8_t *hostname_storage,
		jsstr8_t *pathname_storage, jsstr8_t *search_storage,
		jsstr8_t *hash_storage);

#define JSURL_HOSTNAME_ASCII_MAX 253
#define JSURL_HOSTNAME_LABEL_ASCII_MAX 63
#define JSURL_PUNYCODE_BASE 36
#define JSURL_PUNYCODE_TMIN 1
#define JSURL_PUNYCODE_TMAX 26
#define JSURL_PUNYCODE_SKEW 38
#define JSURL_PUNYCODE_DAMP 700
#define JSURL_PUNYCODE_INITIAL_BIAS 72
#define JSURL_PUNYCODE_INITIAL_N 128

typedef int (*jsurl_parts_callback_t)(const jsurl_parts_t *parts, void *opaque);
static int jsurl_resolve_with_base(jsstr8_t input, const jsurl_t *base,
		jsurl_parts_callback_t callback, void *opaque);

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

static int jsurl_is_ascii_upper(uint8_t c)
{
	return c >= 'A' && c <= 'Z';
}

static uint8_t jsurl_ascii_tolower(uint8_t c)
{
	return jsurl_is_ascii_upper(c) ? (uint8_t)(c - 'A' + 'a') : c;
}

static int jsurl_is_digit(uint8_t c)
{
	return c >= '0' && c <= '9';
}

static int jsurl_is_dns_ascii_char(uint8_t c)
{
	c = jsurl_ascii_tolower(c);
	return jsurl_is_alpha(c) || jsurl_is_digit(c) || c == '-';
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

static int jsurl_pathname_wire_measure_parts(const jsurl_parts_t *parts,
		size_t *len_ptr)
{
	size_t len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (parts->has_authority && parts->pathname.len == 0) {
		len = 1;
	} else {
		if (jsurl_component_wire_measure(parts->pathname,
				JSURL_COMPONENT_PATHNAME, &len) < 0) {
			return -1;
		}
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_search_wire_measure_parts(const jsurl_parts_t *parts,
		size_t *len_ptr)
{
	jsstr8_t body;
	size_t len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	body = jsurl_query_body(parts->search);
	if (body.len > 0) {
		if (jsurl_component_wire_measure(body, JSURL_COMPONENT_SEARCH, &len) < 0) {
			return -1;
		}
		len += 1;
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_hash_wire_measure_parts(const jsurl_parts_t *parts,
		size_t *len_ptr)
{
	jsstr8_t body;
	size_t len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	body = parts->hash.len > 0 && parts->hash.bytes[0] == '#'
		? jsurl_slice(parts->hash, 1, -1)
		: parts->hash;
	if (body.len > 0) {
		if (jsurl_component_wire_measure(body, JSURL_COMPONENT_HASH, &len) < 0) {
			return -1;
		}
		len += 1;
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_measure_derived(const jsurl_parts_t *parts, size_t *host_len_ptr,
		size_t *origin_len_ptr, size_t *href_len_ptr)
{
	size_t host_len = jsurl_host_len(parts);
	size_t origin_len;
	size_t path_len = 0;
	size_t search_len = 0;
	size_t hash_len = 0;
	size_t href_len;

	if (jsurl_pathname_wire_measure_parts(parts, &path_len) < 0
			|| jsurl_search_wire_measure_parts(parts, &search_len) < 0
			|| jsurl_hash_wire_measure_parts(parts, &hash_len) < 0) {
		return -1;
	}
	href_len = parts->protocol.len + search_len + hash_len;

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
		href_len += path_len;
	} else {
		href_len += path_len;
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

static int jsurl_validate_ascii_hostname_label_bytes(const uint8_t *bytes,
		size_t len)
{
	size_t i;

	if ((len > 0 && bytes == NULL) || len == 0
			|| len > JSURL_HOSTNAME_LABEL_ASCII_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (bytes[0] == '-' || bytes[len - 1] == '-') {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < len; i++) {
		if (!jsurl_is_dns_ascii_char(bytes[i])) {
			errno = EINVAL;
			return -1;
		}
	}
	return 0;
}

static int jsurl_is_ipv6_literal_hostname(jsstr8_t hostname)
{
	return hostname.len >= 2 && hostname.bytes != NULL
		&& hostname.bytes[0] == '['
		&& hostname.bytes[hostname.len - 1] == ']';
}

static int jsurl_is_dot_separator(uint32_t cp)
{
	return cp == '.'
		|| cp == 0x3002
		|| cp == 0xff0e
		|| cp == 0xff61;
}

static uint8_t jsurl_punycode_encode_digit(size_t d)
{
	return (uint8_t)(d < 26 ? ('a' + d) : ('0' + (d - 26)));
}

static size_t jsurl_punycode_adapt(size_t delta, size_t numpoints,
		int first_time)
{
	size_t k = 0;

	delta = first_time ? (delta / JSURL_PUNYCODE_DAMP) : (delta / 2);
	delta += delta / numpoints;
	while (delta > ((JSURL_PUNYCODE_BASE - JSURL_PUNYCODE_TMIN)
			* JSURL_PUNYCODE_TMAX) / 2) {
		delta /= JSURL_PUNYCODE_BASE - JSURL_PUNYCODE_TMIN;
		k += JSURL_PUNYCODE_BASE;
	}
	return k + (((JSURL_PUNYCODE_BASE - JSURL_PUNYCODE_TMIN + 1) * delta)
			/ (delta + JSURL_PUNYCODE_SKEW));
}

static int jsurl_punycode_encode_label(const uint32_t *src, size_t len,
		uint8_t *dst, size_t dst_cap, size_t *dst_len_ptr)
{
	size_t out_len = 0;
	size_t b = 0;
	size_t h;
	size_t delta = 0;
	size_t bias = JSURL_PUNYCODE_INITIAL_BIAS;
	uint32_t n = JSURL_PUNYCODE_INITIAL_N;
	size_t i;

	if (src == NULL || dst == NULL || dst_len_ptr == NULL || len == 0) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < len; i++) {
		if (src[i] < 0x80) {
			if (out_len >= dst_cap) {
				errno = EINVAL;
				return -1;
			}
			dst[out_len++] = (uint8_t)src[i];
			b++;
		}
	}
	h = b;
	if (b > 0 && h < len) {
		if (out_len >= dst_cap) {
			errno = EINVAL;
			return -1;
		}
		dst[out_len++] = '-';
	}

	while (h < len) {
		uint32_t m = UINT32_MAX;

		for (i = 0; i < len; i++) {
			if (src[i] >= n && src[i] < m) {
				m = src[i];
			}
		}
		if (m == UINT32_MAX) {
			errno = EINVAL;
			return -1;
		}
		if ((size_t)(m - n) > (SIZE_MAX - delta) / (h + 1)) {
			errno = EOVERFLOW;
			return -1;
		}
		delta += (size_t)(m - n) * (h + 1);
		n = m;

		for (i = 0; i < len; i++) {
			size_t q;

			if (src[i] < n) {
				if (delta == SIZE_MAX) {
					errno = EOVERFLOW;
					return -1;
				}
				delta++;
			}
			if (src[i] != n) {
				continue;
			}

			q = delta;
			for (;;) {
				size_t k;
				size_t t;

				k = JSURL_PUNYCODE_BASE;
				while (1) {
					t = k <= bias ? JSURL_PUNYCODE_TMIN
						: k >= bias + JSURL_PUNYCODE_TMAX
							? JSURL_PUNYCODE_TMAX
							: k - bias;
					if (q < t) {
						break;
					}
					if (out_len >= dst_cap) {
						errno = EINVAL;
						return -1;
					}
					dst[out_len++] = jsurl_punycode_encode_digit(t
						+ ((q - t) % (JSURL_PUNYCODE_BASE - t)));
					q = (q - t) / (JSURL_PUNYCODE_BASE - t);
					if (k > SIZE_MAX - JSURL_PUNYCODE_BASE) {
						errno = EOVERFLOW;
						return -1;
					}
					k += JSURL_PUNYCODE_BASE;
				}
				if (out_len >= dst_cap) {
					errno = EINVAL;
					return -1;
				}
				dst[out_len++] = jsurl_punycode_encode_digit(q);
				break;
			}
			bias = jsurl_punycode_adapt(delta, h + 1, h == b);
			delta = 0;
			h++;
		}

		if (delta == SIZE_MAX || n == UINT32_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		delta++;
		n++;
	}

	*dst_len_ptr = out_len;
	return 0;
}

static int jsurl_punycode_decode_digit(uint8_t c, size_t *digit_ptr)
{
	if (digit_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	c = jsurl_ascii_tolower(c);
	if (c >= 'a' && c <= 'z') {
		*digit_ptr = (size_t)(c - 'a');
		return 0;
	}
	if (c >= '0' && c <= '9') {
		*digit_ptr = (size_t)(26 + c - '0');
		return 0;
	}
	errno = EINVAL;
	return -1;
}

static int jsurl_punycode_decode_label(const uint8_t *src, size_t len,
		uint32_t *dst, size_t dst_cap, size_t *dst_len_ptr)
{
	size_t out_len = 0;
	size_t bias = JSURL_PUNYCODE_INITIAL_BIAS;
	uint32_t n = JSURL_PUNYCODE_INITIAL_N;
	size_t i = 0;
	size_t basic_len = 0;
	size_t in_i;
	size_t j;

	if (src == NULL || dst == NULL || dst_len_ptr == NULL || len == 0) {
		errno = EINVAL;
		return -1;
	}

	for (j = len; j > 0; j--) {
		if (src[j - 1] == '-') {
			basic_len = j - 1;
			break;
		}
	}
	for (j = 0; j < basic_len; j++) {
		if (src[j] >= 0x80 || out_len >= dst_cap) {
			errno = EINVAL;
			return -1;
		}
		dst[out_len++] = src[j];
	}
	in_i = basic_len > 0 ? basic_len + 1 : 0;

	while (in_i < len) {
		size_t oldi = i;
		size_t w = 1;
		size_t k;

		for (k = JSURL_PUNYCODE_BASE;; k += JSURL_PUNYCODE_BASE) {
			size_t digit;
			size_t t;

			if (in_i >= len) {
				errno = EINVAL;
				return -1;
			}
			if (jsurl_punycode_decode_digit(src[in_i++], &digit) < 0) {
				return -1;
			}
			if (digit > (SIZE_MAX - i) / w) {
				errno = EOVERFLOW;
				return -1;
			}
			i += digit * w;
			t = k <= bias ? JSURL_PUNYCODE_TMIN
				: k >= bias + JSURL_PUNYCODE_TMAX
					? JSURL_PUNYCODE_TMAX
					: k - bias;
			if (digit < t) {
				break;
			}
			if (w > SIZE_MAX / (JSURL_PUNYCODE_BASE - t)) {
				errno = EOVERFLOW;
				return -1;
			}
			w *= JSURL_PUNYCODE_BASE - t;
		}

		bias = jsurl_punycode_adapt(i - oldi, out_len + 1, oldi == 0);
		if (i / (out_len + 1) > UINT32_MAX - n) {
			errno = EOVERFLOW;
			return -1;
		}
		n += (uint32_t)(i / (out_len + 1));
		i %= out_len + 1;
		if (n > 0x10ffff || (n >= 0xd800 && n < 0xe000)
				|| out_len >= dst_cap) {
			errno = EINVAL;
			return -1;
		}
		memmove(dst + i + 1, dst + i, (out_len - i) * sizeof(*dst));
		dst[i] = n;
		out_len++;
		i++;
	}

	*dst_len_ptr = out_len;
	return 0;
}

static int jsurl_validate_ace_hostname_label_bytes(const uint8_t *bytes,
		size_t len)
{
	uint32_t decoded[JSURL_HOSTNAME_LABEL_ASCII_MAX];
	uint8_t reencoded[JSURL_HOSTNAME_LABEL_ASCII_MAX];
	size_t decoded_len;
	size_t reencoded_len;
	size_t i;
	int saw_non_ascii = 0;

	if (bytes == NULL || len <= 4) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_punycode_decode_label(bytes + 4, len - 4, decoded,
			sizeof(decoded) / sizeof(decoded[0]), &decoded_len) < 0) {
		return -1;
	}
	if (decoded_len == 0) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < decoded_len; i++) {
		if (decoded[i] > 0x7f) {
			saw_non_ascii = 1;
			break;
		}
	}
	if (!saw_non_ascii) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_punycode_encode_label(decoded, decoded_len, reencoded,
			sizeof(reencoded), &reencoded_len) < 0) {
		return -1;
	}
	if (reencoded_len != len - 4
			|| memcmp(bytes + 4, reencoded, reencoded_len) != 0) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static int jsurl_normalize_hostname_label_codepoints_copy(uint8_t *dst,
		size_t dst_cap, const uint32_t *input, size_t input_len,
		size_t *dst_len_ptr)
{
	size_t workspace_cap;
	size_t i;
	int all_ascii = 1;

	if (dst == NULL || dst_len_ptr == NULL || input == NULL
			|| input_len == 0 || dst_cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (unicode_normalize_form_workspace_len(input, input_len,
			UNICODE_NORMALIZE_NFC, &workspace_cap) < 0) {
		return -1;
	}
	{
		uint32_t normalized[workspace_cap > 0 ? workspace_cap : 1];
		size_t puny_len;
		size_t normalized_len;

		normalized_len = unicode_normalize_into_form(input, input_len,
				normalized, workspace_cap, UNICODE_NORMALIZE_NFC);
		if (normalized_len > workspace_cap) {
			errno = EINVAL;
			return -1;
		}
		if (normalized_len == 0) {
			errno = EINVAL;
			return -1;
		}

		for (i = 0; i < normalized_len; i++) {
			uint32_t lower = unicode_tolower(normalized[i]);

			if (jsurl_is_dot_separator(lower)) {
				errno = EINVAL;
				return -1;
			}
			normalized[i] = lower;
			if (lower > 0x7f) {
				all_ascii = 0;
			} else if (!jsurl_is_dns_ascii_char((uint8_t)lower)) {
				errno = EINVAL;
				return -1;
			}
		}

		if (all_ascii) {
			if (normalized_len > dst_cap) {
				errno = ENOBUFS;
				return -1;
			}
			for (i = 0; i < normalized_len; i++) {
				dst[i] = (uint8_t)normalized[i];
			}
			if (jsurl_validate_ascii_hostname_label_bytes(dst,
					normalized_len) < 0) {
				return -1;
			}
			if (normalized_len >= 4
					&& memcmp(dst, "xn--", 4) == 0
					&& jsurl_validate_ace_hostname_label_bytes(dst,
						normalized_len) < 0) {
				return -1;
			}
			*dst_len_ptr = normalized_len;
			return 0;
		}

		if (dst_cap < 4) {
			errno = ENOBUFS;
			return -1;
		}
		memcpy(dst, "xn--", 4);
		if (jsurl_punycode_encode_label(normalized, normalized_len,
				dst + 4, dst_cap - 4, &puny_len) < 0) {
			return -1;
		}
		if (jsurl_validate_ascii_hostname_label_bytes(dst, 4 + puny_len) < 0) {
			return -1;
		}
		*dst_len_ptr = 4 + puny_len;
		return 0;
	}
}

static int jsurl_normalize_hostname_label_copy(uint8_t *dst, size_t dst_cap,
		jsstr8_t input, size_t *dst_len_ptr)
{
	size_t decoded_cap;
	uint32_t cp;
	int l;
	const uint8_t *bc;
	const uint8_t *bs;
	size_t decoded_len = 0;

	if (dst == NULL || dst_len_ptr == NULL || input.bytes == NULL
			|| input.len == 0 || dst_cap == 0) {
		errno = EINVAL;
		return -1;
	}

	decoded_cap = input.len > 0 ? input.len : 1;
	{
		uint32_t decoded[decoded_cap];

		bc = input.bytes;
		bs = input.bytes + input.len;
		while (bc < bs) {
			UTF8_CHAR(bc, bs, &cp, &l);
			if (l <= 0) {
				errno = EINVAL;
				return -1;
			}
			decoded[decoded_len++] = cp;
			bc += l;
		}
		return jsurl_normalize_hostname_label_codepoints_copy(dst, dst_cap,
				decoded, decoded_len, dst_len_ptr);
	}
}

static int jsurl_normalize_hostname_copy(jsstr8_t *result_ptr, jsstr8_t input)
{
	size_t decoded_cap;
	uint32_t cp;
	int l;
	const uint8_t *bc;
	const uint8_t *bs;
	size_t out_len = 0;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (input.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	if (jsurl_is_ipv6_literal_hostname(input)) {
		if (input.len > result_ptr->cap) {
			errno = ENOBUFS;
			return -1;
		}
		memmove(result_ptr->bytes, input.bytes, input.len);
		result_ptr->len = input.len;
		return 0;
	}
	decoded_cap = input.len > 0 ? input.len : 1;
	{
		uint32_t label[decoded_cap];
		size_t label_len = 0;

		bc = input.bytes;
		bs = input.bytes + input.len;
		while (bc < bs) {
			uint8_t label_buf[JSURL_HOSTNAME_LABEL_ASCII_MAX];
			size_t ascii_label_len;

			UTF8_CHAR(bc, bs, &cp, &l);
			if (l <= 0) {
				errno = EINVAL;
				return -1;
			}
			bc += l;
			if (jsurl_is_dot_separator(cp)) {
				if (label_len == 0) {
					errno = EINVAL;
					return -1;
				}
				if (jsurl_normalize_hostname_label_codepoints_copy(label_buf,
						sizeof(label_buf), label, label_len,
						&ascii_label_len) < 0) {
					return -1;
				}
				if ((out_len > 0 && out_len == result_ptr->cap)
						|| out_len + (out_len > 0 ? 1 : 0) + ascii_label_len
							> result_ptr->cap) {
					errno = ENOBUFS;
					return -1;
				}
				if (out_len > 0) {
					result_ptr->bytes[out_len++] = '.';
				}
				memmove(result_ptr->bytes + out_len, label_buf, ascii_label_len);
				out_len += ascii_label_len;
				label_len = 0;
				continue;
			}
			label[label_len++] = cp;
		}

		if (label_len == 0) {
			errno = EINVAL;
			return -1;
		}
		{
			uint8_t label_buf[JSURL_HOSTNAME_LABEL_ASCII_MAX];
			size_t ascii_label_len;

			if (jsurl_normalize_hostname_label_codepoints_copy(label_buf,
					sizeof(label_buf), label, label_len,
					&ascii_label_len) < 0) {
				return -1;
			}
			if ((out_len > 0 && out_len == result_ptr->cap)
					|| out_len + (out_len > 0 ? 1 : 0) + ascii_label_len
						> result_ptr->cap) {
				errno = ENOBUFS;
				return -1;
			}
			if (out_len > 0) {
				result_ptr->bytes[out_len++] = '.';
			}
			memmove(result_ptr->bytes + out_len, label_buf, ascii_label_len);
			out_len += ascii_label_len;
		}
	}

	if (out_len > JSURL_HOSTNAME_ASCII_MAX) {
		errno = EINVAL;
		return -1;
	}
	result_ptr->len = out_len;
	return 0;
}

static int jsurl_hostname_display_measure_value(jsstr8_t hostname,
		size_t *len_ptr)
{
	size_t len = 0;
	size_t start_i = 0;
	size_t i;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (hostname.len == 0) {
		*len_ptr = 0;
		return 0;
	}
	if (jsurl_is_ipv6_literal_hostname(hostname)) {
		*len_ptr = hostname.len;
		return 0;
	}
	for (i = 0; i <= hostname.len; i++) {
		if (i == hostname.len || hostname.bytes[i] == '.') {
			jsstr8_t label;

			if (i == start_i) {
				errno = EINVAL;
				return -1;
			}
			label = jsurl_slice(hostname, start_i, (ssize_t)i);
			if (label.len >= 4 && memcmp(label.bytes, "xn--", 4) == 0) {
				uint32_t decoded[JSURL_HOSTNAME_LABEL_ASCII_MAX];
				size_t decoded_len;
				size_t j;

				if (jsurl_punycode_decode_label(label.bytes + 4, label.len - 4,
						decoded, sizeof(decoded) / sizeof(decoded[0]),
						&decoded_len) < 0) {
					return -1;
				}
				for (j = 0; j < decoded_len; j++) {
					len += (size_t)UTF8_CLEN(decoded[j]);
				}
			} else {
				len += label.len;
			}
			if (i < hostname.len) {
				len += 1;
			}
			start_i = i + 1;
		}
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_hostname_display_serialize_value(jsstr8_t hostname,
		jsstr8_t *result_ptr)
{
	size_t needed_len;
	size_t out_len = 0;
	size_t start_i = 0;
	size_t i;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_hostname_display_measure_value(hostname, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (hostname.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	if (jsurl_is_ipv6_literal_hostname(hostname)) {
		return jsurl_buffer_copy(result_ptr, hostname);
	}
	for (i = 0; i <= hostname.len; i++) {
		if (i == hostname.len || hostname.bytes[i] == '.') {
			jsstr8_t label;

			label = jsurl_slice(hostname, start_i, (ssize_t)i);
			if (label.len >= 4 && memcmp(label.bytes, "xn--", 4) == 0) {
				uint32_t decoded[JSURL_HOSTNAME_LABEL_ASCII_MAX];
				size_t decoded_len;
				size_t j;

				if (jsurl_punycode_decode_label(label.bytes + 4, label.len - 4,
						decoded, sizeof(decoded) / sizeof(decoded[0]),
						&decoded_len) < 0) {
					return -1;
				}
				for (j = 0; j < decoded_len; j++) {
					const uint32_t *read = decoded + j;
					uint8_t *write = result_ptr->bytes + out_len;
					const uint8_t *write_stop = result_ptr->bytes
						+ result_ptr->cap;

					UTF8_ENCODE(&read, decoded + j + 1, &write, write_stop);
					out_len = (size_t)(write - result_ptr->bytes);
				}
			} else if (label.len > 0) {
				memmove(result_ptr->bytes + out_len, label.bytes, label.len);
				out_len += label.len;
			}
			if (i < hostname.len) {
				result_ptr->bytes[out_len++] = '.';
			}
			start_i = i + 1;
		}
	}
	result_ptr->len = out_len;
	return 0;
}

static int jsurl_host_display_measure_parts(const jsurl_parts_t *parts,
		size_t *len_ptr)
{
	size_t hostname_len = 0;
	size_t len = 0;

	if (parts == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (parts->hostname.len > 0) {
		if (jsurl_hostname_display_measure_value(parts->hostname,
				&hostname_len) < 0) {
			return -1;
		}
		len = hostname_len;
		if (parts->port.len > 0) {
			len += 1 + parts->port.len;
		}
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_serialize_host_display_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_host_display_measure_parts(parts, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->len = 0;
	if (parts->hostname.len == 0) {
		return 0;
	}
	{
		jsstr8_t hostname_out;

		hostname_out.bytes = result_ptr->bytes;
		hostname_out.len = 0;
		hostname_out.cap = result_ptr->cap;
		if (jsurl_hostname_display_serialize_value(parts->hostname,
				&hostname_out) < 0) {
			return -1;
		}
		result_ptr->len = hostname_out.len;
	}
	if (parts->port.len > 0) {
		result_ptr->bytes[result_ptr->len++] = ':';
		memmove(result_ptr->bytes + result_ptr->len, parts->port.bytes,
				parts->port.len);
		result_ptr->len += parts->port.len;
	}
	return 0;
}

static int jsurl_origin_display_measure_parts(const jsurl_parts_t *parts,
		size_t *len_ptr)
{
	size_t host_len;

	if (parts == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_protocol_is_special_origin(parts->protocol)
			&& parts->has_authority) {
		if (jsurl_host_display_measure_parts(parts, &host_len) < 0) {
			return -1;
		}
		*len_ptr = parts->protocol.len + 2 + host_len;
		return 0;
	}
	*len_ptr = 4;
	return 0;
}

static int jsurl_serialize_origin_display_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;
	size_t host_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_origin_display_measure_parts(parts, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_protocol_is_special_origin(parts->protocol)
			&& parts->has_authority) {
		result_ptr->len = 0;
		memmove(result_ptr->bytes, parts->protocol.bytes, parts->protocol.len);
		result_ptr->len = parts->protocol.len;
		result_ptr->bytes[result_ptr->len++] = '/';
		result_ptr->bytes[result_ptr->len++] = '/';
		if (jsurl_host_display_measure_parts(parts, &host_len) < 0) {
			return -1;
		}
		if (host_len > 0) {
			jsstr8_t host_out;

			host_out.bytes = result_ptr->bytes + result_ptr->len;
			host_out.len = 0;
			host_out.cap = result_ptr->cap - result_ptr->len;
			if (jsurl_serialize_host_display_parts(parts, &host_out) < 0) {
				return -1;
			}
			result_ptr->len += host_out.len;
		}
		return 0;
	}
	return jsurl_buffer_copy_literal(result_ptr, "null");
}

static int jsurl_serialize_host_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_measure_derived(parts, &needed_len, NULL, NULL) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->len = 0;
	if (parts->hostname.len > 0) {
		memmove(result_ptr->bytes, parts->hostname.bytes, parts->hostname.len);
		result_ptr->len = parts->hostname.len;
		if (parts->port.len > 0) {
			result_ptr->bytes[result_ptr->len++] = ':';
			memmove(result_ptr->bytes + result_ptr->len, parts->port.bytes,
				parts->port.len);
			result_ptr->len += parts->port.len;
		}
	}
	return 0;
}

static int jsurl_serialize_origin_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;
	size_t host_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_measure_derived(parts, &host_len, &needed_len, NULL) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_protocol_is_special_origin(parts->protocol) && parts->has_authority) {
		result_ptr->len = 0;
		memmove(result_ptr->bytes, parts->protocol.bytes, parts->protocol.len);
		result_ptr->len = parts->protocol.len;
		result_ptr->bytes[result_ptr->len++] = '/';
		result_ptr->bytes[result_ptr->len++] = '/';
		if (host_len > 0) {
			jsstr8_t host_out;

			host_out.bytes = result_ptr->bytes + result_ptr->len;
			host_out.len = 0;
			host_out.cap = result_ptr->cap - result_ptr->len;
			if (jsurl_serialize_host_parts(parts, &host_out) < 0) {
				return -1;
			}
			result_ptr->len += host_out.len;
		}
		return 0;
	}
	return jsurl_buffer_copy_literal(result_ptr, "null");
}

static int jsurl_serialize_pathname_wire_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_pathname_wire_measure_parts(parts, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (parts->has_authority && parts->pathname.len == 0) {
		result_ptr->bytes[0] = '/';
		result_ptr->len = 1;
		return 0;
	}
	return jsurl_component_wire_serialize(parts->pathname,
			JSURL_COMPONENT_PATHNAME, result_ptr);
}

static int jsurl_serialize_search_wire_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	jsstr8_t body;
	jsstr8_t body_out;
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_wire_measure_parts(parts, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	body = jsurl_query_body(parts->search);
	if (body.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	result_ptr->bytes[0] = '?';
	jsstr8_init_from_buf(&body_out, (const char *)(result_ptr->bytes + 1),
			result_ptr->cap - 1);
	if (jsurl_component_wire_serialize(body, JSURL_COMPONENT_SEARCH,
			&body_out) < 0) {
		return -1;
	}
	result_ptr->len = body_out.len + 1;
	return 0;
}

static int jsurl_serialize_hash_wire_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	jsstr8_t body;
	jsstr8_t body_out;
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_hash_wire_measure_parts(parts, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	body = parts->hash.len > 0 && parts->hash.bytes[0] == '#'
		? jsurl_slice(parts->hash, 1, -1)
		: parts->hash;
	if (body.len == 0) {
		result_ptr->len = 0;
		return 0;
	}
	result_ptr->bytes[0] = '#';
	jsstr8_init_from_buf(&body_out, (const char *)(result_ptr->bytes + 1),
			result_ptr->cap - 1);
	if (jsurl_component_wire_serialize(body, JSURL_COMPONENT_HASH,
			&body_out) < 0) {
		return -1;
	}
	result_ptr->len = body_out.len + 1;
	return 0;
}

static int jsurl_serialize_href_parts(const jsurl_parts_t *parts,
		jsstr8_t *result_ptr)
{
	size_t needed_len;

	if (parts == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_measure_derived(parts, NULL, NULL, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	result_ptr->len = 0;
	if (parts->protocol.len > 0) {
		memmove(result_ptr->bytes, parts->protocol.bytes, parts->protocol.len);
		result_ptr->len += parts->protocol.len;
	}
	if (parts->has_authority) {
		result_ptr->bytes[result_ptr->len++] = '/';
		result_ptr->bytes[result_ptr->len++] = '/';
		if (parts->username.len > 0 || parts->password.len > 0) {
			if (parts->username.len > 0) {
				memmove(result_ptr->bytes + result_ptr->len,
					parts->username.bytes, parts->username.len);
				result_ptr->len += parts->username.len;
			}
			if (parts->password.len > 0) {
				result_ptr->bytes[result_ptr->len++] = ':';
				memmove(result_ptr->bytes + result_ptr->len,
					parts->password.bytes, parts->password.len);
				result_ptr->len += parts->password.len;
			}
			result_ptr->bytes[result_ptr->len++] = '@';
		}
		if (parts->hostname.len > 0) {
			jsstr8_t host_out;

			host_out.bytes = result_ptr->bytes + result_ptr->len;
			host_out.len = 0;
			host_out.cap = result_ptr->cap - result_ptr->len;
			if (jsurl_serialize_host_parts(parts, &host_out) < 0) {
				return -1;
			}
			result_ptr->len += host_out.len;
		}
		{
			jsstr8_t path_out;

			jsstr8_init_from_buf(&path_out,
					(const char *)(result_ptr->bytes + result_ptr->len),
					result_ptr->cap - result_ptr->len);
			if (jsurl_serialize_pathname_wire_parts(parts, &path_out) < 0) {
				return -1;
			}
			result_ptr->len += path_out.len;
		}
	} else {
		jsstr8_t path_out;

		jsstr8_init_from_buf(&path_out,
				(const char *)(result_ptr->bytes + result_ptr->len),
				result_ptr->cap - result_ptr->len);
		if (jsurl_serialize_pathname_wire_parts(parts, &path_out) < 0) {
			return -1;
		}
		result_ptr->len += path_out.len;
	}
	if (parts->search.len > 0) {
		jsstr8_t search_out;

		jsstr8_init_from_buf(&search_out,
				(const char *)(result_ptr->bytes + result_ptr->len),
				result_ptr->cap - result_ptr->len);
		if (jsurl_serialize_search_wire_parts(parts, &search_out) < 0) {
			return -1;
		}
		result_ptr->len += search_out.len;
	}
	if (parts->hash.len > 0) {
		jsstr8_t hash_out;

		jsstr8_init_from_buf(&hash_out,
				(const char *)(result_ptr->bytes + result_ptr->len),
				result_ptr->cap - result_ptr->len);
		if (jsurl_serialize_hash_wire_parts(parts, &hash_out) < 0) {
			return -1;
		}
		result_ptr->len += hash_out.len;
	}
	return 0;
}

static int jsurl_copy_sizes_from_parts(const jsurl_parts_t *parts,
		jsurl_sizes_t *sizes_ptr)
{
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = parts != NULL ? parts->pathname.len + 1 : 1;
	size_t search_cap = parts != NULL ? parts->search.len + 1 : 1;
	size_t hash_cap = parts != NULL ? parts->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;
	jsurl_parts_t normalized_parts;
	size_t params_need;
	size_t storage_need;

	if (sizes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_normalize_parts_copy(parts, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	if (jsurl_measure_derived(&normalized_parts, &sizes_ptr->host_cap,
			&sizes_ptr->origin_cap, &sizes_ptr->href_cap) < 0) {
		return -1;
	}
	if (jsurl_search_params_requirements(normalized_parts.search, &params_need,
			&storage_need) < 0) {
		return -1;
	}
	sizes_ptr->protocol_cap = normalized_parts.protocol.len;
	sizes_ptr->username_cap = normalized_parts.username.len;
	sizes_ptr->password_cap = normalized_parts.password.len;
	sizes_ptr->hostname_cap = normalized_parts.hostname.len;
	sizes_ptr->port_cap = normalized_parts.port.len;
	sizes_ptr->pathname_cap = normalized_parts.pathname.len;
	sizes_ptr->search_cap = normalized_parts.search.len;
	sizes_ptr->hash_cap = normalized_parts.hash.len;
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

static int jsurl_utf8_validate(jsstr8_t src)
{
	const uint8_t *bc = src.bytes;
	const uint8_t *bs = src.bytes != NULL ? src.bytes + src.len : NULL;
	uint32_t cp;
	int l;

	while (bc != NULL && bc < bs) {
		UTF8_CHAR(bc, bs, &cp, &l);
		if (l <= 0) {
			errno = EINVAL;
			return -1;
		}
		bc += l;
	}
	return 0;
}

static int jsurl_component_preserve_escape(jsurl_component_mode_t mode,
		uint8_t c)
{
	switch (mode) {
	case JSURL_COMPONENT_PATHNAME:
		return c == '/' || c == '?' || c == '#' || c == '.';
	case JSURL_COMPONENT_SEARCH:
		return c == '&' || c == '=' || c == '+' || c == '#';
	case JSURL_COMPONENT_HASH:
	default:
		return 0;
	}
}

static int jsurl_component_decoded_len(jsstr8_t src,
		jsurl_component_mode_t mode, size_t *len_ptr)
{
	size_t i = 0;
	size_t out_len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	while (i < src.len) {
		if (src.bytes[i] == '%') {
			uint8_t decoded;

			if (i + 2 >= src.len || !jsurl_is_hex_digit(src.bytes[i + 1])
					|| !jsurl_is_hex_digit(src.bytes[i + 2])) {
				errno = EINVAL;
				return -1;
			}
			decoded = (uint8_t)((jsurl_hex_value(src.bytes[i + 1]) << 4)
					| jsurl_hex_value(src.bytes[i + 2]));
			out_len += jsurl_component_preserve_escape(mode, decoded) ? 3 : 1;
			i += 3;
			continue;
		}
		out_len += 1;
		i += 1;
	}
	*len_ptr = out_len;
	return 0;
}

static int jsurl_component_decode_into(jsstr8_t src,
		jsurl_component_mode_t mode, jsstr8_t *result_ptr)
{
	size_t needed_len;
	size_t i = 0;
	size_t out_len = 0;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_component_decoded_len(src, mode, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	while (i < src.len) {
		if (src.bytes[i] == '%') {
			uint8_t decoded;

			decoded = (uint8_t)((jsurl_hex_value(src.bytes[i + 1]) << 4)
					| jsurl_hex_value(src.bytes[i + 2]));
			if (jsurl_component_preserve_escape(mode, decoded)) {
				result_ptr->bytes[out_len++] = '%';
				result_ptr->bytes[out_len++] = VALHEX((decoded >> 4) & 0x0f);
				result_ptr->bytes[out_len++] = VALHEX(decoded & 0x0f);
			} else {
				result_ptr->bytes[out_len++] = decoded;
			}
			i += 3;
			continue;
		}
		result_ptr->bytes[out_len++] = src.bytes[i++];
	}
	result_ptr->len = out_len;
	if (jsurl_utf8_validate(*result_ptr) < 0) {
		result_ptr->len = 0;
		return -1;
	}
	return 0;
}

static int jsurl_component_needs_wire_encode(jsurl_component_mode_t mode,
		uint8_t c)
{
	if (c <= 0x20 || c >= 0x7f || c == '"' || c == '%' || c == '<'
			|| c == '>' || c == '\\' || c == '`' || c == '{'
			|| c == '}') {
		return 1;
	}
	switch (mode) {
	case JSURL_COMPONENT_PATHNAME:
		return c == '#' || c == '?';
	case JSURL_COMPONENT_SEARCH:
		return c == '#';
	case JSURL_COMPONENT_HASH:
	default:
		return c == '#';
	}
}

static int jsurl_component_wire_measure(jsstr8_t src,
		jsurl_component_mode_t mode, size_t *len_ptr)
{
	size_t i = 0;
	size_t out_len = 0;

	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	while (i < src.len) {
		if (src.bytes[i] == '%' && i + 2 < src.len
				&& jsurl_is_hex_digit(src.bytes[i + 1])
				&& jsurl_is_hex_digit(src.bytes[i + 2])) {
			uint8_t decoded = (uint8_t)((jsurl_hex_value(src.bytes[i + 1]) << 4)
					| jsurl_hex_value(src.bytes[i + 2]));

			if (jsurl_component_preserve_escape(mode, decoded)) {
				out_len += 3;
				i += 3;
				continue;
			}
		}
		out_len += jsurl_component_needs_wire_encode(mode, src.bytes[i]) ? 3 : 1;
		i += 1;
	}
	*len_ptr = out_len;
	return 0;
}

static int jsurl_component_wire_serialize(jsstr8_t src,
		jsurl_component_mode_t mode, jsstr8_t *result_ptr)
{
	size_t needed_len;
	size_t i = 0;
	size_t out_len = 0;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_component_wire_measure(src, mode, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	while (i < src.len) {
		if (src.bytes[i] == '%' && i + 2 < src.len
				&& jsurl_is_hex_digit(src.bytes[i + 1])
				&& jsurl_is_hex_digit(src.bytes[i + 2])) {
			uint8_t decoded = (uint8_t)((jsurl_hex_value(src.bytes[i + 1]) << 4)
					| jsurl_hex_value(src.bytes[i + 2]));

			if (jsurl_component_preserve_escape(mode, decoded)) {
				result_ptr->bytes[out_len++] = '%';
				result_ptr->bytes[out_len++] = VALHEX((decoded >> 4) & 0x0f);
				result_ptr->bytes[out_len++] = VALHEX(decoded & 0x0f);
				i += 3;
				continue;
			}
		}
		if (jsurl_component_needs_wire_encode(mode, src.bytes[i])) {
			result_ptr->bytes[out_len++] = '%';
			result_ptr->bytes[out_len++] = VALHEX((src.bytes[i] >> 4) & 0x0f);
			result_ptr->bytes[out_len++] = VALHEX(src.bytes[i] & 0x0f);
		} else {
			result_ptr->bytes[out_len++] = src.bytes[i];
		}
		i += 1;
	}
	result_ptr->len = out_len;
	return 0;
}

static int jsurl_query_internal_needs_escape(uint8_t c)
{
	return c == '&' || c == '=' || c == '+' || c == '#' || c == '%';
}

static int jsurl_search_params_internal_measure(const jsurl_search_params_t *params,
		size_t *len_ptr)
{
	size_t i;
	size_t len = 0;

	if (params == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		size_t j;

		if (i > 0) {
			len += 1;
		}
		for (j = 0; j < params->params[i].name.len; j++) {
			len += jsurl_query_internal_needs_escape(
					params->params[i].name.bytes[j]) ? 3 : 1;
		}
		len += 1;
		for (j = 0; j < params->params[i].value.len; j++) {
			len += jsurl_query_internal_needs_escape(
					params->params[i].value.bytes[j]) ? 3 : 1;
		}
	}
	*len_ptr = len;
	return 0;
}

static int jsurl_search_params_internal_serialize(const jsurl_search_params_t *params,
		jsstr8_t *result_ptr)
{
	size_t i;
	size_t needed_len;
	size_t out_len = 0;

	if (params == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_internal_measure(params, &needed_len) < 0) {
		return -1;
	}
	if (needed_len > result_ptr->cap) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < params->len; i++) {
		size_t j;

		if (i > 0) {
			result_ptr->bytes[out_len++] = '&';
		}
		for (j = 0; j < params->params[i].name.len; j++) {
			uint8_t c = params->params[i].name.bytes[j];

			if (jsurl_query_internal_needs_escape(c)) {
				result_ptr->bytes[out_len++] = '%';
				result_ptr->bytes[out_len++] = VALHEX((c >> 4) & 0x0f);
				result_ptr->bytes[out_len++] = VALHEX(c & 0x0f);
			} else {
				result_ptr->bytes[out_len++] = c;
			}
		}
		result_ptr->bytes[out_len++] = '=';
		for (j = 0; j < params->params[i].value.len; j++) {
			uint8_t c = params->params[i].value.bytes[j];

			if (jsurl_query_internal_needs_escape(c)) {
				result_ptr->bytes[out_len++] = '%';
				result_ptr->bytes[out_len++] = VALHEX((c >> 4) & 0x0f);
				result_ptr->bytes[out_len++] = VALHEX(c & 0x0f);
			} else {
				result_ptr->bytes[out_len++] = c;
			}
		}
	}
	result_ptr->len = out_len;
	return 0;
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
	if (jsurl_search_params_internal_measure(preview, &query_len) < 0) {
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
	if (jsurl_serialize_host_parts(&parts, &url->host) < 0) {
		return -1;
	}
	if (jsurl_serialize_origin_parts(&parts, &url->origin) < 0) {
		return -1;
	}
	return jsurl_serialize_href_parts(&parts, &url->href);
}

static int jsurl_sync_search_from_params(jsurl_t *url)
{
	size_t query_len;
	jsstr8_t query_out;

	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_search_params_internal_measure(&url->search_params, &query_len) < 0) {
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
		if (jsurl_search_params_internal_serialize(&url->search_params,
				&query_out) < 0) {
			return -1;
		}
		url->search.len = query_out.len + 1;
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
	jsstr8_t decoded_body;

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
	jsstr8_init_from_buf(&decoded_body,
			(const char *)(result_ptr->bytes + 1), result_ptr->cap - 1);
	if (jsurl_component_decode_into(body, JSURL_COMPONENT_SEARCH,
			&decoded_body) < 0) {
		return -1;
	}
	result_ptr->len = decoded_body.len + 1;
	return 0;
}

static int jsurl_normalize_hash_copy(jsstr8_t *result_ptr, jsstr8_t input)
{
	jsstr8_t body = input.len > 0 && input.bytes[0] == '#'
		? jsurl_slice(input, 1, -1)
		: input;
	jsstr8_t decoded_body;

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
	jsstr8_init_from_buf(&decoded_body,
			(const char *)(result_ptr->bytes + 1), result_ptr->cap - 1);
	if (jsurl_component_decode_into(body, JSURL_COMPONENT_HASH,
			&decoded_body) < 0) {
		return -1;
	}
	result_ptr->len = decoded_body.len + 1;
	return 0;
}

static int jsurl_normalize_pathname_structural_copy(jsstr8_t *result_ptr,
		jsstr8_t input, int has_authority)
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

static int jsurl_normalize_pathname_copy(jsstr8_t *result_ptr, jsstr8_t input,
		int has_authority)
{
	size_t decoded_cap = input.len > 0 ? input.len : 1;
	uint8_t decoded_buf[decoded_cap];
	jsstr8_t decoded;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&decoded, (const char *)decoded_buf, decoded_cap);
	if (input.len > 0
			&& jsurl_component_decode_into(input, JSURL_COMPONENT_PATHNAME,
				&decoded) < 0) {
		return -1;
	}
	return jsurl_normalize_pathname_structural_copy(result_ptr, decoded,
			has_authority);
}

static int jsurl_normalize_parts_copy(const jsurl_parts_t *parts,
		jsurl_parts_t *normalized_parts, jsstr8_t *hostname_storage,
		jsstr8_t *pathname_storage, jsstr8_t *search_storage,
		jsstr8_t *hash_storage)
{
	if (parts == NULL || normalized_parts == NULL || hostname_storage == NULL
			|| pathname_storage == NULL || search_storage == NULL
			|| hash_storage == NULL) {
		errno = EINVAL;
		return -1;
	}
	*normalized_parts = *parts;
	if (jsurl_normalize_hostname_copy(hostname_storage, parts->hostname) < 0) {
		return -1;
	}
	if (jsurl_normalize_pathname_copy(pathname_storage, parts->pathname,
			parts->has_authority) < 0) {
		return -1;
	}
	if (jsurl_normalize_search_copy(search_storage, parts->search) < 0) {
		return -1;
	}
	if (jsurl_normalize_hash_copy(hash_storage, parts->hash) < 0) {
		return -1;
	}
	normalized_parts->hostname = *hostname_storage;
	normalized_parts->pathname = *pathname_storage;
	normalized_parts->search = *search_storage;
	normalized_parts->hash = *hash_storage;
	return 0;
}

static int jsurl_commit_parts(jsurl_t *url, const jsurl_parts_t *parts)
{
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = parts != NULL ? parts->pathname.len + 1 : 1;
	size_t search_cap = parts != NULL ? parts->search.len + 1 : 1;
	size_t hash_cap = parts != NULL ? parts->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;
	jsurl_parts_t normalized_parts;
	size_t params_need;
	size_t storage_need;
	size_t host_len;
	size_t origin_len;
	size_t href_len;

	if (url == NULL || parts == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_normalize_parts_copy(parts, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	if (normalized_parts.protocol.len > url->protocol.cap
			|| normalized_parts.username.len > url->username.cap
			|| normalized_parts.password.len > url->password.cap
			|| normalized_parts.hostname.len > url->hostname.cap
			|| normalized_parts.port.len > url->port.cap
			|| normalized_parts.pathname.len > url->pathname.cap
			|| normalized_parts.search.len > url->search.cap
			|| normalized_parts.hash.len > url->hash.cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_search_params_requirements(normalized_parts.search, &params_need,
			&storage_need) < 0) {
		return -1;
	}
	if (params_need > url->search_params.cap
			|| storage_need > url->search_params.storage.cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (jsurl_measure_derived(&normalized_parts, &host_len, &origin_len,
			&href_len) < 0) {
		return -1;
	}
	if (host_len > url->host.cap || origin_len > url->origin.cap
			|| href_len > url->href.cap) {
		errno = ENOBUFS;
		return -1;
	}

	jsurl_buffer_copy(&url->protocol, normalized_parts.protocol);
	jsurl_buffer_copy(&url->username, normalized_parts.username);
	jsurl_buffer_copy(&url->password, normalized_parts.password);
	jsurl_buffer_copy(&url->hostname, normalized_parts.hostname);
	jsurl_buffer_copy(&url->port, normalized_parts.port);
	jsurl_buffer_copy(&url->pathname, normalized_parts.pathname);
	jsurl_buffer_copy(&url->search, normalized_parts.search);
	jsurl_buffer_copy(&url->hash, normalized_parts.hash);
	url->has_authority = normalized_parts.has_authority;

	jsurl_search_params_clear(&url->search_params);
	if (jsurl_search_params_parse_no_sync(&url->search_params,
			normalized_parts.search) < 0) {
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

static int jsurl_view_normalize_parts(const jsurl_view_t *view,
		jsurl_parts_t *normalized_parts, jsstr8_t *hostname_storage,
		jsstr8_t *pathname_storage, jsstr8_t *search_storage,
		jsstr8_t *hash_storage)
{
	jsurl_parts_t parts;

	if (view == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_view_parts(view, &parts);
	return jsurl_normalize_parts_copy(&parts, normalized_parts, hostname_storage,
			pathname_storage, search_storage, hash_storage);
}

int jsurl_view_hostname_display_measure(const jsurl_view_t *view,
		size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_hostname_display_measure_value(normalized_parts.hostname,
			len_ptr);
}

int jsurl_view_hostname_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_hostname_display_serialize_value(normalized_parts.hostname,
			result_ptr);
}

int jsurl_view_host_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;
	size_t host_len;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	if (jsurl_measure_derived(&normalized_parts, &host_len, NULL, NULL) < 0) {
		return -1;
	}
	*len_ptr = host_len;
	return 0;
}

int jsurl_view_host_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_host_parts(&normalized_parts, result_ptr);
}

int jsurl_view_host_display_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_host_display_measure_parts(&normalized_parts, len_ptr);
}

int jsurl_view_host_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_host_display_parts(&normalized_parts, result_ptr);
}

int jsurl_view_origin_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;
	size_t origin_len;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	if (jsurl_measure_derived(&normalized_parts, NULL, &origin_len, NULL) < 0) {
		return -1;
	}
	*len_ptr = origin_len;
	return 0;
}

int jsurl_view_origin_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_origin_parts(&normalized_parts, result_ptr);
}

int jsurl_view_origin_display_measure(const jsurl_view_t *view,
		size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_origin_display_measure_parts(&normalized_parts, len_ptr);
}

int jsurl_view_origin_display_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_origin_display_parts(&normalized_parts, result_ptr);
}

int jsurl_view_pathname_wire_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_pathname_wire_measure_parts(&normalized_parts, len_ptr);
}

int jsurl_view_pathname_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_pathname_wire_parts(&normalized_parts, result_ptr);
}

int jsurl_view_search_wire_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_search_wire_measure_parts(&normalized_parts, len_ptr);
}

int jsurl_view_search_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_search_wire_parts(&normalized_parts, result_ptr);
}

int jsurl_view_hash_wire_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_hash_wire_measure_parts(&normalized_parts, len_ptr);
}

int jsurl_view_hash_wire_serialize(const jsurl_view_t *view,
		jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
			sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_hash_wire_parts(&normalized_parts, result_ptr);
}

int jsurl_view_href_measure(const jsurl_view_t *view, size_t *len_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;
	size_t href_len;

	if (view == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	if (jsurl_measure_derived(&normalized_parts, NULL, NULL, &href_len) < 0) {
		return -1;
	}
	*len_ptr = href_len;
	return 0;
}

int jsurl_view_href_serialize(const jsurl_view_t *view, jsstr8_t *result_ptr)
{
	jsurl_parts_t normalized_parts;
	uint8_t hostname_buf[JSURL_HOSTNAME_ASCII_MAX];
	size_t pathname_cap = view != NULL ? view->pathname.len + 1 : 1;
	size_t search_cap = view != NULL ? view->search.len + 1 : 1;
	size_t hash_cap = view != NULL ? view->hash.len + 1 : 1;
	uint8_t pathname_buf[pathname_cap];
	uint8_t search_buf[search_cap];
	uint8_t hash_buf[hash_cap];
	jsstr8_t normalized_hostname;
	jsstr8_t normalized_pathname;
	jsstr8_t normalized_search;
	jsstr8_t normalized_hash;

	if (view == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsstr8_init_from_buf(&normalized_hostname, (const char *)hostname_buf,
		sizeof(hostname_buf));
	jsstr8_init_from_buf(&normalized_pathname, (const char *)pathname_buf,
			pathname_cap);
	jsstr8_init_from_buf(&normalized_search, (const char *)search_buf,
			search_cap);
	jsstr8_init_from_buf(&normalized_hash, (const char *)hash_buf, hash_cap);
	if (jsurl_view_normalize_parts(view, &normalized_parts, &normalized_hostname,
			&normalized_pathname, &normalized_search, &normalized_hash) < 0) {
		return -1;
	}
	return jsurl_serialize_href_parts(&normalized_parts, result_ptr);
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

typedef struct jsurl_copy_sizes_ctx_s {
	jsurl_sizes_t *sizes_ptr;
} jsurl_copy_sizes_ctx_t;

static int jsurl_copy_sizes_parts_cb(const jsurl_parts_t *parts, void *opaque)
{
	jsurl_copy_sizes_ctx_t *ctx = (jsurl_copy_sizes_ctx_t *)opaque;

	return jsurl_copy_sizes_from_parts(parts, ctx->sizes_ptr);
}

int jsurl_copy_sizes_with_base(jsstr8_t input, const jsurl_t *base,
		jsurl_sizes_t *sizes_ptr)
{
	jsurl_copy_sizes_ctx_t ctx;

	if (base == NULL || sizes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	ctx.sizes_ptr = sizes_ptr;
	return jsurl_resolve_with_base(input, base, jsurl_copy_sizes_parts_cb,
			&ctx);
}

static void jsurl_region_string_init(jsstr8_t *value, uint8_t *buf, size_t cap)
{
	if (cap == 0) {
		jsstr8_init(value);
		return;
	}
	jsstr8_init_from_buf(value, (const char *)buf, cap);
}

static int jsurl_region_measure_sizes(const jsval_region_t *region,
		size_t *used_ptr, const jsurl_sizes_t *sizes)
{
	if (region == NULL || used_ptr == NULL || sizes == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (sizes->search_params_cap > 0
			&& sizes->search_params_cap > SIZE_MAX / sizeof(jsurl_param_t)) {
		errno = EOVERFLOW;
		return -1;
	}

	if (sizes->search_params_cap > 0
			&& jsval_region_measure_alloc(region, used_ptr,
				sizes->search_params_cap * sizeof(jsurl_param_t),
				_Alignof(jsurl_param_t)) < 0) {
		return -1;
	}
#define JSURL_MEASURE_FIELD(_field) \
	do { \
		if ((sizes->_field) > 0 \
				&& jsval_region_measure_alloc(region, used_ptr, \
					(sizes->_field), 1) < 0) { \
			return -1; \
		} \
	} while (0)

	JSURL_MEASURE_FIELD(protocol_cap);
	JSURL_MEASURE_FIELD(username_cap);
	JSURL_MEASURE_FIELD(password_cap);
	JSURL_MEASURE_FIELD(hostname_cap);
	JSURL_MEASURE_FIELD(port_cap);
	JSURL_MEASURE_FIELD(pathname_cap);
	JSURL_MEASURE_FIELD(search_cap);
	JSURL_MEASURE_FIELD(hash_cap);
	JSURL_MEASURE_FIELD(host_cap);
	JSURL_MEASURE_FIELD(origin_cap);
	JSURL_MEASURE_FIELD(href_cap);
	JSURL_MEASURE_FIELD(search_params_storage_cap);
#undef JSURL_MEASURE_FIELD
	return 0;
}

static int jsurl_region_storage_alloc(jsval_region_t *region,
		const jsurl_sizes_t *sizes, jsurl_storage_t *storage)
{
	jsurl_param_t *params = NULL;
	uint8_t *buf = NULL;

	if (region == NULL || sizes == NULL || storage == NULL) {
		errno = EINVAL;
		return -1;
	}
	memset(storage, 0, sizeof(*storage));

	if (sizes->search_params_cap > 0) {
		if (jsval_region_alloc(region,
				sizes->search_params_cap * sizeof(jsurl_param_t),
				_Alignof(jsurl_param_t), (void **)&params) < 0) {
			return -1;
		}
	}
	storage->search_params.params = params;
	storage->search_params.params_cap = sizes->search_params_cap;

#define JSURL_ALLOC_FIELD(_field, _capfield) \
	do { \
		buf = NULL; \
		if ((sizes->_capfield) > 0 \
				&& jsval_region_alloc(region, (sizes->_capfield), 1, \
					(void **)&buf) < 0) { \
			return -1; \
		} \
		jsurl_region_string_init(&storage->_field, buf, sizes->_capfield); \
	} while (0)

	JSURL_ALLOC_FIELD(protocol, protocol_cap);
	JSURL_ALLOC_FIELD(username, username_cap);
	JSURL_ALLOC_FIELD(password, password_cap);
	JSURL_ALLOC_FIELD(hostname, hostname_cap);
	JSURL_ALLOC_FIELD(port, port_cap);
	JSURL_ALLOC_FIELD(pathname, pathname_cap);
	JSURL_ALLOC_FIELD(search, search_cap);
	JSURL_ALLOC_FIELD(hash, hash_cap);
	JSURL_ALLOC_FIELD(host, host_cap);
	JSURL_ALLOC_FIELD(origin, origin_cap);
	JSURL_ALLOC_FIELD(href, href_cap);
	JSURL_ALLOC_FIELD(search_params.storage, search_params_storage_cap);
#undef JSURL_ALLOC_FIELD

	return 0;
}

static int jsurl_region_copy_parts(jsval_region_t *region,
		const jsurl_parts_t *parts, jsurl_t *url_ptr)
{
	jsurl_sizes_t sizes;
	jsurl_storage_t storage;
	size_t used;

	if (region == NULL || parts == NULL || url_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_copy_sizes_from_parts(parts, &sizes) < 0) {
		return -1;
	}
	used = region->used;
	if (jsurl_region_measure_sizes(region, &used, &sizes) < 0) {
		return -1;
	}
	if (jsurl_region_storage_alloc(region, &sizes, &storage) < 0) {
		return -1;
	}
	if (jsurl_init(url_ptr, &storage) < 0) {
		return -1;
	}
	return jsurl_commit_parts(url_ptr, parts);
}

static int jsurl_region_measure_parts(const jsval_region_t *region,
		const jsurl_parts_t *parts, size_t *bytes_ptr)
{
	jsurl_sizes_t sizes;
	size_t start_used;
	size_t used;

	if (region == NULL || parts == NULL || bytes_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsurl_copy_sizes_from_parts(parts, &sizes) < 0) {
		return -1;
	}
	start_used = region->used;
	used = start_used;
	if (jsurl_region_measure_sizes(region, &used, &sizes) < 0) {
		return -1;
	}
	*bytes_ptr = used - start_used;
	return 0;
}

static int jsurl_commit_parts_cb(const jsurl_parts_t *parts, void *opaque)
{
	jsurl_t *url = (jsurl_t *)opaque;

	return jsurl_commit_parts(url, parts);
}

typedef struct jsurl_region_measure_ctx_s {
	const jsval_region_t *region;
	size_t *bytes_ptr;
} jsurl_region_measure_ctx_t;

static int jsurl_region_measure_parts_cb(const jsurl_parts_t *parts, void *opaque)
{
	jsurl_region_measure_ctx_t *ctx = (jsurl_region_measure_ctx_t *)opaque;

	return jsurl_region_measure_parts(ctx->region, parts, ctx->bytes_ptr);
}

typedef struct jsurl_region_copy_ctx_s {
	jsval_region_t *region;
	jsurl_t *url_ptr;
} jsurl_region_copy_ctx_t;

static int jsurl_region_copy_parts_cb(const jsurl_parts_t *parts, void *opaque)
{
	jsurl_region_copy_ctx_t *ctx = (jsurl_region_copy_ctx_t *)opaque;

	return jsurl_region_copy_parts(ctx->region, parts, ctx->url_ptr);
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

static int jsurl_resolve_with_base(jsstr8_t input, const jsurl_t *base,
		jsurl_parts_callback_t callback, void *opaque)
{
	jsurl_parts_t parts;
	jsurl_parts_t base_parts;
	ssize_t scheme_i = jsurl_find_scheme_end(input);

	if (callback == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (scheme_i >= 0) {
		if (jsurl_parse_absolute_parts(input, &parts) < 0) {
			return -1;
		}
		return callback(&parts, opaque);
	}
	if (base == NULL) {
		errno = EINVAL;
		return -1;
	}

	jsurl_current_parts(base, &base_parts);
	parts = base_parts;
	if (input.len == 0) {
		return callback(&parts, opaque);
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
		return callback(&parts, opaque);
	}
	if (input.bytes[0] == '?') {
		jsstr8_t path_part;

		jsurl_split_path_search_hash(input, &path_part, &parts.search, &parts.hash);
		(void) path_part;
		return callback(&parts, opaque);
	}
	if (input.bytes[0] == '#') {
		jsstr8_t path_part;
		jsstr8_t search_part;

		jsurl_split_path_search_hash(input, &path_part, &search_part, &parts.hash);
		(void) path_part;
		(void) search_part;
		return callback(&parts, opaque);
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
			return callback(&parts, opaque);
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
			return callback(&parts, opaque);
		}
	}
}

int jsurl_parse_copy_with_base(jsurl_t *url, jsstr8_t input, const jsurl_t *base)
{
	if (url == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_resolve_with_base(input, base, jsurl_commit_parts_cb, url);
}

int jsurl_region_parse_copy_measure(const jsval_region_t *region, jsstr8_t input,
		size_t *bytes_ptr)
{
	jsurl_view_t view;
	jsurl_parts_t parts;

	if (jsurl_view_parse(&view, input) < 0) {
		return -1;
	}
	jsurl_view_parts(&view, &parts);
	return jsurl_region_measure_parts(region, &parts, bytes_ptr);
}

int jsurl_region_parse_copy(jsval_region_t *region, jsstr8_t input,
		jsurl_t *url_ptr)
{
	jsurl_view_t view;
	jsurl_parts_t parts;

	if (jsurl_view_parse(&view, input) < 0) {
		return -1;
	}
	jsurl_view_parts(&view, &parts);
	return jsurl_region_copy_parts(region, &parts, url_ptr);
}

int jsurl_region_parse_copy_with_base_measure(const jsval_region_t *region,
		jsstr8_t input, const jsurl_t *base, size_t *bytes_ptr)
{
	jsurl_region_measure_ctx_t ctx;

	ctx.region = region;
	ctx.bytes_ptr = bytes_ptr;
	return jsurl_resolve_with_base(input, base, jsurl_region_measure_parts_cb,
			&ctx);
}

int jsurl_region_parse_copy_with_base(jsval_region_t *region, jsstr8_t input,
		const jsurl_t *base, jsurl_t *url_ptr)
{
	jsurl_region_copy_ctx_t ctx;

	ctx.region = region;
	ctx.url_ptr = url_ptr;
	return jsurl_resolve_with_base(input, base, jsurl_region_copy_parts_cb,
			&ctx);
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

int jsurl_pathname_wire_measure(const jsurl_t *url, size_t *len_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_pathname_wire_measure_parts(&parts, len_ptr);
}

int jsurl_pathname_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_serialize_pathname_wire_parts(&parts, result_ptr);
}

int jsurl_search_wire_measure(const jsurl_t *url, size_t *len_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_search_wire_measure_parts(&parts, len_ptr);
}

int jsurl_search_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_serialize_search_wire_parts(&parts, result_ptr);
}

int jsurl_hash_wire_measure(const jsurl_t *url, size_t *len_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_hash_wire_measure_parts(&parts, len_ptr);
}

int jsurl_hash_wire_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_serialize_hash_wire_parts(&parts, result_ptr);
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

int jsurl_hostname_display_measure(const jsurl_t *url, size_t *len_ptr)
{
	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_hostname_display_measure_value(url->hostname, len_ptr);
}

int jsurl_hostname_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsurl_hostname_display_serialize_value(url->hostname, result_ptr);
}

int jsurl_host_display_measure(const jsurl_t *url, size_t *len_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_host_display_measure_parts(&parts, len_ptr);
}

int jsurl_host_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_serialize_host_display_parts(&parts, result_ptr);
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

int jsurl_origin_display_measure(const jsurl_t *url, size_t *len_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_origin_display_measure_parts(&parts, len_ptr);
}

int jsurl_origin_display_serialize(const jsurl_t *url, jsstr8_t *result_ptr)
{
	jsurl_parts_t parts;

	if (url == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	jsurl_current_parts(url, &parts);
	return jsurl_serialize_origin_display_parts(&parts, result_ptr);
}

jsstr8_t jsurl_to_json(const jsurl_t *url)
{
	if (url == NULL) {
		return jsstr8_empty;
	}
	return url->href;
}
