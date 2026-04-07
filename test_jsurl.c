#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "jsurl.h"

typedef struct test_url_buffers_s {
	uint8_t protocol[64];
	uint8_t username[64];
	uint8_t password[64];
	uint8_t hostname[128];
	uint8_t port[32];
	uint8_t pathname[256];
	uint8_t search[256];
	uint8_t hash[128];
	uint8_t host[160];
	uint8_t origin[256];
	uint8_t href[512];
	jsurl_param_t params[16];
	uint8_t params_storage_buf[256];
} test_url_buffers_t;

typedef struct test_search_params_buffers_s {
	jsurl_param_t params[16];
	uint8_t storage_buf[256];
} test_search_params_buffers_t;

static void test_expect_jsstr(jsstr8_t actual, const char *expected)
{
	jsstr8_t expected_str;

	jsstr8_init_from_str(&expected_str, expected);
	assert(jsstr8_u8_cmp(&actual, &expected_str) == 0);
}

static void test_make_empty(jsstr8_t *value, uint8_t *buf, size_t cap)
{
	jsstr8_init_from_buf(value, (const char *) buf, cap);
}

static void test_url_init(jsurl_t *url, test_url_buffers_t *buffers)
{
	jsurl_storage_t storage;

	memset(buffers, 0, sizeof(*buffers));
	test_make_empty(&storage.protocol, buffers->protocol, sizeof(buffers->protocol));
	test_make_empty(&storage.username, buffers->username, sizeof(buffers->username));
	test_make_empty(&storage.password, buffers->password, sizeof(buffers->password));
	test_make_empty(&storage.hostname, buffers->hostname, sizeof(buffers->hostname));
	test_make_empty(&storage.port, buffers->port, sizeof(buffers->port));
	test_make_empty(&storage.pathname, buffers->pathname, sizeof(buffers->pathname));
	test_make_empty(&storage.search, buffers->search, sizeof(buffers->search));
	test_make_empty(&storage.hash, buffers->hash, sizeof(buffers->hash));
	test_make_empty(&storage.host, buffers->host, sizeof(buffers->host));
	test_make_empty(&storage.origin, buffers->origin, sizeof(buffers->origin));
	test_make_empty(&storage.href, buffers->href, sizeof(buffers->href));
	storage.search_params.params = buffers->params;
	storage.search_params.params_cap =
		sizeof(buffers->params) / sizeof(buffers->params[0]);
	test_make_empty(&storage.search_params.storage, buffers->params_storage_buf,
		sizeof(buffers->params_storage_buf));

	assert(jsurl_init(url, &storage) == 0);
}

static void test_search_params_init(jsurl_search_params_t *params,
		test_search_params_buffers_t *buffers)
{
	jsurl_search_params_storage_t storage;

	memset(buffers, 0, sizeof(*buffers));
	storage.params = buffers->params;
	storage.params_cap = sizeof(buffers->params) / sizeof(buffers->params[0]);
	test_make_empty(&storage.storage, buffers->storage_buf,
		sizeof(buffers->storage_buf));
	assert(jsurl_search_params_init(params, &storage) == 0);
}

static void test_jsurl_view_parse(void)
{
	jsurl_view_t view;
	jsurl_sizes_t sizes;
	uint8_t host_buf[128];
	uint8_t origin_buf[256];
	uint8_t href_buf[256];
	uint8_t params_buf[128];
	jsstr8_t host;
	jsstr8_t origin;
	jsstr8_t href;
	jsstr8_t params;

	jsstr8_init_from_buf(&host, (const char *) host_buf, sizeof(host_buf));
	jsstr8_init_from_buf(&origin, (const char *) origin_buf, sizeof(origin_buf));
	jsstr8_init_from_buf(&href, (const char *) href_buf, sizeof(href_buf));
	jsstr8_init_from_buf(&params, (const char *) params_buf, sizeof(params_buf));

	declare_jsstr8(input,
		"https://user:pass@example.com:8443/a/b?x=1&y=two+words#frag");
	assert(jsurl_view_parse(&view, input) == 0);

	test_expect_jsstr(view.protocol, "https:");
	test_expect_jsstr(view.username, "user");
	test_expect_jsstr(view.password, "pass");
	test_expect_jsstr(view.hostname, "example.com");
	test_expect_jsstr(view.port, "8443");
	test_expect_jsstr(view.pathname, "/a/b");
	test_expect_jsstr(view.search, "?x=1&y=two+words");
	test_expect_jsstr(view.hash, "#frag");

	assert(jsurl_view_host_serialize(&view, &host) == 0);
	assert(jsurl_view_origin_serialize(&view, &origin) == 0);
	assert(jsurl_view_href_serialize(&view, &href) == 0);
	assert(jsurl_search_params_view_serialize(&view.search_params, &params) == 0);
	test_expect_jsstr(host, "example.com:8443");
	test_expect_jsstr(origin, "https://example.com:8443");
	test_expect_jsstr(href,
		"https://user:pass@example.com:8443/a/b?x=1&y=two+words#frag");
	test_expect_jsstr(params, "x=1&y=two+words");
	assert(jsurl_search_params_view_size(&view.search_params) == 2);

	assert(jsurl_copy_sizes(&view, &sizes) == 0);
	assert(sizes.protocol_cap == strlen("https:"));
	assert(sizes.origin_cap == strlen("https://example.com:8443"));
	assert(sizes.href_cap ==
		strlen("https://user:pass@example.com:8443/a/b?x=1&y=two+words#frag"));
	assert(sizes.search_params_cap == 2);
}

static void test_jsurl_search_params_roundtrip(void)
{
	jsurl_search_params_t params;
	test_search_params_buffers_t buffers;
	jsstr8_t value;
	jsstr8_t values[4];
	size_t values_len = 4;
	size_t found_len = 0;
	int found = 0;
	uint8_t out_buf[256];
	jsstr8_t out;

	test_search_params_init(&params, &buffers);
	jsstr8_init_from_buf(&out, (const char *) out_buf, sizeof(out_buf));

	declare_jsstr8(search, "?a=1&b=two+words&encoded=%2F");
	assert(jsurl_search_params_parse(&params, search) == 0);
	assert(jsurl_search_params_size(&params) == 3);

	declare_jsstr8(b_name, "b");
	assert(jsurl_search_params_get(&params, b_name, &value, &found) == 0);
	assert(found == 1);
	test_expect_jsstr(value, "two words");

	declare_jsstr8(a_name, "a");
	declare_jsstr8(a_value2, "3");
	assert(jsurl_search_params_append(&params, a_name, a_value2) == 0);
	assert(jsurl_search_params_get_all(&params, a_name, values, &values_len,
			&found_len) == 0);
	assert(values_len == 2);
	assert(found_len == 2);
	test_expect_jsstr(values[0], "1");
	test_expect_jsstr(values[1], "3");

	declare_jsstr8(new_b_value, "x y");
	assert(jsurl_search_params_set(&params, b_name, new_b_value) == 0);
	assert(jsurl_search_params_delete_value(&params, a_name, values[0]) == 0);
	assert(jsurl_search_params_sort(&params) == 0);
	assert(jsurl_search_params_serialize(&params, &out) == 0);
	test_expect_jsstr(out, "a=3&b=x+y&encoded=%2F");
}

static void test_jsurl_copy_and_relative_parse(void)
{
	jsurl_view_t view;
	jsurl_t base;
	jsurl_t url;
	test_url_buffers_t base_buffers;
	test_url_buffers_t url_buffers;

	test_url_init(&base, &base_buffers);
	test_url_init(&url, &url_buffers);

	declare_jsstr8(input,
		"https://user:pass@example.com:8443/a/b?x=1&y=two+words#frag");
	assert(jsurl_view_parse(&view, input) == 0);
	assert(jsurl_copy_from_view(&url, &view) == 0);
	test_expect_jsstr(url.protocol, "https:");
	test_expect_jsstr(url.host, "example.com:8443");
	test_expect_jsstr(url.origin, "https://example.com:8443");
	test_expect_jsstr(url.href,
		"https://user:pass@example.com:8443/a/b?x=1&y=two+words#frag");

	declare_jsstr8(base_input, "https://example.com/dir/sub/index.html?x=1#old");
	assert(jsurl_parse_copy(&base, base_input) == 0);

	declare_jsstr8(relative_input, "../up?q=2");
	assert(jsurl_parse_copy_with_base(&url, relative_input, &base) == 0);
	test_expect_jsstr(url.href, "https://example.com/dir/up?q=2");
	test_expect_jsstr(url.pathname, "/dir/up");
	test_expect_jsstr(url.search, "?q=2");
	assert(url.hash.len == 0);

	declare_jsstr8(query_only, "?z=3#new");
	assert(jsurl_parse_copy_with_base(&url, query_only, &base) == 0);
	test_expect_jsstr(url.href, "https://example.com/dir/sub/index.html?z=3#new");
	test_expect_jsstr(url.pathname, "/dir/sub/index.html");
}

static void test_jsurl_setters_and_sync(void)
{
	jsurl_t url;
	test_url_buffers_t buffers;

	test_url_init(&url, &buffers);

	declare_jsstr8(input, "https://example.com/base");
	assert(jsurl_parse_copy(&url, input) == 0);

	declare_jsstr8(search, "?a=1&b=2");
	assert(jsurl_set_search(&url, search) == 0);
	declare_jsstr8(c_name, "c");
	declare_jsstr8(c_value, "3");
	assert(jsurl_search_params_append(&url.search_params, c_name, c_value) == 0);
	test_expect_jsstr(url.search, "?a=1&b=2&c=3");
	test_expect_jsstr(url.href, "https://example.com/base?a=1&b=2&c=3");

	declare_jsstr8(hash, "section");
	assert(jsurl_set_hash(&url, hash) == 0);
	test_expect_jsstr(url.hash, "#section");
	test_expect_jsstr(url.href, "https://example.com/base?a=1&b=2&c=3#section");

	declare_jsstr8(protocol, "http");
	assert(jsurl_set_protocol(&url, protocol) == 0);
	declare_jsstr8(hostname, "example.org");
	assert(jsurl_set_hostname(&url, hostname) == 0);
	test_expect_jsstr(url.origin, "http://example.org");
	test_expect_jsstr(url.href, "http://example.org/base?a=1&b=2&c=3#section");
}

static void test_jsurl_errors(void)
{
	jsurl_t url;
	jsurl_view_t view;
	jsurl_storage_t storage;
	jsurl_search_params_t params;
	jsurl_search_params_storage_t params_storage;
	uint8_t protocol[16];
	uint8_t username[16];
	uint8_t password[16];
	uint8_t hostname[32];
	uint8_t port[16];
	uint8_t pathname[16];
	uint8_t search[16];
	uint8_t hash[16];
	uint8_t host[16];
	uint8_t origin[16];
	uint8_t href[16];
	jsurl_param_t param_entries[1];
	uint8_t param_storage_buf[4];

	memset(protocol, 0, sizeof(protocol));
	memset(username, 0, sizeof(username));
	memset(password, 0, sizeof(password));
	memset(hostname, 0, sizeof(hostname));
	memset(port, 0, sizeof(port));
	memset(pathname, 0, sizeof(pathname));
	memset(search, 0, sizeof(search));
	memset(hash, 0, sizeof(hash));
	memset(host, 0, sizeof(host));
	memset(origin, 0, sizeof(origin));
	memset(href, 0, sizeof(href));
	memset(param_entries, 0, sizeof(param_entries));
	memset(param_storage_buf, 0, sizeof(param_storage_buf));

	test_make_empty(&storage.protocol, protocol, sizeof(protocol));
	test_make_empty(&storage.username, username, sizeof(username));
	test_make_empty(&storage.password, password, sizeof(password));
	test_make_empty(&storage.hostname, hostname, sizeof(hostname));
	test_make_empty(&storage.port, port, sizeof(port));
	test_make_empty(&storage.pathname, pathname, sizeof(pathname));
	test_make_empty(&storage.search, search, sizeof(search));
	test_make_empty(&storage.hash, hash, sizeof(hash));
	test_make_empty(&storage.host, host, sizeof(host));
	test_make_empty(&storage.origin, origin, sizeof(origin));
	test_make_empty(&storage.href, href, sizeof(href));
	storage.search_params.params = param_entries;
	storage.search_params.params_cap =
		sizeof(param_entries) / sizeof(param_entries[0]);
	test_make_empty(&storage.search_params.storage, param_storage_buf,
		sizeof(param_storage_buf));

	assert(jsurl_init(&url, &storage) == 0);
	declare_jsstr8(long_input, "https://example.com/this/path/is/too/long");
	errno = 0;
	assert(jsurl_parse_copy(&url, long_input) == -1);
	assert(errno == ENOBUFS);

	params_storage.params = param_entries;
	params_storage.params_cap = sizeof(param_entries) / sizeof(param_entries[0]);
	test_make_empty(&params_storage.storage, param_storage_buf,
		sizeof(param_storage_buf));
	assert(jsurl_search_params_init(&params, &params_storage) == 0);
	declare_jsstr8(big_query, "?a=12345");
	errno = 0;
	assert(jsurl_search_params_parse(&params, big_query) == -1);
	assert(errno == ENOBUFS);

	errno = 0;
	declare_jsstr8(relative, "../oops");
	assert(jsurl_view_parse(&view, relative) == -1);
	assert(errno == EINVAL);
}

int main(void)
{
	test_jsurl_view_parse();
	test_jsurl_search_params_roundtrip();
	test_jsurl_copy_and_relative_parse();
	test_jsurl_setters_and_sync();
	test_jsurl_errors();

	printf("All tests passed.\n");
	return 0;
}
