#include <string.h>

#include "mnurl.h"

void urlsearchparams_init(urlsearchparams_t *searchParams, jsstr8_t search) {
    searchParams->cap = 32;
    searchParams->len = 0;

    /* parse search to fill in the searchParams */
    /* use jsstr8_index{of,token}() to find the next delimiter */
    /* use jsstr8_slice() to assign the fields */
    /* use urlsearchparams_append() to append the fields */

    size_t cursor_i = 0;
    size_t amp_i = 0;
    size_t eq_i = 0;
    jsstr8_t param;
    jsstr8_t key;
    jsstr8_t value;
    static wchar_t param_term[] = {'&', ';', 0};
    for (;;) {
        /* init param */
        amp_i = jsstr8_indextoken(&search, param_term, 2, cursor_i);
        if (amp_i < 0) {
            amp_i = -amp_i;
        }
        jsstr8_slice(&param, &search, cursor_i, amp_i);

        /* init key */
        eq_i = jsstr8_indexof(&param, '=', cursor_i);
        if (eq_i < 0) {
            eq_i = -eq_i;
        }
        jsstr8_slice(&key, &search, cursor_i, eq_i);

        /* init value */
        jsstr8_slice(&value, &search, eq_i + 1, amp_i);

        /* append param */
        urlsearchparams_append(searchParams, key, value);
        cursor_i = amp_i + 1;
    }
}

void urlsearchparams_append(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value) {
    if (searchParams->len < searchParams->cap) {
        searchParams->params[searchParams->len].key = key;
        searchParams->params[searchParams->len].value = value;
        searchParams->len++;
    }
}

void urlsearchparams_delete(urlsearchparams_t *searchParams, jsstr8_t key) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].key, &key) == 0) {
            for (size_t j = i; j < searchParams->len - 1; j++) {
                searchParams->params[j] = searchParams->params[j + 1];
            }
            searchParams->len--;
            break;
        }
    }
}

void urlsearchparams_deletevalue(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].key, &key) == 0 && jsstr8_cmp(&searchParams->params[i].value, &value) == 0) {
            for (size_t j = i; j < searchParams->len - 1; j++) {
                searchParams->params[j] = searchParams->params[j + 1];
            }
            searchParams->len--;
            break;
        }
    }
}

jsstr8_t urlsearchparams_get(urlsearchparams_t *searchParams, jsstr8_t key) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].key, &key) == 0) {
            return searchParams->params[i].value;
        }
    }
    return (jsstr8_t) {0};
}

void urlsearchparams_getAll(urlsearchparams_t *searchParams, jsstr8_t key, urlparams_t *params) {
    return searchParams->params;
}

int urlsearchparams_has(urlsearchparams_t *searchParams, jsstr8_t key) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].key, &key) == 0) {
            return 1;
        }
    }
    return 0;
}

void urlsearchparams_set(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].key, &key) == 0) {
            searchParams->params[i].value = value;
            return;
        }
    }
    urlsearchparams_append(searchParams, key, value);
}

void urlsearchparams_sort(urlsearchparams_t *searchParams) {
    for (size_t i = 0; i < searchParams->len; i++) {
        for (size_t j = i + 1; j < searchParams->len; j++) {
            if (jsstr8_cmp(&searchParams->params[i].key, &searchParams->params[j].key) > 0) {
                urlparams_t tmp = searchParams->params[i];
                searchParams->params[i] = searchParams->params[j];
                searchParams->params[j] = tmp;
            }
        }
    }
}

void url_init(url_t *url, jsstr8_t href) {
    ssize_t protocol_i;
    uint8_t *authority_b1;
    uint8_t *authority_b2;
    size_t authority_i;
    ssize_t at_i;
    ssize_t colon_i;
    size_t host_i;
    ssize_t port_i;
    ssize_t path_i;
    ssize_t search_i;
    ssize_t hash_i;

    memset(url, 0, sizeof(url_t));

    url->href = href;
    /* parse href to point into the other fields, using jsstr8_indexof() to find the next delimiter */
    /* use jsstr8_slice() to assign the fields */
    /* use urlsearchparams_init() to initialize searchParams */

    protocol_i = jsstr8_indexof(&href, ':', 0);
    if (protocol_i < 0) {
        return;
    }
    
    jsstr8_slice(&url->protocol, &href, 0, protocol_i);

    /* verify the two authority slashes */
    authority_b1 = jsstr8_get_at(&href, protocol_i + 1);
    authority_b2 = jsstr8_get_at(&href, protocol_i + 2);
    if (authority_b1 != '/' && authority_b2 != '/') {
        return;
    }
    authority_i = protocol_i + 3;

    at_i = jsstr8_indexof(&href, '@', authority_i);
    if (at_i >= 0) {
        colon_i = jsstr8_indexof(&href, ':', at_i);
        if (colon_i >= 0) {
            jsstr8_slice(&url->username, &href, protocol_i + 3, colon_i);
            jsstr8_slice(&url->password, &href, colon_i + 1, at_i);
        } else {
            jsstr8_slice(&url->username, &href, protocol_i + 3, at_i);
        }
        host_i = at_i + 1;
    } else {
        host_i = authority_i;
    }

    port_i = jsstr8_indexof(&href, ':', host_i);
    if (port_i >= 0) {
        jsstr8_slice(&url->host, &href, host_i, port_i);
    } else {
        port_i = host_i;
    }

    path_i = jsstr8_indexof(&href, '/', host_i);
    if (path_i >= 0) {
        jsstr8_slice(&url->host, &href, host_i, path_i);
    } else {
        path_i = -path_i; /* end of search */
        jsstr8_slice(&url->host, &href, host_i, path_i);
        return;
    }
    search_i = jsstr8_indexof(&href, '?', path_i);
    if (search_i != -1) {
        jsstr8_slice(&url->pathname, &href, search_i, path_i);
        hash_i = jsstr8_indexof(&href, '#', search_i);
        if (hash_i >= 0) {
            jsstr8_slice(&url->search, &href, search_i, hash_i);
            jsstr8_slice(&url->hash, &href, hash_i, -1);
        } else {
            jsstr8_slice(&url->search, &href, search_i, -1);
        }
    } else {
        hash_i = jsstr8_indexof(&href, '#', path_i);
        if (hash_i >= 0) {
            jsstr8_slice(&url->pathname, &href, path_i, hash_i);
            jsstr8_slice(&url->hash, &href, hash_i, -1);
        } else {
            jsstr8_slice(&url->pathname, &href, path_i, -1);
        }
    }
    urlsearchparams_init(&url->searchParams, url->search);
}
