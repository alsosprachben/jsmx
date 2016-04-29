#ifndef __JSMN_H_
#define __JSMN_H_

#include <stddef.h>
#ifdef JSMN_EMITTER
#include <sys/queue.h> /* take from BSD if missing -- do not reinvent */
#define JSMN_PARENT_LINKS
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
	int size;
#ifdef JSMN_PARENT_LINKS
	int parent;
#endif
#ifdef JSMN_EMITTER
	TAILQ_ENTRY(jsmntok_s) editlinks; /* for out-of-line edits appended to the end of the token array */
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
#ifdef JSMN_EMITTER
	TAILQ_HEAD(edithead, jsmntok_s);
#endif
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

#ifdef JSMN_EMITTER
/**
 * JSON emitter.
 */
typedef struct {
	jsmn_parser *parser;
	jsmntok_t   *tok;
	jsmntok_t   *parenttok;
	unsigned int parentitem;
} json_emitter;

void jsmn_init_emitter(jsmn_emitter *emitter, jsmn_parser *parser);

/**
 * Emit as many remaining tokens as possible into the buffer. Call this iteratively to fill multiple buffers. Returns the number bytes written excluding the null terminator, the string length. There will always be a null terminator because a token will only be emitted if there is enough room for the string including its null terminator.
 */
int jsmn_emit(jsmn_emitter *emitter, char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, char *outjs, size_t outlen);

int jsmn_emit_pending(jsmn_emitter *emitter);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_H_ */
