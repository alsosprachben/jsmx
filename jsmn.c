#include "jsmn.h"
#ifdef JSMN_EMITTER
#include <string.h> /* for memcpy() */
#endif
/**
 * Allocates a fresh unused token from the token pull.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
		jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
#ifdef JSMN_PARENT_LINKS
	tok->parent = -1;
#endif
#ifdef JSMN_DOM
	tok->family.parent = -1;
	tok->family.siblings.prev = -1;
	tok->family.siblings.next = -1;
	tok->family.children.first = -1;
	tok->family.children.last = -1;
#endif
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
                            int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
#ifdef JSMN_DOM
#else
	token->size = 0;
#endif
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
		size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;
	int start;

	start = parser->pos;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
#ifndef JSMN_STRICT
			/* In strict mode primitive must be followed by "," or "}" or "]" */
			case ':':
#endif
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}
#ifdef JSMN_STRICT
	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return JSMN_ERROR_PART;
#endif

found:
	if (tokens == NULL) {
		parser->pos--;
		return 0;
	}
	token = jsmn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}
	jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
	token->parent = parser->toksuper;
#endif
	parser->pos--;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
		size_t len, jsmntok_t *tokens, size_t num_tokens) {
	jsmntok_t *token;

	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			if (tokens == NULL) {
				return 0;
			}
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSMN_ERROR_NOMEM;
			}
			jsmn_fill_token(token, JSMN_STRING, start+1, parser->pos);
#ifdef JSMN_PARENT_LINKS
			token->parent = parser->toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (js[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;
					for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
									(js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
									(js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
							parser->pos = start;
							return JSMN_ERROR_INVAL;
						}
						parser->pos++;
					}
					parser->pos--;
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSMN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens) {
	int r;
	int i;
	jsmntok_t *token;
	int count = parser->toknext;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c;
		jsmntype_t type;

		c = js[parser->pos];
		switch (c) {
			case '{': case '[':
				count++;
				if (tokens == NULL) {
					break;
				}
				token = jsmn_alloc_token(parser, tokens, num_tokens);
				if (token == NULL)
					return JSMN_ERROR_NOMEM;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
#ifdef JSMN_PARENT_LINKS
					token->parent = parser->toksuper;
#endif
				}
				token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
				break;
			case '}': case ']':
				if (tokens == NULL)
					break;
				type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
				/* Open tokens have a set `start`, but an unset (-1) `end`. */
#ifdef JSMN_PARENT_LINKS
				/* Find container's explicit `parent` token. Set its `end`. Set `toksuper` to its explicit `parent`. */
				if (parser->toknext < 1) {
					return JSMN_ERROR_INVAL;
				}
				token = &tokens[parser->toknext - 1];
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSMN_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = &tokens[token->parent];
				}
#else
				/* Find container's latest open token. Set its `end`. */
				for (i = parser->toknext - 1; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return JSMN_ERROR_INVAL;
						}
						parser->toksuper = -1;
						token->end = parser->pos + 1;
						break;
					}
				}
				/* Error if unmatched closing bracket */
				if (i == -1) return JSMN_ERROR_INVAL;
				/* Find the next latest open token. Set that to the current `toksuper`. */
				for (; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						parser->toksuper = i;
						break;
					}
				}
#endif
				break;
			case '\"':
				r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
			case ':':
				parser->toksuper = parser->toknext - 1;
				break;
			case ',':
				if (tokens != NULL && parser->toksuper != -1 &&
						tokens[parser->toksuper].type != JSMN_ARRAY &&
						tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
					parser->toksuper = tokens[parser->toksuper].parent;
#else
					for (i = parser->toknext - 1; i >= 0; i--) {
						if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
							if (tokens[i].start != -1 && tokens[i].end == -1) {
								parser->toksuper = i;
								break;
							}
						}
					}
#endif
				}
				break;
#ifdef JSMN_STRICT
			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				/* And they must not be keys of the object */
				if (tokens != NULL && parser->toksuper != -1) {
					jsmntok_t *t = &tokens[parser->toksuper];
					if (t->type == JSMN_OBJECT ||
							(t->type == JSMN_STRING && t->size != 0)) {
						return JSMN_ERROR_INVAL;
					}
				}
#else
			/* In non-strict mode every unquoted value is a primitive */
			default:
#endif
				r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;

#ifdef JSMN_STRICT
			/* Unexpected char in strict mode */
			default:
				return JSMN_ERROR_INVAL;
#endif
		}
	}

	if (tokens != NULL) {
		for (i = parser->toknext - 1; i >= 0; i--) {
			/* Unmatched opened object or array */
			if (tokens[i].start != -1 && tokens[i].end == -1) {
				return JSMN_ERROR_PART;
			}
		}
	}

	return count;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
void jsmn_init(jsmn_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

#ifdef JSMN_DOM
int jsmn_dom_add(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i) {
	if (i >= num_tokens || parent_i >= num_tokens) {
		return JSMN_ERROR_INVAL;
	}

	tokens[i].family.parent = i;
	if (tokens[parent_i].family.children.first == -1) {
		tokens[parent_i].family.children.first = i;
		tokens[parent_i].family.children.last  = i;
	} else {
		tokens[i].family.siblings.prev = tokens[parent_i].family.children.last;
		tokens[tokens[parent_i].family.children.last].family.siblings.next = i;
		tokens[parent_i].family.children.last  = i;
	}

	return 0;
}
int jsmn_dom_delete(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int i) {
	int parent_i;

	if (i >= num_tokens || parent_i >= num_tokens) {
		return JSMN_ERROR_INVAL;
	}

	parent_i = tokens[i].family.parent;

	return 0;
}
int jsmn_dom_move(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int parent_i, int i) {
	int rc;

	rc = jsmn_dom_delete(parser, tokens, num_tokens, i);
	if (rc < 0) {
		return rc;
	}

	rc = jsmn_dom_add(parser, tokens, num_tokens, parent_i, i);
	if (rc < 0) {
		return rc;
	}

	return 0;
}
int jsmn_dom_set(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, int i, jsmntype_t type, int start, int end) {
	if (i >= num_tokens || parent_i >= num_tokens) {
		return JSMN_ERROR_INVAL;
	}

	jsmn_fill_token(&tokens[i], type, start, end);

	return 0;
}
int jsmn_dom_new(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens) {
	int i;
	jsmntok_t *tok;

	i = parser->toknext;

	tok = jsmn_alloc_token(parser, tokens, num_tokens);
	if (tok == NULL) {
		return JSMN_ERROR_INVAL;
	}

	return i;	
}
int jsmn_dom_new_as(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens, jsmntype_t type, start, end) {
	int rc;
	int i;

	i = jsmn_dom_new(parser, tokens, num_tokens);
	if (i < 0) {
		return i;
	}

	rc = jsmn_dom_set(parser, tokens, num_tokens, type, start, end);
	if (rc < 0) {
		return rc;
	}

	return i;
}
int jsmn_dom_new_object(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens) {
	return jsmn_dom_new(parser, tokens, num_tokens, JSMN_OJBECT, 0, 0);
}
int jsmn_dom_new_array(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens) {
	return jsmn_dom_new(parser, tokens, num_tokens, JSMN_ARRAY, 0, 0);
}
int jsmn_dom_new_primitive(jsmn_parser *parser, char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value) {
	int i;
	int rc;
	size_t size;

	size = strlen(value);

	if (parser->pos + size + 1 >= len) {
		return JSMN_ERROR_NOMEM;
	}

	i = parser->toknext;

	memcpy(&js[parser->pos], value, size + 1);

	rc = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
	if (rc < 0) {
		return rc;
	}

	return i;
}
int jsmn_dom_new_string(jsmn_parser *parser, char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, const char *value) {
	int rc;
	size_t size;

	size = strlen(value);

	if (len <  parser->pos + 1 + size + 1 + 1) {
		return JSMN_ERROR_NOMEM;
	}

	i = parser->toknext;

	        js[parser->pos] = '"';
	memcpy(&js[parser->pos + 1], value, size);
	        js[parser->pos + 1 + size] = '"';
	        js[parser->pos + 1 + size + 1] = '\0';

	parser->pos++; /* skip past opening quote, where expected by `jsmn_parse_string` */

	rc = jsmn_parse_string(parser, js, len, tokens, num_tokens);
	if (rc < 0) {
		return rc;
	}

	return i;
}
#endif

#ifdef JSMN_EMITTER

int jsmn_emit(jsmn_parser *parser, char *js, size_t len,
		jsmntok_t *tokens, unsigned int num_tokens,
		int *token_cursor, char *js_cursor, char *js_boundary) {
	return 0;
}


void jsmn_init_emitter(jsmn_emitter *emitter, jsmn_parser *parser) {
	emitter->parser = parser;
	emitter->tok = NULL;
	emitter->parenttok = NULL;
	emitter->parentitem = 0;
}

#define START_PRE                      ""
#define CONT_PRE                       ""
#define END_PRE                        ""
#define SINGLE_PRE                     ""
#define KEY_START_PRE                  "{"
#define KEY_CONT_PRE                   ""
#define KEY_END_PRE                    "" 
#define KEY_SINGLE_PRE                 "{" 
#define START_POST                     ", "
#define CONT_POST                      ", "
#define END_POST                       "}"
#define SINGLE_POST                    "}"
#define KEY_START_POST                 ": "
#define KEY_CONT_POST                  ": "
#define KEY_END_POST                   ": "
#define KEY_SINGLE_POST                ": "
#define STRING_START_PRE               "\""
#define STRING_CONT_PRE                "\""
#define STRING_END_PRE                 "\""
#define STRING_SINGLE_PRE              "\""
#define STRING_KEY_START_PRE           "{\""
#define STRING_KEY_CONT_PRE            "\""
#define STRING_KEY_END_PRE             "\""
#define STRING_KEY_SINGLE_PRE          "{\""
#define STRING_START_POST              "\", "
#define STRING_CONT_POST               "\", "
#define STRING_END_POST                "\"}"
#define STRING_SINGLE_POST             "\"}"
#define STRING_KEY_START_POST          "\": "
#define STRING_KEY_CONT_POST           "\": "
#define STRING_KEY_END_POST            "\": "
#define STRING_KEY_SINGLE_POST         "\": "
#define ARRAY_START_PRE                      "["
#define ARRAY_CONT_PRE                       ""
#define ARRAY_END_PRE                        ""
#define ARRAY_SINGLE_PRE                     "["
#define ARRAY_KEY_START_PRE                  ""
#define ARRAY_KEY_CONT_PRE                   ""
#define ARRAY_KEY_END_PRE                    "" 
#define ARRAY_KEY_SINGLE_PRE                 "" 
#define ARRAY_START_POST                     ", "
#define ARRAY_CONT_POST                      ", "
#define ARRAY_END_POST                       "]"
#define ARRAY_SINGLE_POST                    "]"
#define ARRAY_KEY_START_POST                 ""
#define ARRAY_KEY_CONT_POST                  ""
#define ARRAY_KEY_END_POST                   ""
#define ARRAY_KEY_SINGLE_POST                ""
#define ARRAY_STRING_START_PRE               "[\""
#define ARRAY_STRING_CONT_PRE                "\""
#define ARRAY_STRING_END_PRE                 "\""
#define ARRAY_STRING_SINGLE_PRE              "[\""
#define ARRAY_STRING_KEY_START_PRE           ""
#define ARRAY_STRING_KEY_CONT_PRE            ""
#define ARRAY_STRING_KEY_END_PRE             ""
#define ARRAY_STRING_KEY_SINGLE_PRE          ""
#define ARRAY_STRING_START_POST              "\", "
#define ARRAY_STRING_CONT_POST               "\", "
#define ARRAY_STRING_END_POST                "\"]"
#define ARRAY_STRING_SINGLE_POST             "\"]"
#define ARRAY_STRING_KEY_START_POST          ""
#define ARRAY_STRING_KEY_CONT_POST           ""
#define ARRAY_STRING_KEY_END_POST            ""
#define ARRAY_STRING_KEY_SINGLE_POST         ""

const char *emission_table[] = {
	START_PRE,
	CONT_PRE,
	END_PRE,
	SINGLE_PRE,
	KEY_START_PRE,
	KEY_CONT_PRE,
	KEY_END_PRE,
	KEY_SINGLE_PRE,
	START_POST,
	CONT_POST,
	END_POST,
	SINGLE_POST,
	KEY_START_POST,
	KEY_CONT_POST,
	KEY_END_POST,
	KEY_SINGLE_POST,
	STRING_START_PRE,
	STRING_CONT_PRE,
	STRING_END_PRE,
	STRING_SINGLE_PRE,
	STRING_KEY_START_PRE,
	STRING_KEY_CONT_PRE,
	STRING_KEY_END_PRE,
	STRING_KEY_SINGLE_PRE,
	STRING_START_POST,
	STRING_CONT_POST,
	STRING_END_POST,
	STRING_SINGLE_POST,
	STRING_KEY_START_POST,
	STRING_KEY_CONT_POST,
	STRING_KEY_END_POST,
	STRING_KEY_SINGLE_POST,
	ARRAY_START_PRE,
	ARRAY_CONT_PRE,
	ARRAY_END_PRE,
	ARRAY_SINGLE_PRE,
	ARRAY_KEY_START_PRE,
	ARRAY_KEY_CONT_PRE,
	ARRAY_KEY_END_PRE,
	ARRAY_KEY_SINGLE_PRE,
	ARRAY_START_POST,
	ARRAY_CONT_POST,
	ARRAY_END_POST,
	ARRAY_SINGLE_POST,
	ARRAY_KEY_START_POST,
	ARRAY_KEY_CONT_POST,
	ARRAY_KEY_END_POST,
	ARRAY_KEY_SINGLE_POST,
	ARRAY_STRING_START_PRE,
	ARRAY_STRING_CONT_PRE,
	ARRAY_STRING_END_PRE,
	ARRAY_STRING_SINGLE_PRE,
	ARRAY_STRING_KEY_START_PRE,
	ARRAY_STRING_KEY_CONT_PRE,
	ARRAY_STRING_KEY_END_PRE,
	ARRAY_STRING_KEY_SINGLE_PRE,
	ARRAY_STRING_START_POST,
	ARRAY_STRING_CONT_POST,
	ARRAY_STRING_END_POST,
	ARRAY_STRING_SINGLE_POST,
	ARRAY_STRING_KEY_START_POST,
	ARRAY_STRING_KEY_CONT_POST,
	ARRAY_STRING_KEY_END_POST,
	ARRAY_STRING_KEY_SINGLE_POST
};
#define IDX_START  0
#define IDX_CONT   1
#define IDX_END    2
#define IDX_SINGLE 3

#define IDX_VAL   0
#define IDX_KEY   4

#define IDX_PRE   0
#define IDX_POST  8

#define IDX_UNQUOTED  0
#define IDX_QUOTED    16

#define IDX_OBJECT    0
#define IDX_ARRAY     32

int jsmn_emit(jsmn_emitter *emitter, char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens, char *outjs, size_t outlen) {
	jsmntok_t *containertok;
	size_t pos = 0;

	int    tokidx;
	int    size;
	int    item;

	const char *prebuf;
	size_t prelen;

	size_t spanlen;

	const char *postbuf;
	size_t postlen;

	if (emitter->tok == NULL) {
		emitter->tok = TAILQ_FIRST(&emitter->parser->edithead);
	}

	while (emitter->tok != NULL) {
		fprintf(stderr, "parent: %i, size: %i\n", emitter->tok->parent, tokens[emitter->tok->parent].size);
		if (emitter->tok->parent >= 0) {
			switch (tokens[emitter->tok->parent].type) {
				case JSMN_OBJECT:
				case JSMN_ARRAY:
					containertok = &tokens[emitter->tok->parent];
					break;
				default:
					if (tokens[emitter->tok->parent].parent >= 0) {
						containertok = &tokens[tokens[emitter->tok->parent].parent];
					} else {
						containertok = NULL;
					}
			}

			if (emitter->parenttok == containertok) {
				++emitter->parentitem;
			} else {
				emitter->parenttok = containertok;
				emitter->parentitem = 0;
			}
		} else {
			emitter->parenttok = NULL;
		}
		if (emitter->tok->start > emitter->tok->end) {
			return JSMN_ERROR_INVAL;
		}

		tokidx = 0;

		switch (emitter->tok->type) {
			case JSMN_OBJECT:
				tokidx = -1;;
				spanlen = 0;
				break;
			case JSMN_ARRAY:
				tokidx = -1;
				spanlen = 0;
				break;
			case JSMN_PRIMITIVE:
				tokidx += IDX_UNQUOTED;
				spanlen = emitter->tok->end - emitter->tok->start;
				break;
			case JSMN_STRING:
				tokidx += IDX_QUOTED;
				spanlen = emitter->tok->end - emitter->tok->start;
				break;
			case JSMN_UNDEFINED:
				tokidx = -1;;
				break;
		}

		if (tokidx == -1) {
			emitter->tok = TAILQ_NEXT(emitter->tok, editlinks);
			fprintf(stderr, "skipping container\n");
			continue;
		}

		if (emitter->parenttok != NULL) {
			switch (emitter->parenttok->type) {
				case JSMN_OBJECT:
					tokidx += IDX_OBJECT;
					if (emitter->parenttok != NULL) {
						size = emitter->parenttok->size;
					} else {
						size = 0;
					}
					if (emitter->parentitem % 2 == 0) {
						tokidx += IDX_KEY;
					} else {
						tokidx += IDX_VAL;
					}
					item = emitter->parentitem / 2;
					break;
				case JSMN_ARRAY:
					tokidx += IDX_ARRAY;
					if (emitter->parenttok != NULL) {
						size = emitter->parenttok->size;
					} else {
						size = 0;
					}
					tokidx += IDX_VAL;
					item = emitter->parentitem;
					break;
				case JSMN_PRIMITIVE:
					size = 0;
					item = 0;
					break;
				case JSMN_STRING:
					size = 0;
					item = 0;
					break;
				case JSMN_UNDEFINED:
					break;
			}
		} else {
			size = 0;
			item = 0;
		}

		if (size == 1) {
			tokidx += IDX_SINGLE;
		} else if (size > 1) {
			if (item == 0) {
				tokidx += IDX_START;
			} else if (item >= size - 1) {
				tokidx += IDX_END;
			} else {
				tokidx += IDX_CONT;
			}
		}

		prebuf  = emission_table[tokidx + IDX_PRE];
		postbuf = emission_table[tokidx + IDX_POST];
		prelen  = strlen(prebuf);
		postlen = strlen(postbuf);

		if (outlen <  pos +         prelen +                           spanlen +          postlen) {
			break;
		}
		memcpy(&outjs[pos], prebuf, prelen);
		memcpy(&outjs[pos +         prelen], &js[emitter->tok->start], spanlen);
		memcpy(&outjs[pos +         prelen +                           spanlen], postbuf, postlen);
		        outjs[pos +         prelen +                           spanlen +          postlen] = '\0';
		              pos +=        prelen +                           spanlen +          postlen;
	
		fprintf(stderr, "{preidx: %i, postidx: %i, start %i, len: %zu, size: %i, item: %i, parent: %p}\n", tokidx + IDX_PRE, tokidx + IDX_POST, emitter->tok->start, spanlen, size, item, emitter->parenttok);
		emitter->tok = TAILQ_NEXT(emitter->tok, editlinks);
	}

	return pos;
}

int jsmn_emit_pending(jsmn_emitter *emitter) {
	return emitter->tok != NULL;
}

void jsmn_remove_token(jsmn_parser *parser, jsmntok_t *tok) {
	TAILQ_REMOVE(&parser->edithead, tok, editlinks);
}
#endif
