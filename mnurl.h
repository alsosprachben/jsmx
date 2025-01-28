#ifndef MNURL_H
#define MNURL_H

#include "jsstr.h"

typedef struct urlparams_s {
    jsstr8_t key;
    jsstr8_t value;
} urlparams_t;

/*
 * as the whatwg URLSearchParams spec, but in C idioms
 */
typedef struct urlsearchparams_s {
    size_t cap;
    size_t len;
    urlparams_t params[32];
} urlsearchparams_t;

void urlsearchparams_init(urlsearchparams_t *searchParams, jsstr8_t search_str);

void urlsearchparams_append(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value);
void urlsearchparams_delete(urlsearchparams_t *searchParams, jsstr8_t key);
void urlsearchparams_deletevalue(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value);
jsstr8_t urlsearchparams_get(urlsearchparams_t *searchParams, jsstr8_t key);
void urlsearchparams_getAll(urlsearchparams_t *searchParams, jsstr8_t key, urlparams_t *params);
int urlsearchparams_has(urlsearchparams_t *searchParams, jsstr8_t key);
void urlsearchparams_set(urlsearchparams_t *searchParams, jsstr8_t key, jsstr8_t value);
urlsearchparams_sort(urlsearchparams_t *searchParams);

/*
 * as the whatwg URL spec, but in C idioms
 */
typedef struct url_s {
    jsstr8_t href;
    jsstr8_t protocol;
    jsstr8_t username;
    jsstr8_t password;
    jsstr8_t host;
    jsstr8_t hostname;
    jsstr8_t port;
    jsstr8_t pathname;
    jsstr8_t search;
    urlsearchparams_t searchParams;
    jsstr8_t hash;
} url_t;

void url_init(url_t *url, jsstr8_t href);

void url_toJSON(url_t *url, jsstr8_t *json_str);

#endif