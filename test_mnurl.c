#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mnurl.h"

void test_urlsearchparams_init() {
    urlsearchparams_t searchParams;
    declare_jsstr8(search, "key1=value1&key2=value2");
    urlsearchparams_init(&searchParams, search);

    assert(searchParams.len == 2);
    declare_jsstr8(key1, "key1");
    declare_jsstr8(value1, "value1");
    declare_jsstr8(key2, "key2");
    declare_jsstr8(value2, "value2");
    assert(jsstr8_cmp(&searchParams.params[0].key, &key1) == 0);
    assert(jsstr8_cmp(&searchParams.params[0].value, &value1) == 0);
    assert(jsstr8_cmp(&searchParams.params[1].key, &key2) == 0);
    assert(jsstr8_cmp(&searchParams.params[1].value, &value2) == 0);
}

void test_urlsearchparams_append() {
    urlsearchparams_t searchParams;
    declare_jsstr8(key, "key");
    declare_jsstr8(value, "value");
    urlsearchparams_init(&searchParams, jsstr8_empty);

    urlsearchparams_append(&searchParams, key, value);

    assert(searchParams.len == 1);
    assert(jsstr8_cmp(&searchParams.params[0].key, &key) == 0);
    assert(jsstr8_cmp(&searchParams.params[0].value, &value) == 0);
}

void test_urlsearchparams_delete() {
    urlsearchparams_t searchParams;
    declare_jsstr8(search, "key1=value1&key2=value2");
    urlsearchparams_init(&searchParams, search);

    urlsearchparams_delete(&searchParams, jsstr8_from_str("key1"));

    declare_jsstr8(key2, "key2");
    declare_jsstr8(value2, "value2");
    assert(searchParams.len == 1);
    assert(jsstr8_cmp(&searchParams.params[0].key, &key2) == 0);
    assert(jsstr8_cmp(&searchParams.params[0].value, &value2) == 0);
}

void test_urlsearchparams_get() {
    urlsearchparams_t searchParams;
    declare_jsstr8(search, "key1=value1&key2=value2");
    urlsearchparams_init(&searchParams, search);

    declare_jsstr8(key, "key2");
    declare_jsstr8(value, "value2");

    jsstr8_t got_value = urlsearchparams_get(&searchParams, key);

    assert(jsstr8_cmp(&got_value, &value) == 0);
}

void test_url_init() {
    url_t url;
    declare_jsstr8(href, "http://username:password@host:8080/path?key=value#hash");

    url_init(&url, href);

    declare_jsstr8(protocol, "http");
    declare_jsstr8(username, "username");
    declare_jsstr8(password, "password");
    declare_jsstr8(host, "host");
    declare_jsstr8(port, "8080");
    declare_jsstr8(pathname, "/path");
    declare_jsstr8(search, "key=value");
    declare_jsstr8(hash, "hash");

    assert(jsstr8_cmp(&url.protocol, &protocol) == 0);
    assert(jsstr8_cmp(&url.username, &username) == 0);
    assert(jsstr8_cmp(&url.password, &password) == 0);
    assert(jsstr8_cmp(&url.host, &host) == 0);
    assert(jsstr8_cmp(&url.port, &port) == 0);
    assert(jsstr8_cmp(&url.pathname, &pathname) == 0);
    assert(jsstr8_cmp(&url.search, &search) == 0);
    assert(jsstr8_cmp(&url.hash, &hash) == 0);
}

int main() {
    test_urlsearchparams_init();
    test_urlsearchparams_append();
    test_urlsearchparams_delete();
    test_urlsearchparams_get();
    test_url_init();

    printf("All tests passed.\n");
    return 0;
}
