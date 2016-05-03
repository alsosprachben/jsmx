#ifndef __JSMN_H_
#define __JSMN_H_

#include <stddef.h>
#ifdef JSMN_EMITTER
#define JSMN_DOM
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
int jsmn_dom_add(   jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i);
int jsmn_dom_delete(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int i);
int jsmn_dom_move(  jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i);
int jsmn_dom_set(   jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int i, jsmntype_t type, int start, int end);
int jsmn_dom_new(   jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens);
#endif

#ifdef JSMN_EMITTER
/*
 * Emit as many remaining tokens as possible into the buffer from `js_cursor` to `js_boundary`.
 * Call iteratively to fill multiple or extended buffers. 
 * `token_cursor` and `js_cursor` are updated each iteration.
 * When emission is complete, token_cursor will be NULL, and JSMN_ERROR_PART is returned.
 * Returns the number of tokens written:
 *  >  0: some tokens were emitted
 *  == 0: finished emitting
 *  <  0: JSMN_* error
 * 
 * There will always be a null terminator because a token will only be emitted if there is enough room for the string including its null terminator.
 */
int jsmn_emit(jsmn_parser *parser, char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens,
		int *token_cursor, char *js_cursor, char *js_boundary);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */
