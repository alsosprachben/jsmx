#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#include "mnurl.h"

void urlsearchparams_init(urlsearchparams_t *searchParams, jsstr8_t search) {
    searchParams->cap = 32;
    searchParams->len = 0;

    /* parse search to fill in the searchParams */
    /* use jsstr8_index{of,token}() to find the next delimiter */
    /* use jsstr8_slice() to assign the fields */
    /* use urlsearchparams_append() to append the fields */

    size_t param_i = 0;
    ssize_t amp_i = 0;
    ssize_t eq_i = 0;
    jsstr8_t param;
    jsstr8_t name;
    jsstr8_t value;
    static uint32_t *param_term = L"&;";
    size_t search_len = jsstr8_get_utf32len(&search);
    
    for (
        param_i = 0, amp_i = 0, eq_i = 0; /* init positions */
        param_i < search_len && amp_i >= 0; /* until we don't find an arg terminator */
        param_i = amp_i + 1 /* skip to the next arg */
    ) {
        /* init param */
        amp_i = jsstr8_u32_indextoken(&search, param_term, 2, param_i);
        if (amp_i < 0) {
            amp_i = -1; /* to end */
        }
        jsstr8_slice(&param, &search, param_i, amp_i);

        /* init name */
        eq_i = jsstr8_u32_indexof(&param, '=', 0);
        if (eq_i < 0) {
            eq_i = -1; /* to end */
        }
        jsstr8_slice(&name, &param, 0, eq_i);

        /* init value */
        if (eq_i >= 0) {
            jsstr8_slice(&value, &param, eq_i + 1, -1);
        } else {
            value = jsstr8_empty;
        }

        /* append param */
        urlsearchparams_append(searchParams, name, value);
    }
}

size_t urlsearchparams_size(urlsearchparams_t *searchParams) {
    return searchParams->len;
}

void urlsearchparams_append(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value) {
    if (searchParams->len < searchParams->cap) {
        searchParams->params[searchParams->len].name = name;
        searchParams->params[searchParams->len].value = value;
        searchParams->len++;
    }
}

void urlsearchparams_delete(urlsearchparams_t *searchParams, jsstr8_t name) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0) {
            for (size_t j = i; j < searchParams->len - 1; j++) {
                searchParams->params[j] = searchParams->params[j + 1];
            }
            searchParams->len--;
            break;
        }
    }
}

void urlsearchparams_deletevalue(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0 && jsstr8_cmp(&searchParams->params[i].value, &value) == 0) {
            for (size_t j = i; j < searchParams->len - 1; j++) {
                searchParams->params[j] = searchParams->params[j + 1];
            }
            searchParams->len--;
            break;
        }
    }
}

jsstr8_t urlsearchparams_get(urlsearchparams_t *searchParams, jsstr8_t name) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0) {
            return searchParams->params[i].value;
        }
    }
    return (jsstr8_t) {0};
}


size_t urlsearchparams_getAll(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t *values, size_t *len_ptr) {
    size_t in_len = *len_ptr;
    size_t out_len = 0;
    size_t found_len = 0;
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0) {
            if (found_len < in_len) {
                /* write the value to the output array, if it fits */
                values[out_len] = searchParams->params[i].value;
                out_len++;
            }
            found_len++;
        }
    }
    *len_ptr = out_len;
    return found_len; /* return the number of values found */
}

int urlsearchparams_has(urlsearchparams_t *searchParams, jsstr8_t name) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0) {
            return 1;
        }
    }
    return 0;
}

void urlsearchparams_set(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value) {
    for (size_t i = 0; i < searchParams->len; i++) {
        if (jsstr8_cmp(&searchParams->params[i].name, &name) == 0) {
            searchParams->params[i].value = value;
            return;
        }
    }
    urlsearchparams_append(searchParams, name, value);
}

void urlsearchparams_sort(urlsearchparams_t *searchParams) {
    for (size_t i = 0; i < searchParams->len; i++) {
        for (size_t j = i + 1; j < searchParams->len; j++) {
            if (jsstr8_cmp(&searchParams->params[i].name, &searchParams->params[j].name) > 0) {
                urlparams_t tmp = searchParams->params[i];
                searchParams->params[i] = searchParams->params[j];
                searchParams->params[j] = tmp;
            }
        }
    }
}

int urlsearchparams_toString(urlsearchparams_t *searchParams, jsstr8_t *result_ptr) {
    int rc;
    declare_jsstr8(amp, "&");
    declare_jsstr8(equals, "=");
    for (size_t i = 0; i < searchParams->len; i++) {
        if (i > 0) {
            rc = jsstr8_concat(result_ptr, &amp);
            if (rc == -1) {
                return -1; /* error in concatenation */
            }
        }
        jsstr8_concat(result_ptr, &searchParams->params[i].name);
        if (searchParams->params[i].value.len > 0) {
            rc = jsstr8_concat(result_ptr, &equals);
            if (rc == -1) {
                return -1; /* error in concatenation */
            }
            rc = jsstr8_concat(result_ptr, &searchParams->params[i].value);
            if (rc == -1) {
                return -1; /* error in concatenation */
            }
        }
    }
    return 0;
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
    /* parse href to point into the other fields, using jsstr8_u32_indexof() to find the next delimiter */
    /* use jsstr8_slice() to assign the fields */
    /* use urlsearchparams_init() to initialize searchParams */

    /*
     * Origin
     */

    protocol_i = jsstr8_u32_indexof(&href, ':', 0);
    if (protocol_i < 0) {
        return;
    }
    
    jsstr8_slice(&url->protocol, &href, 0, protocol_i);

    /* verify the two authority slashes */
    authority_b1 = jsstr8_u8s_at(&href, protocol_i + 1);
    authority_b2 = jsstr8_u8s_at(&href, protocol_i + 2);
    if (authority_b1[0] != '/' && authority_b2[0] != '/') {
        return;
    }
    authority_i = protocol_i + 3;

    at_i = jsstr8_u32_indexof(&href, '@', authority_i);
    if (at_i >= 0) {
        colon_i = jsstr8_u32_indexof(&href, ':', authority_i);
        if (colon_i >= 0) {
            jsstr8_slice(&url->username, &href, authority_i, colon_i);
            jsstr8_slice(&url->password, &href, colon_i + 1, at_i);
        } else {
            jsstr8_slice(&url->username, &href, authority_i, at_i);
        }
        host_i = at_i + 1;
    } else {
        host_i = authority_i;
    }

    port_i = jsstr8_u32_indexof(&href, ':', host_i);
    path_i = jsstr8_u32_indexof(&href, '/', host_i);
    if (port_i > path_i) {
        port_i = -1;
    }
    if (path_i >= 0) {
        if (port_i >= 0) {
            jsstr8_slice(&url->host, &href, host_i, port_i);
            jsstr8_slice(&url->port, &href, port_i + 1, path_i);
        } else {
            jsstr8_slice(&url->host, &href, host_i, path_i);
        }
    } else {
        path_i = -path_i; /* end of search */
        jsstr8_slice(&url->host, &href, host_i, path_i);
        return;
    }

    jsstr8_slice(&url->origin, &href, 0, path_i);

    /* Path */

    search_i = jsstr8_u32_indexof(&href, '?', path_i + 1);
    if (search_i != -1) {
        jsstr8_slice(&url->pathname, &href, path_i, search_i);
        hash_i = jsstr8_u32_indexof(&href, '#', search_i + 1);
        if (hash_i >= 0) {
            jsstr8_slice(&url->search, &href, search_i + 1, hash_i);
            jsstr8_slice(&url->hash, &href, hash_i + 1, -1);
        } else {
            jsstr8_slice(&url->search, &href, search_i + 1, -1);
        }
    } else {
        hash_i = jsstr8_u32_indexof(&href, '#', path_i + 1);
        if (hash_i >= 0) {
            jsstr8_slice(&url->pathname, &href, path_i, hash_i);
            jsstr8_slice(&url->hash, &href, hash_i + 1, -1);
        } else {
            jsstr8_slice(&url->pathname, &href, path_i, -1);
        }
    }
    urlsearchparams_init(&url->searchParams, url->search);
}

jsstr8_t url_toJSON(url_t *url) {
    return url->href;
}
