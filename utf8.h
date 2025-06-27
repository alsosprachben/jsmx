#ifndef UTF8_H
#define UTF8_H
#include <stddef.h>
#include <stdint.h>

/*
 * UTF-8 functions
 */

/*
 * `bc` is byte cursor
 * `bs` is byte stop
 * `cc` is character cursor
 * `cs` is character stop
 * `qc` is JSON-quoted character
 * `qs` is JSON-quoted character stop
 * `c`  is character (`*cc`)
 * `l`  is character byte length
 * `i'  is index into `bc` for peeks
 * `n`  is the number of valid bytes
 */

/*
 * UTF-8 Byte Shift
 * Given a sequence byte index `i`, and a sequence byte length `l', return the bit shift amount for the bits in this byte.
 */
static inline int UTF8_NSHIFT(int i, int l) {
	return 6 * (l - i - 1);
}

/*
 * UTF-8 Character Byte Length from Byte 1
 * Measures the number of contiguous high set bits.
 *   l == 0: ASCII
 *   l == 1: continuation byte
 *   l >= 2: UTF-8 sequence length
 *   l >  6: malformed byte
 */
static inline void UTF8_BLEN(const uint8_t *bc, int *l) {
	*l = 0;
	while (*l < 6 && (bc[0] & (0x80 >> *l)) != 0) {
		(*l)++;
	}
}

/*
 * UTF-8 Decode Byte 1
 * Set bits from the first byte into the character.
 * Increment `n` on success.
 */
static inline void UTF8_B1(const uint8_t *bc, uint32_t *c, int *l, int *n) {
	if (*l == 0) {  /* ASCII */
		*c = bc[0];
		*l = 1;
		(*n)++;
	} else if (*l >= 2 && *l <= 6) { /* sequence start */
		*c = (bc[0] & ((1 << (7 - *l)) - 1)) << UTF8_NSHIFT(0, *l);
		(*n)++;
	} else if (*l == 1) { /* unexpected sequence continuation */
		*l = 1;
	} else if (*l > 6) { /* malformed */
		*l = 1;
	} else { /* insane negative value */
		*l = 1;
	}
}

/*
 * UTF-8 Decode Continuation Byte (2+)
 * Set bits from the continuation byte into the character.
 * Increment `n` on success.
 */
static inline void UTF8_BN(const uint8_t *bc, int i, uint32_t *c, int l, int *n) {
	if (*n > 0 && (bc[i] & 0xc0) == 0x80) {
		*c |= (bc[i] & 0x3f) << UTF8_NSHIFT(i, l);
		(*n)++;
	}
}

/*
 * UTF-8 Character Validator
 * Flip the sign of `l` if the character is invalid.
 */
static inline void UTF8_CANONICAL(uint32_t c, int *l) {
	if ((*l) == 1 && (c) < 0x80) { /* ASCII */ 
	} else if (
			((*l) < 1 || (*l) > 4)                                 /* sequence length range */ 
		||	((c) >= 0x110000 || ((c) >= 0xD800 && (c) < 0xE000)) /* code point range */ 
		||	((*l) == 1 && (                 (c) >= 0x80))         /* over long */ 
		||	((*l) == 2 && ((c) < 0x80    || (c) >= 0x800))        /* over long */ 
		||	((*l) == 3 && ((c) < 0x800   || (c) >= 0x10000))      /* over long */ 
		||	((*l) == 4 && ((c) < 0x10000 || (c) >= 0x200000))     /* over long */ 
	) { 
		(*l) = - (*l); 
	} 
}

/*
 * UTF-8 Decode Character
 * Set the character bits from the byte cursor.
 * Set `l`:
 *   l >  0:  `l` length of a valid UTF-8 sequence.
 *   l == 0:  `l` is too long for `bl`.
 *   l <  0: `-l` length of an invalid UTF-8 sequence.
 */
static inline void UTF8_CHAR(const uint8_t *bc, const uint8_t *bs, uint32_t *c, int *l) {
	int __utf8_seqlen;
	int __utf8_n = 0;
	UTF8_BLEN(bc, &__utf8_seqlen);
	UTF8_B1(bc, c, &__utf8_seqlen, &__utf8_n);
	if (__utf8_seqlen == 1) {
		if (__utf8_n == 1) { /* ASCII */
			*l = __utf8_seqlen;
		} else { /* invalid start byte */
			*l = -1;
		}
	} else if (bc + __utf8_seqlen <= bs) {
		switch (__utf8_seqlen) {
			case 6:
				UTF8_BN(bc, 5, c, __utf8_seqlen, &__utf8_n);
			case 5:
				UTF8_BN(bc, 4, c, __utf8_seqlen, &__utf8_n);
			case 4:
				UTF8_BN(bc, 3, c, __utf8_seqlen, &__utf8_n);
			case 3:
				UTF8_BN(bc, 2, c, __utf8_seqlen, &__utf8_n);
			case 2:
				UTF8_BN(bc, 1, c, __utf8_seqlen, &__utf8_n);
				break;
		}
		*l = __utf8_n;
		if (__utf8_n < __utf8_seqlen) {
			*l = -(*l); /* error the invalid byte sequence */
		} else {
			UTF8_CANONICAL(*c, l); /* error any invalid characters */
		}
	} else {
		*l = 0; /* `bs - bc` not long enough yet */
	}
}

static inline int UTF8_WELL_FORMED(const uint8_t *bc, const uint8_t *bs) {
	while (bc < bs) {
		int l;
		uint32_t c;
		UTF8_CHAR(bc, bs, &c, &l);
		if (l > 0) {  // valid sequence
			bc += l;
		} else if (l == 0) {
			break;  // incomplete sequence at bs
		} else {  // malformed
			return 0;
		}
	}
	return 1;  // well-formed
}

/*
 * UTF-8 Make Well-Formed
 * Copy from `bc` to `out_bc`, replacing malformed sequences with U+FFFD.
 * Return the number of bytes written to `out_bc`.
 */
static inline size_t UTF8_TO_WELL_FORMED(const uint8_t *bc, const uint8_t *bs, uint8_t *out_bc, size_t out_cap) {
	size_t out_i = 0;
  	while (bc < bs && out_i < out_cap) {
		int l;
		uint32_t c;
		UTF8_CHAR(bc, bs, &c, &l);
		if (l > 0) {  // valid sequence
			for (int i = 0; i < l && out_i < out_cap; i++) {
				out_bc[out_i++] = bc[i];
			}
			bc += l;
		} else if (l == 0) {
			break;  // incomplete sequence at bs
		} else {  // malformed
			// replace with U+FFFD
			if (out_i + 3 <= out_cap) {
				out_bc[out_i++] = 0xEF;
				out_bc[out_i++] = 0xBF;
				out_bc[out_i++] = 0xBD;
			} else {
				break;  // not enough space for replacement character
			}
			bc += -l;  // skip the invalid byte sequence
		}
	}
	return out_i;  // return the number of bytes written to out_bc
}

/*
 * UTF-8 Decode String
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
static inline void UTF8_DECODE(const uint8_t **bc_ptr, const uint8_t *bs, uint32_t **cc_ptr, const uint32_t *cs) {
	const uint8_t *bc = *bc_ptr;
	uint32_t *cc = *cc_ptr;

	int __utf8_seqlen2;
	while (bc < bs && cc < cs) {
		UTF8_CHAR(bc, bs, cc, &__utf8_seqlen2);
		if (__utf8_seqlen2 > 0) { /* valid character of ASCII or UTF-8 */
			bc += __utf8_seqlen2;
		} else if (__utf8_seqlen2 == 0) {
			break; /* blocking on byte length */
		} else {
			*cc = 0xFFFD; /* represent invalid sequence with the replacement character */
			bc += -__utf8_seqlen2;
		}
		cc++;
	}

	*bc_ptr = bc;
	*cc_ptr = cc;
	return;
}

static uint8_t UTF8_REPLACEMENT_CHAR[] = {0xEF, 0xBF, 0xBD}; /* U+FFFD Replacement Character in UTF-8 */

static inline int UNICODE_BOUNDARY() {
	return 0x110000; /* Maximum valid Unicode code point */
}

/*
 * UTF-8 Character Byte Length
 * Set the byte length from the character,
 */
static inline int UTF8_CLEN(uint32_t c) {
    if (c < 0x80)        return 1;
    if (c < 0x800)       return 2;
    if (c < 0x10000)     return 3;
    if (c < 0x110000)    return 4;  /* Unicode max */
    /* invalid → U+FFFD */
    return 3;
}

/*
 * UTF-8 Encode Character Byte 1
 * Sets bits from the character into the first byte, and set the sequence start high-bits.
 */
static inline void UTF8_C1(uint32_t c, uint8_t *bc, int l) {
	bc[0] = ((0xFF << (8 - l)) & 0xFF) | ((c >> UTF8_NSHIFT(0, l)) & ((1 << (7 - l)) - 1));
}

/*
 * UTF-8 Encode Character Continuation Byte (2+)
 * Sets bits from the character into the continuation byte as index `i`, and set the continuation high-bits.
 */
static inline void UTF8_CN(uint32_t c, uint8_t *bc, int i, int l) {
	bc[i] = 0x80 | ((c >> UTF8_NSHIFT(i, l)) & 0x3f);
}

/*
 * UTF8-8 Encode String
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
static inline void UTF8_ENCODE(const uint32_t **cc_ptr, const uint32_t *cs, uint8_t **bc_ptr, const uint8_t *bs) {
	const uint32_t *cc = *cc_ptr;
	uint8_t *bc = *bc_ptr;
	int replace = 0; /* whether to replace invalid characters with U+FFFD */

	int __utf8_seqlen;
	uint32_t c;
	while (cc < cs && bc < bs) {
		replace = *cc >= UNICODE_BOUNDARY() || (*cc >= 0xD800 && *cc < 0xE000);

		__utf8_seqlen = UTF8_CLEN(*cc);
		if (__utf8_seqlen == 1) { /* ASCII */
			*(bc++) = *(cc++);
		} else if (__utf8_seqlen > 1) {
			if (bc + __utf8_seqlen <= bs) { /* character fits */
				if (replace) {
					c = 0xFFFD; /* replacement character */
				} else {
					c = *(cc++);
				}
				UTF8_C1(c, bc, __utf8_seqlen);
				switch (__utf8_seqlen) {
					/*
					case 6:
						UTF8_CN(c, bc, 5, __utf8_seqlen);
					case 5:
						UTF8_CN(c, bc, 4, __utf8_seqlen);
						*/
					case 4:
						UTF8_CN(c, bc, 3, __utf8_seqlen);
					case 3:
						UTF8_CN(c, bc, 2, __utf8_seqlen);
					case 2:
						UTF8_CN(c, bc, 1, __utf8_seqlen);
						break;
				}
				bc += __utf8_seqlen;
			} else {
				break; /* blocking on byte length */
			}
		} else {
			cc++;
			/* XXX: silently skip insane character */
		}
	}

	*cc_ptr = cc;
	*bc_ptr = bc;
	return;
}

/*
 * UTF-16 functions
 */

/*
 * UTF-16 Length of Code Point in Code Units
 */
static inline int UTF16_BLEN(const uint16_t *bc, const uint16_t *bs) {
	if (bc < bs) {
		if (*bc >= 0xD800 && *bc < 0xDC00) { /* high surrogate */
			if (bc + 1 < bs && *(bc + 1) >= 0xDC00 && *(bc + 1) < 0xE000) { /* low surrogate */
				return 2; /* surrogate pair */
			} else {
				return 1; /* invalid sequence */
			}
		} else {
			return 1; /* BMP */
		}
	} else {
		return 0; /* no more bytes */
	}
}

/*
 * Whether the UTF-16 Code Point sequence is a valid Code Unit sequence.
 */
static inline int UTF16_VALID(const uint16_t *bc, const uint16_t *bs) {
	if (bc < bs) {
		if (*bc >= 0xD800 && *bc < 0xDC00) { /* high surrogate */
			if (bc + 1 < bs) {
				if (*(bc + 1) >= 0xDC00 && *(bc + 1) < 0xE000) {
					return 1; /* valid surrogate pair */
				}
				return 0; /* invalid low-surrogate */
			}
			return -1;    /* need one more code unit */
		} else if (*bc < 0xD800 || *bc >= 0xE000) { /* BMP */
			return 1; /* valid BMP character */
		}
		return 0; /* unexpected low-surrogate */
	}
	return -1; /* no more bytes */
}

/*
 * Whether the UTF-16 Code Unit sequence is well-formed.
 */
static int UTF16_WELL_FORMED(const uint16_t *bc, const uint16_t *bs) {
	while (bc < bs) {
		int r = UTF16_VALID(bc, bs);
		if (r < 0)   return 0;  // incomplete sequence at bs
		if (r == 0)  return 0;  // malformed
		// r == 1: advance by 2 if we saw a surrogate pair, otherwise by 1
		if (*bc >= 0xD800 && *bc < 0xDC00) {
			bc += 2;
		} else {
			bc += 1;
		}
	}
	return 1;
}

static void UTF16_TO_WELL_FORMED(uint16_t *bc, uint16_t *bs) {
  /* like UTF16_WELL_FORMED, but each code unit that is malformed gets replaced with the replacement character */
	while (bc < bs) {
		int r = UTF16_VALID(bc, bs);
		if (r <= 0) {  // malformed
			*bc++ = 0xFFFD; /* replacement character */
			continue;
		}
		// r == 1: advance by 2 if we saw a surrogate pair, otherwise by 1
		if (*bc >= 0xD800 && *bc < 0xDC00) {
			bc += 2;
		} else {
			bc += 1;
		}
	}
}

static int UTF32_WELL_FORMED(const uint32_t *cc, const uint32_t *cs) {
  while (cc < cs) {
	uint32_t c = *cc++;
	if (c >= 0x110000 || (c >= 0xD800 && c < 0xE000)) {
		return 0;  // malformed
	}
  }
  return 1;
}

static void UTF32_TO_WELL_FORMED(uint32_t *cc, uint32_t *cs) {
  /* like UTF32_WELL_FORMED, but each code unit that is malformed gets replaced with the replacement character */
	while (cc < cs) {
		uint32_t c = *cc++;
		if (c >= 0x110000 || (c >= 0xD800 && c < 0xE000)) {
			*cc = 0xFFFD; /* replacement character */
		}
		cc++;
	}
}

/*
 * uint32_t to uint16_t UTF-16 character conversion
 */
static inline void UTF16_CHAR(const uint16_t *bc, const uint16_t *bs, uint32_t *c, int *l) {
	if (bc < bs) {
		if (*bc >= 0xD800 && *bc < 0xDC00) { /* high surrogate */
			if (bc + 1 < bs && *(bc + 1) >= 0xDC00 && *(bc + 1) < 0xE000) { /* low surrogate */
				*c = 0x10000 + (((*bc - 0xD800) << 10) | (*(bc + 1) - 0xDC00));
				*l = 2; /* surrogate pair */
				return;
			} else {
				*c = 0xFFFD; /* replacement character for invalid sequence */
				*l = -1; /* invalid sequence */
				return;
			}
		} else { /* BMP */
			*c = *bc;
			*l = 1;
			return;
		}
	} else {
		*c = 0; /* no more bytes */
		*l = 0;
		return;
	}
}

/*
 * uint32_t to uint16_t UTF-16 character length
 */
static inline int UTF16_CLEN(uint32_t c) {
	if (c < 0) {
		return 0;
	} else if (c < 0x10000) {
		return 1; /* BMP */
	} else if (c < 0x110000) {
		return 2; /* Supplementary Planes */
	} else {
		return -1; /* invalid code point */
	}
}

/*
 * uint32_t to uint16_t UTF-16 code point conversion (surrogate pair, where UTF16_CLEN(c) == 2)
 * Converts a code point in the Supplementary Planes to a surrogate pair.
 */
static inline void UTF16_CODEPAIR(uint32_t c, uint16_t *bc) {
	c -= 0x10000;
	bc[0] = 0xD800 + ((c >> 10) & 0x3FF); /* high surrogate */
	bc[1] = 0xDC00 + ( c        & 0x3FF); /* low surrogate */
}

/*
 * UTF-16 Encode String
 * The cursors will be updated as UTF-16 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
static inline void UTF16_ENCODE(const uint32_t **cc_ptr, const uint32_t *cs, uint16_t **bc_ptr, const uint16_t *bs) {
	const uint32_t *cc = *cc_ptr;
	uint16_t *bc = *bc_ptr;

	int __utf16_seqlen;
	while (cc < cs && bc < bs) {
		__utf16_seqlen = UTF16_CLEN(*cc);
		switch (__utf16_seqlen) {
			case 0: /* invalid code point */
				*(bc++) = 0xFFFD; /* replacement character */
				cc++;
				continue;
			case 1: /* BMP */
				*(bc++) = *(cc++);
				break;
			case 2: /* Supplementary Planes */
				if (bc + 2 <= bs) { /* character fits */
					UTF16_CODEPAIR(*(cc++), bc);
					bc += 2;
				} else {
					break; /* blocking on byte length */
				}
				break;
			default:
				cc++;
				/* XXX: silently skip insane character */
				continue;
		}
	}

	*cc_ptr = cc;
	*bc_ptr = bc;
	return;
}

/*
 * UTF-16 Decode String
 * The cursors will be updated as UTF-16 is parsed and characters are emitted, until:
 * 1. a cursor reaches a stop address.
 * 2. a complete sequence would run past the byte stop address.
 * 3. a blocking condition is encountered, which will stop the decoding.
 */
static inline void UTF16_DECODE(const uint16_t **bc_ptr, const uint16_t *bs, uint32_t **cc_ptr, const uint32_t *cs) {
	const uint16_t *bc = *bc_ptr;
	uint32_t *cc = *cc_ptr;

	int __utf16_seqlen2;
	uint32_t c;
	while (bc < bs && cc < cs) {
		__utf16_seqlen2 = UTF16_BLEN(bc, bs);
		if (__utf16_seqlen2 > 0) { /* valid character of BMP or Supplementary Planes */
			UTF16_CHAR(bc, bs, &c, &__utf16_seqlen2);
			if (__utf16_seqlen2 > 0) {
				bc += __utf16_seqlen2;
			} else {
				c = 0xFFFD; /* represent invalid sequence with the replacement character */
				bc += -__utf16_seqlen2;
			}
			*(cc++) = c;
		} else if (__utf16_seqlen2 == 0) {
			break; /* blocking on byte length */
		} else {
			bc += -__utf16_seqlen2; /* invalid sequence */
			*(cc++) = 0xFFFD; /* replacement character */
		}
	}

	*bc_ptr = bc;
	*cc_ptr = cc;
	return;
}

/*
 * JSON functions
 */

/* branchless int to hex-char */
static inline uint32_t VALHEX(uint8_t v) {
	return (uint32_t)((((v) + 48) & (-((((v) - 10) & 0x80) >> 7))) | (((v) + 55) & (-(((9 - (v)) & 0x80) >> 7))));
}

/* JSON HEX4DIG token emitter */
static inline void JSMN_EMIT_HEX4DIG(uint8_t *qc, uint32_t c) {
	qc[0] = VALHEX((c >> 12) & 0xF);
	qc[1] = VALHEX((c >> 8)  & 0xF);
	qc[2] = VALHEX((c >> 4)  & 0xF);
	qc[3] = VALHEX( c        & 0xF);
}

/*
 * JSON String Quoting
 */
static inline void JSMN_QUOTE_UNICODE(const uint32_t **cc_ptr, const uint32_t *cs, uint8_t **qc_ptr, const uint8_t *qs) {
	const uint32_t *cc = *cc_ptr;
	uint8_t *qc = *qc_ptr;

	uint32_t __jsmn_char = 0;
	uint32_t hex4dig1    = 0;
	uint32_t hex4dig2    = 0;
	while ((cc) < (cs) && (qc) < qs) {
		if (*(cc) >= 0x20 && *(cc) <= 0x7F) { /* non-control ASCII */
			switch (*(cc)) {
				case '"':
				case '\\':
				/* case '/': */
					__jsmn_char = *(cc);
					break;
				default:
					__jsmn_char = '\0';
					break;
			}
			if (__jsmn_char == '\0') {
				*((qc)++) = *((cc)++);
			} else {
				if ((qc) + 2 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = __jsmn_char;
					(qc) += 2;
					(cc)++;
				} else {
					break;
				}
			}
		} else if (*(cc) >= 0 && *(cc) < 0x20) { /* ASCII control characters */
			switch (*(cc)) {
				case '\b':
					__jsmn_char = 'b';
					break;
				case '\f':
					__jsmn_char = 'f';
					break;
				case '\n':
					__jsmn_char = 'n';
					break;
				case '\r':
					__jsmn_char = 'r';
					break;
				case '\t':
					__jsmn_char = 't';
					break;
				default:
					__jsmn_char = *(cc);
					break;
			}
			if (__jsmn_char >= 0x20) {
				if ((qc) + 2 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = __jsmn_char;
					(qc) += 2;
					(cc)++;
				} else {
					break;
				}
			} else {
				if ((qc) + 6 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = 'u';
					JSMN_EMIT_HEX4DIG((qc) + 2, *(cc));
					(qc) += 6;
					(cc)++;
				} else {
					break;
				}
			}
		} else {
			if (*(cc) < 0x10000) { /* Basic Multilingual Plane */
				if ((qc) + 6 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = 'u';
					JSMN_EMIT_HEX4DIG((qc) + 2, *(cc));
					(qc) += 6;
					(cc)++;
				} else {
					break;
				}
			} else if (*(cc) >= 0x10000 && *(cc) < 0x110000) /* Supplementary Planes */ {
				if ((qc) + 12 <= (qs)) {
					__jsmn_char = (*(cc)) - 0x10000;
					hex4dig1 = 0xD800 + ((__jsmn_char >> 10) & 0x03FF);
					hex4dig2 = 0xDC00 + ( __jsmn_char        & 0x03FF);
					(qc)[0] = '\\';
					(qc)[1] = 'u';
					JSMN_EMIT_HEX4DIG((qc) + 2, hex4dig1);
					(qc)[6] = '\\';
					(qc)[7] = 'u';
					JSMN_EMIT_HEX4DIG((qc) + 8, hex4dig2);
					(qc) += 12;
					(cc)++;
				} else {
					break;
				}
			} else { /* not within a valid Unicode plane */
				if ((qc) + 6 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = 'u';
					(qc)[2] = 'F';
					(qc)[3] = 'F';
					(qc)[4] = 'F';
					(qc)[5] = 'D';
					(qc) += 6;
					(cc)++;
				} else {
					break;
				}
			}
		}
	}

	*cc_ptr = cc;
	*qc_ptr = qc;
	return;
}

static inline void JSMN_QUOTE_ASCII(const uint8_t **cc_ptr, const uint8_t *cs, uint8_t **qc_ptr, const uint8_t *qs) {
	const uint8_t *cc = *cc_ptr;
	uint8_t *qc = *qc_ptr;

	uint32_t __jsmn_char = 0;
	uint32_t hex4dig1    = 0;
	uint32_t hex4dig2    = 0;
	while ((cc) < (cs) && (qc) < qs) {
		if (*(cc) >= 0x20 && *(cc) <= 0x7F) { /* non-control ASCII */
			switch (*(cc)) {
				case '"':
				case '\\':
				/* case '/': */
					__jsmn_char = *(cc);
					break;
				default:
					__jsmn_char = '\0';
					break;
			}
			if (__jsmn_char == '\0') {
				*((qc)++) = *((cc)++);
			} else {
				if ((qc) + 2 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = __jsmn_char;
					(qc) += 2;
					(cc)++;
				} else {
					break;
				}
			}
		} else if (*(cc) >= 0 && *(cc) < 0x20) { /* ASCII control characters */
			switch (*(cc)) {
				case '\b':
					__jsmn_char = 'b';
					break;
				case '\f':
					__jsmn_char = 'f';
					break;
				case '\n':
					__jsmn_char = 'n';
					break;
				case '\r':
					__jsmn_char = 'r';
					break;
				case '\t':
					__jsmn_char = 't';
					break;
				default:
					__jsmn_char = *(cc);
					break;
			}
			if (__jsmn_char >= 0x20) {
				if ((qc) + 2 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = __jsmn_char;
					(qc) += 2;
					(cc)++;
				} else {
					break;
				}
			} else {
				if ((qc) + 6 <= (qs)) {
					(qc)[0] = '\\';
					(qc)[1] = 'u';
					JSMN_EMIT_HEX4DIG((qc) + 2, *(cc));
					(qc) += 6;
					(cc)++;
				} else {
					break;
				}
			}
		} else {
			*((qc)++) = *((cc)++);
		}
	}

	*cc_ptr = cc;
	*qc_ptr = qc;
	return;
}

#define JSMN_QUOTE(cc_ptr, cs, qc_ptr, qs) _Generic((cs), \
    const uint32_t *: JSMN_QUOTE_UNICODE, \
    const uint8_t *: JSMN_QUOTE_ASCII \
)(cc_ptr, cs, qc_ptr, qs)

/* branchless hex-char to int */
static inline int HEXVAL(uint8_t b) {
	return (((b & 0x1f) + ((b >> 6) * 0x19) - 0x10) & 0xF);
}

/* JSON HEX2DIG token parser */
static inline uint32_t JSMN_HEX2DIG_ASCII(const uint8_t *bc) {
	return (HEXVAL(bc[0]) << 4) | HEXVAL(bc[1]);
}
static inline uint32_t JSMN_HEX2DIG_UNICODE(const uint32_t *bc) {
	return (HEXVAL((char) bc[0]) << 4) | HEXVAL((char) bc[1]);
}
#define JSMN_HEX2DIG(bc) _Generic((bc), \
	const uint32_t *: JSMN_HEX2DIG_UNICODE, \
	const uint8_t *: JSMN_HEX2DIG_ASCII \
)(bc)

/* JSON HEX4DIG token parser */
static inline uint32_t JSMN_HEX4DIG_ASCII(const uint8_t *bc) {
	return (HEXVAL(bc[0]) << 12) | (HEXVAL(bc[1]) << 8) | (HEXVAL(bc[2]) << 4) | HEXVAL(bc[3]);
}
static inline uint32_t JSMN_HEX4DIG_UNICODE(const uint32_t *bc) {
	return (HEXVAL(bc[0]) << 12) | (HEXVAL(bc[1]) << 8) | (HEXVAL(bc[2]) << 4) | HEXVAL(bc[3]);
}
#define JSMN_HEX4DIG(bc) _Generic((bc), \
	const uint32_t *: JSMN_HEX4DIG_UNICODE, \
	const uint8_t *: JSMN_HEX4DIG_ASCII \
)(bc)

/*
 * JSON String Unquoting
 */
static inline void JSMN_UNQUOTE_ASCII(const uint8_t **qc_ptr, const uint8_t *qs, uint32_t **cc_ptr, const uint32_t *cs) {
	const uint8_t *qc = *qc_ptr;
	uint32_t *cc = *cc_ptr;

	uint32_t __jsmn_char = 0;
	uint32_t hex4dig1    = 0;
	uint32_t hex4dig2    = 0;
	while (qc < qs && cc < cs) {
		if (*qc == '\\') {
			if (qc + 2 <= qs) {
				switch (qc[1]) {
					case '"':
					case '\\':
					case '/':
						__jsmn_char = qc[1];
						break;
					case 'b':
						__jsmn_char = '\b';
						break;
					case 'f':
						__jsmn_char = '\f';
						break;
					case 'n':
						__jsmn_char = '\n';
						break;
					case 'r':
						__jsmn_char = '\r';
						break;
					case 't':
						__jsmn_char = '\t';
						break;
					case 'u':
						__jsmn_char = 'u';
						break;
				}
				if (__jsmn_char == 'u') {
					if (qc + 6 <= qs) {
						hex4dig1 = JSMN_HEX4DIG(qc + 2);
						if (hex4dig1 >> 10 == 0xD800 >> 10) {
							/* \uD[8-B]?? of the high surrogate pair */
							if (qc + 12 <= qs) {
								if (qc[6] == '\\' && qc[7] == 'u') {
									hex4dig2 = JSMN_HEX4DIG(qc + 8);
									if (hex4dig2 >> 10 == 0xDC00 >> 10) {
										/* \uD[C-F]?? of the low surrogate pair */
										*cc++ = 0x10000 + (((hex4dig1 % 0x400) << 10) | (hex4dig2 % 0x400));
										qc += 12;
									} else {
										*cc++ = 0xFFFD; /* the replacement character */
										qc += 6;
									}
								} else {
									*cc++ = 0xFFFD; /* the replacement character */
									qc += 6;
								}
							} else {
								break; /* blocking for surrogate pair */
							}
						} else if (hex4dig1 >> 10 == 0xDC00 >> 10) {
							/* \uD[C-F]?? of the *unexpected* low surrogate pair */
							*cc++ = 0xFFFD; /* the replacement character */
							qc += 6;
						} else {
							/* Within the Basic Multilingual Plane */
							*cc++ = hex4dig1;
							qc += 6;
						}
					} else {
						break; /* blocking for unicode quote */
					}
				} else {
					/* simple escape */
					*cc++ = __jsmn_char;
					qc += 2;
				}
			} else {
				break; /* blocking for simple escape */
			}
		} else {
			*cc++ = *qc++;
		}
	}

	*qc_ptr = qc;
	*cc_ptr = cc;
	return;
}

static inline void JSMN_UNQUOTE_UNICODE(const uint32_t **qc_ptr, const uint32_t *qs, uint32_t **cc_ptr, const uint32_t *cs) {
	const uint32_t *qc = *qc_ptr;
	uint32_t *cc = *cc_ptr;

	uint32_t __jsmn_char = 0;
	uint32_t hex4dig1    = 0;
	uint32_t hex4dig2    = 0;
	while (qc < qs && cc < cs) {
		if (*qc == '\\') {
			if (qc + 2 <= qs) {
				switch (qc[1]) {
					case '"':
					case '\\':
					case '/':
						__jsmn_char = qc[1];
						break;
					case 'b':
						__jsmn_char = '\b';
						break;
					case 'f':
						__jsmn_char = '\f';
						break;
					case 'n':
						__jsmn_char = '\n';
						break;
					case 'r':
						__jsmn_char = '\r';
						break;
					case 't':
						__jsmn_char = '\t';
						break;
					case 'u':
						__jsmn_char = 'u';
						break;
				}
				if (__jsmn_char == 'u') {
					if (qc + 6 <= qs) {
						hex4dig1 = JSMN_HEX4DIG(qc + 2);
						if (hex4dig1 >> 10 == 0xD800 >> 10) {
							/* \uD[8-B]?? of the high surrogate pair */
							if (qc + 12 <= qs) {
								if (qc[6] == '\\' && qc[7] == 'u') {
									hex4dig2 = JSMN_HEX4DIG(qc + 8);
									if (hex4dig2 >> 10 == 0xDC00 >> 10) {
										/* \uD[C-F]?? of the low surrogate pair */
										*cc++ = 0x10000 + (((hex4dig1 % 0x400) << 10) | (hex4dig2 % 0x400));
										qc += 12;
									} else {
										*cc++ = 0xFFFD; /* the replacement character */
										qc += 6;
									}
								} else {
									*cc++ = 0xFFFD; /* the replacement character */
									qc += 6;
								}
							} else {
								break; /* blocking for surrogate pair */
							}
						} else if (hex4dig1 >> 10 == 0xDC00 >> 10) {
							/* \uD[C-F]?? of the *unexpected* low surrogate pair */
							*cc++ = 0xFFFD; /* the replacement character */
							qc += 6;
						} else {
							/* Within the Basic Multilingual Plane */
							*cc++ = hex4dig1;
							qc += 6;
						}
					} else {
						break; /* blocking for unicode quote */
					}
				} else {
					/* simple escape */
					*cc++ = __jsmn_char;
					qc += 2;
				}
			} else {
				break; /* blocking for simple escape */
			}
		} else {
			*cc++ = *qc++;
		}
	}

	*qc_ptr = qc;
	*cc_ptr = cc;
	return;
}

#define JSMN_UNQUOTE(qc_ptr, qs, cc_ptr, cs) _Generic((qs), \
	const uint32_t *: JSMN_UNQUOTE_UNICODE, \
	const uint8_t *: JSMN_UNQUOTE_ASCII \
)(qc_ptr, qs, cc_ptr, cs)


static inline void JSMN_DECODE_URL(const uint8_t **qc_ptr, const uint8_t *qs, uint8_t **cc_ptr, const uint8_t *cs, int block) {
	const uint8_t *qc = *qc_ptr;
	uint8_t *cc = *cc_ptr;

	uint8_t __jsmn_char = 0;
	while (qc < qs && cc < cs) {
		if (*qc == '%') {
			if (qc + 2 <= qs) {
				*cc++ = (HEXVAL(qc[1]) << 4) | HEXVAL(qc[2]);
				qc += 3;
			} else if (block) {
				break; /* blocking for percent escape */
			} else {
				/* incomplete escape sequence at bs of stream becomes literal */
				*cc++ = *qc++;
			}
		} else {
			*cc++ = *qc++;
		}
	}

	*qc_ptr = qc;
	*cc_ptr = cc;
	return;
}

static inline void JSMN_ENCODE_URL(const uint8_t **cc_ptr, const uint8_t *cs, uint8_t **qc_ptr, const uint8_t *qs, const uint8_t *encode_set) {
	const uint8_t *cc = *cc_ptr;
	uint8_t *qc = *qc_ptr;
	
	uint8_t __jsmn_char = 0;
	while (cc < cs && qc < qs) {
		if (*cc >= 0x20 && *cc <= 0x7F) { /* non-control ASCII */
			if (encode_set[*cc]) {
				if (qc + 3 <= qs) {
					qc[0] = '%';
					qc[1] = VALHEX((*cc >> 4) & 0xF);
					qc[2] = VALHEX(*cc & 0xF);
					qc += 3;
					cc++;
				} else {
					break; /* blocking for percent escape */
				}
			} else {
				*(qc++) = *(cc++);
			}
		} else {
			*(qc++) = *(cc++);
		}
	}

	*cc_ptr = cc;
	*qc_ptr = qc;
	return;
}

/* encode sets for JSMN_URLENCODE */

/* always encode U+0025 (%), the escape character */

/* "The C0 control percent-encode set are the C0 controls and all code points greater than U+007E (~)."" */
static uint8_t jsmn_urlencode_set_c0[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	/* up to 0x7e is clear */
	0, 0, 0, 0, 0, 1, 0, 0, /* 0x20 - 0x27: U+0025 (%) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x38 - 0x3f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
	0, 0, 0, 0, 0, 0, 0, 1, /* 0x78 - 0x7f */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* "The fragment percent-encode set is the C0 control percent-encode set and U+0020 SPACE, U+0022 ("), U+003C (<), U+003E (>), and U+0060 (`). " */
static uint8_t jsmn_urlencode_set_fragment[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 0, 0, 1, 0, 0, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0025 (%) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 0, 0, 1, 0, 1, 0, /* 0x38 - 0x3f: U+003C (<), U+003E (>) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5f */
	1, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67: U+0060 (`) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
	0, 0, 0, 0, 0, 0, 0, 1, /* 0x78 - 0x7f */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* The query percent-encode set is the C0 control percent-encode set and U+0020 SPACE, U+0022 ("), U+0023 (#), U+003C (<), and U+003E (>). */
static uint8_t jsmn_urlencode_set_query[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 0, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0023 (#), U+0025 (%) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 0, 0, 1, 0, 1, 0, /* 0x38 - 0x3f: U+003C (<), U+003E (>) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
	0, 0, 0, 0, 0, 0, 0, 1, /* 0x78 - 0x7f */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* The special-query percent-encode set is the query percent-encode set and U+0027 ('). */
static uint8_t jsmn_urlencode_set_special_query[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 1, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0023 (#), U+0025 (%), U+0027 (') */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 0, 0, 1, 0, 1, 0, /* 0x38 - 0x3f: U+003C (<), U+003E (>) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
	0, 0, 0, 0, 0, 0, 0, 1, /* 0x78 - 0x7f */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* The path percent-encode set is the query percent-encode set and U+003F (?), U+0060 (`), U+007B ({), and U+007D (}). */
static uint8_t jsmn_urlencode_set_path[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 0, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0023 (#), U+0025 (%) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 0, 0, 1, 0, 1, 1, /* 0x38 - 0x3f: U+003C (<), U+003E (>), U+003F (?) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5f */
	1, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67: U+0060 (`) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
	0, 0, 0, 1, 0, 1, 0, 1, /* 0x78 - 0x7f: U+007B ({), and U+007D (}). */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* The userinfo percent-encode set is the path percent-encode set and U+002F (/), U+003A (:), U+003B (;), U+003D (=), U+0040 (@), U+005B ([) to U+005E (^), inclusive, and U+007C (|).  */
static uint8_t jsmn_urlencode_set_userinfo[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 1, 0, 1, 0, 0, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0023 (#), U+0025 (%) */
	0, 0, 0, 0, 0, 0, 0, 1, /* 0x28 - 0x2f: U+002F (/) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 1, 1, 1, 1, 1, 1, /* 0x38 - 0x3f: U+003A (:), U+003B (;), U+003C (<), U+003D (=), U+003E (>), U+003F (?) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 1, 1, 1, 1, 0, /* 0x58 - 0x5f: U+005B ([) to U+005E (^), inclusive */
	1, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67: U+0060 (`) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 1, 0, 0, 0, /* 0x70 - 0x77: U+007C (|) */
	0, 0, 0, 1, 0, 1, 0, 1, /* 0x78 - 0x7f: U+007B ({), and U+007D (}). */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

/* The component percent-encode set is the userinfo percent-encode set and U+0024 ($) to U+0026 (&), inclusive, U+002B (+), and U+002C (,). */
static uint8_t jsmn_urlencode_set_component[8*32] = {
	/* 0x00 - 0x1F: control characters are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 - 0x07 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x08 - 0x0f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x10 - 0x17 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x18 - 0x1f */
	1, 0, 1, 1, 1, 1, 1, 0, /* 0x20 - 0x27: U+0020 SPACE, U+0022 ("), U+0023 (#), U+0024 ($) to U+0026 (&), inclusive */
	0, 0, 0, 1, 1, 0, 0, 1, /* 0x28 - 0x2f: U+002B (+), U+002C (,), U+002F (/) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x37 */
	0, 0, 1, 1, 1, 1, 1, 1, /* 0x38 - 0x3f: U+003A (:), U+003B (;), U+003C (<), U+003D (=), U+003E (>), U+003F (?) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x47 */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x48 - 0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x57 */
	0, 0, 0, 1, 1, 1, 1, 0, /* 0x58 - 0x5f: U+005B ([) to U+005E (^), inclusive */
	1, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67: U+0060 (`) */
	0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6f */
	0, 0, 0, 0, 1, 0, 0, 0, /* 0x70 - 0x77: U+007C (|) */
	0, 0, 0, 1, 0, 1, 0, 1, /* 0x78 - 0x7f: U+007B ({), and U+007D (}). */
	/* above 0x7e are set */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x80 - 0x87 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x88 - 0x8f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x90 - 0x97 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0x98 - 0x9f */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa0 - 0xa7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xa8 - 0xaf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb0 - 0xb7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xb8 - 0xbf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xc7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xc8 - 0xcf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xd7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xd8 - 0xdf */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe0 - 0xe7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xe8 - 0xef */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf0 - 0xf7 */
	1, 1, 1, 1, 1, 1, 1, 1, /* 0xf8 - 0xff */
};

#ifdef UTF8_TEST
/*
 * $ cc -DUTF8_TEST utf8.c -o utf8_test
 * $ ./utf8_test
 */
#include <stdio.h>
#include <string.h>
int test_utf8() {
	int s = 0;
	uint32_t in_c;
	uint8_t    im_b[6];
	uint32_t out_c;

	/*
	for (int i_c = 1; i_c < 0x110000; i_c++) {
		in_c = 49;
	*/
	for (in_c = 0; in_c < 0x110000; in_c++) { 
		uint32_t *in_cc  =  &in_c;
		uint32_t *in_cs  = (&in_c) + 1;
		uint8_t    *im_bc  =  im_b;
		uint8_t    *im_bs  =  im_b + 6;
		uint32_t *out_cc =  &out_c;
		uint32_t *out_cs = (&out_c) + 1;

		/*
		if (i_c == 0xd800) {
			i_c = 0xe000;
		}
		*/
		if (in_c == 0xd800) {
			in_c = 0xe000;
		}

		memset(im_b, 0, 6);

		UTF8_ENCODE((const uint32_t **) &in_cc, (const uint32_t *) in_cs, &im_bc, im_bs);

		/* 
		printf("bin: ");
		for (int i = 0; i < 32; i++) {
			if (i%8 == 0) {
				printf(" ");
			}
			printf(im_b[i/8] & (0x80 >> (i%8)) ? "1" : "0");
		}
		printf("\n");
		*/

		im_bc = im_b;
		UTF8_DECODE((const uint8_t **) &im_bc, (const uint8_t *) im_bs, &out_cc, out_cs);
		if (in_c != out_c) {
			printf("Error on UTF-8 character %i = %i\n.", (int) in_c, (int) out_c);
			return 1;
		}
		s++;
	}

	printf("Succeeded converting all %i UTF-8 characters.\n", s);

	return 0;
}

int test_utf16() {
	int s = 0;
	uint32_t in_c;
	uint16_t    im_b[3];
	uint32_t out_c;

	for (in_c = 0; in_c < 0x110000; in_c++) { 
		uint32_t *in_cc  =  &in_c;
		uint32_t *in_cs  = (&in_c) + 1;
		uint16_t *im_bc  =  im_b;
		uint16_t *im_bs  =  im_b + 3;
		uint32_t *out_cc =  &out_c;
		uint32_t *out_cs = (&out_c) + 1;

		if (in_c == 0xd800) {
			in_c = 0xe000;
		}

		memset(im_b, 0, sizeof(uint16_t) * 3);

		UTF16_ENCODE((const uint32_t **) &in_cc, (const uint32_t *) in_cs, &im_bc, im_bs);

		im_bc = im_b;
		UTF16_DECODE((const uint16_t **) &im_bc, (const uint16_t *) im_bs, &out_cc, out_cs);
		if (in_c != out_c) {
			printf("Error on UTF-16 character %i = %i\n.", (int) in_c, (int) out_c);
			return 1;
		}
		s++;
	}

	printf("Succeeded converting all %i UTF-16 characters.\n", s);

	return 0;
}

int test_quote() {
	int s = 0;
	uint32_t in_c;
	uint8_t im_c[12];
	uint32_t out_c;

	for (in_c = 0; in_c < 0x110000; in_c++) { 
		uint32_t *in_cc  =  &in_c;
		uint32_t *in_cs  = (&in_c) + 1;
		uint8_t *im_cc  =  im_c;
		uint8_t *im_cs  =  im_c + 12;
		uint32_t *out_cc =  &out_c;
		uint32_t *out_cs = (&out_c) + 1;

		if (in_c == 0xd800) {
			in_c = 0xe000;
		}

		/*
		printf("in_c(%i): %lc\n", (int) in_c, in_c);
		*/

		memset(im_c, 0, sizeof (char) * 12);

		JSMN_QUOTE((const uint32_t **) &in_cc, (const uint32_t *) in_cs, &im_cc, im_cs);

		/*
		printf("im_c: ");
		for (int i = 0; i < 12; i++) {
			printf("%lc", im_c[i]);
		}
		printf("\n");
		*/

		im_cc = im_c;
		JSMN_UNQUOTE((const uint8_t **) &im_cc, (const uint8_t *) im_cs, &out_cc, out_cs);
		/*
		printf("out_c(%i): %lc\n", (int) out_c, out_c);
		*/
		if (in_c != out_c) {
			printf("Error on JSON character %i = %i\n.", (int) in_c, (int) out_c);
			return 1;
		}
		s++;
	}

	printf("Succeeded converting all %i JSON Unicode characters.\n", s);


	return 0;
}

int test_quote_ascii() {
	int s = 0;
	uint8_t in_c;
	uint8_t im_c[12];
	uint32_t out_c;

	for (in_c = 0; s <= 0xFF; in_c++) { 
		uint8_t *in_cc  =  &in_c;
		uint8_t *in_cs  = (&in_c) + 1;
		uint8_t *im_cc  =  im_c;
		uint8_t *im_cs  =  im_c + 12;
		uint32_t *out_cc =  &out_c;
		uint32_t *out_cs = (&out_c) + 1;

		/*
		printf("in_c(%i): %lc\n", (int) in_c, in_c);
		*/

		memset(im_c, 0, sizeof (char) * 12);

		JSMN_QUOTE((const uint8_t **) &in_cc, (const uint8_t *) in_cs, &im_cc, im_cs);

		/*
		printf("im_c: ");
		for (int i = 0; i < 12; i++) {
			printf("%lc", im_c[i]);
		}
		printf("\n");
		*/

		im_cc = im_c;
		JSMN_UNQUOTE((const uint8_t **) &im_cc, (const uint8_t *) im_cs, &out_cc, out_cs);
		/*
		printf("out_c(%i): %lc\n", (int) out_c, out_c);
		*/
		if (in_c != out_c) {
			printf("Error on JSON character %i = %i\n.", (int) in_c, (int) out_c);
			return 1;
		}
		s++;
	}

	printf("Succeeded converting all %i JSON ASCII characters.\n", s);


	return 0;
}

int test_encode_url_set(const uint8_t *encode_set, const uint8_t *set_name) {
	/* Test URL encode/decode cycle with the given encoding set. */
	int s = 0;
	uint8_t in_c;
	uint8_t im_c[12];
	uint8_t out_c;

	for (in_c = 0; ; in_c++) { 
		uint8_t *in_cc  =  &in_c;
		uint8_t *in_cs  = (&in_c) + 1;
		uint8_t *im_cc  =  im_c;
		uint8_t *im_cs  =  im_c + 12;
		uint8_t *out_cc =  &out_c;
		uint8_t *out_cs = (&out_c) + 1;

		/*
		printf("in_c(%i): %lc\n", (int) in_c, in_c);
		*/

		memset(im_c, 0, sizeof (char) * 12);

		JSMN_ENCODE_URL((const uint8_t **) &in_cc, (const uint8_t *) in_cs, &im_cc, im_cs, encode_set);

		/*
		printf("im_c: ");
		for (int i = 0; i < 12; i++) {
			printf("%lc", im_c[i]);
		}
		printf("\n");
		*/

		im_cc = im_c;
		JSMN_DECODE_URL((const uint8_t **) &im_cc, (const uint8_t *) im_cs, &out_cc, out_cs, 0);
		/*
		printf("out_c(%i): %lc\n", (int) out_c, out_c);
		*/
		if (in_c != out_c) {
			printf("Error on URL character 0x%x = 0x%x\n.", (int) in_c, (int) out_c);
			return 1;
		}
		s++;

		if (in_c == (uint8_t) 0xFF) {
			break;
		}
	}

	printf("Succeeded converting all %i URL characters of set %s.\n", s, set_name);

	return 0;
}

int test_encode_url_all() {
	/* Test URL encode/decode cycle with all encoding sets. */
	int rc;

	rc = test_encode_url_set(jsmn_urlencode_set_query, "jsmn_urlencode_set_query");
	if (rc != 0) {
		return rc;
	}

	rc = test_encode_url_set(jsmn_urlencode_set_special_query, "jsmn_urlencode_set_special_query");
	if (rc != 0) {
		return rc;
	}

	rc = test_encode_url_set(jsmn_urlencode_set_path, "jsmn_urlencode_set_path");
	if (rc != 0) {
		return rc;
	}

	rc = test_encode_url_set(jsmn_urlencode_set_userinfo, "jsmn_urlencode_set_userinfo");
	if (rc != 0) {
		return rc;
	}

	rc = test_encode_url_set(jsmn_urlencode_set_component, "jsmn_urlencode_set_component");
	if (rc != 0) {
		return rc;
	}

	return rc;
}

int main() {
	int rc;

	rc = test_utf8();
	if (rc != 0) {
		return rc;
	}

	rc = test_utf16();
	if (rc != 0) {
		return rc;
	}

	rc = test_quote();
	if (rc != 0) {
		return rc;
	}

	rc = test_quote_ascii();
	if (rc != 0) {
		return rc;
	}

	rc = test_encode_url_all();
	if (rc == -1) {
		return rc;
	}

	return rc;
}
#endif

#endif
