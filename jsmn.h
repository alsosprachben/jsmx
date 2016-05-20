#ifndef __JSMN_H_
#define __JSMN_H_

#include <stddef.h>
#ifdef JSMN_EMITTER
#define JSMN_DOM
#define JSMN_STRICT
#endif
#ifdef JSMN_DOM
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JSMN_UNDEFINED = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3,
	JSMN_PRIMITIVE = 4
} jsmntype_t;

enum jsmnerr {
	/* Not enough tokens were provided */
	JSMN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSMN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSMN_ERROR_PART = -3
};

#ifdef JSMN_DOM
struct children_t {
	int first;
	int last;
};
struct siblings_t {
	int prev;
	int next;
};
struct family_t {
	int parent;
	struct siblings_t siblings;
	struct children_t children;
};
#endif


/**
 * JSON token description.
 * @param		type	type (object, array, string etc.)
 * @param		start	start position in JSON data string
 * @param		end		end position in JSON data string
 */
typedef struct jsmntok_s {
	jsmntype_t type;
	int start;
	int end;
#ifdef JSMN_DOM
	struct family_t family;
#else
	int size;
#ifdef JSMN_PARENT_LINKS
	int parent;
#endif
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	unsigned int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens);

#ifdef JSMN_DOM
int jsmn_dom_get_utf8(       jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens,               int i, char    *val8,  size_t val8_len);
int jsmn_dom_get_utf32(      jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens,               int i, wchar_t *val32, size_t val32_len);
int jsmn_dom_get_value(      jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens,               int i, char    *buf,   size_t buflen);
int    jsmn_dom_get_start(   jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
size_t jsmn_dom_get_strlen(  jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
jsmntype_t jsmn_dom_get_type(jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);

int jsmn_dom_get_parent(     jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
int jsmn_dom_get_child(      jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
int jsmn_dom_get_sibling(    jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
int jsmn_dom_is_open(        jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
int jsmn_dom_get_open(       jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);

int jsmn_dom_add(            jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i);
int jsmn_dom_delete(         jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i);
int jsmn_dom_move(           jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i);

int jsmn_dom_set(            jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i, jsmntype_t type, int start, int end);
int jsmn_dom_close(          jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,               int i,                             int end);

int jsmn_dom_new(            jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens);
int jsmn_dom_new_as(         jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens,                      jsmntype_t type, int start, int end);
int jsmn_dom_new_object(     jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens);
int jsmn_dom_new_array(      jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens);
int jsmn_dom_new_primitive(  jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value);
int jsmn_dom_new_integer(    jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, int         value);
int jsmn_dom_new_double(     jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, double      value);
int jsmn_dom_new_string(     jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value);
int jsmn_dom_new_utf8(       jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value, size_t value_len);
#define TRUE_EXPR  "true "
#define FALSE_EXPR "false "
#define NULL_EXPR  "null "
int jsmn_dom_eval(           jsmn_parser *parser,       char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value);

int jsmn_dom_insert_name(    jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int object_i, int name_i, int value_i);
int jsmn_dom_insert_value(   jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int array_i,              int value_i);
int jsmn_dom_delete_name(    jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int object_i, int name_i);
int jsmn_dom_delete_value(   jsmn_parser *parser,                             jsmntok_t *tokens, unsigned int num_tokens, int array_i,              int value_i);
#endif

#ifdef JSMN_EMITTER
enum tokphase {
	PHASE_UNOPENED = 0,
	PHASE_OPENED,
	PHASE_UNCLOSED,
	PHASE_CLOSED
};
typedef struct {
	int           cursor_i;
	enum tokphase cursor_phase;
} jsmn_emitter;

void jsmn_init_emitter(jsmn_emitter *emitter);

/*
 * Emit as many remaining tokens as possible into the buffer `outjs` of length `outlen`.
 * There will always be a null terminator because a token will only be emitted if there is enough room for the string including its null terminator.
 * Call iteratively to fill multiple or extended buffers. 
 *
 * Returns the number of bytes written into `outjs` when >= 0:
 * Returns JSMN_ERROR_* when < 0;
 *
 * When emission is complete: `emitter->cursor_i == -1`.
 * Emission starts at emitter state, which defaults to `{0, PHASE_UNOPENED}`.
 */
int jsmn_emit(jsmn_parser *parser, char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens,
		jsmn_emitter *emitter, char *outjs, size_t outlen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */
