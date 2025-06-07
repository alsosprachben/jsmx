#ifndef MNURL_H
#define MNURL_H

#include "jsstr.h"

typedef struct urlparams_s {
    jsstr8_t name;
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

size_t urlsearchparams_size(urlsearchparams_t *searchParams);

/*
 * append a name-value pair to the search params
 */
void urlsearchparams_append(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value);

/*
 * delete a name from the search params
 */
void urlsearchparams_delete(urlsearchparams_t *searchParams, jsstr8_t name);

/*
 * delete a name-value pair from the search params
 */
void urlsearchparams_deletevalue(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value);

/*
 * get the first value for a name in the search params
 */
jsstr8_t urlsearchparams_get(urlsearchparams_t *searchParams, jsstr8_t name);

/*
 * get all values for a name in the search params
 * fills the values array (of length len) and sets len to the number of values found
 * returns the number of values that would be written to an unbounded array
 * if len is 0, it will not write any values and just return the number of values
 * that would be written.
 * If len is less than the number of values, it will write as many as it can
 * and set len to the number of values written.
 */
size_t urlsearchparams_getAll(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t *values, size_t *len);

/*
 * check if a name exists in the search params
 */
int urlsearchparams_has(urlsearchparams_t *searchParams, jsstr8_t name);

/*
 * set a name-value pair in the search params
 */
void urlsearchparams_set(urlsearchparams_t *searchParams, jsstr8_t name, jsstr8_t value);

/*
 * sort the search params by name
 */
void urlsearchparams_sort(urlsearchparams_t *searchParams);

/*
 * as the whatwg URL spec, but in C idioms
 */
typedef struct url_s {
    jsstr8_t href;
    jsstr8_t origin;
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