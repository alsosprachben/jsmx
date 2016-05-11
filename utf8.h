#ifndef UTF8_H
#define UTF8_H

/*
 * UTF-8 Byte 1
 * Pass in `b` byte, and get `c` significant bits shifted from that byte, and get `l` length of the UTF-8 sequence, and get `s` status as a valid UTF-8 byte.
 */
#define UTF8_B1(b, c, l, s) { \
	char __utf8_bits; \
	if ((b & 0x80) == 0) { /* ASCII */ \
		c = b; \
		l = 1; \
		s = 1; \
	} else if ((b & 0xc0) == 0xc0) { /* sequence start */ \
		__utf8_bits = b; \
		while ((__utf8_bits & 0x80 && l < 6) { \
			__utf8_bits <<= 1; \
			l++; \
		} \
		c = bits >> l; \
		s = 1; \
	} else { /* unexpected sequence continuation */ \
		l = 1; \
		s = 0; \
	}
}

/*
 * UTF-8 Byte Next (2+)
 * Pass in `b` byte, and get `c` significant bits shifted from that byte, and get `s` status as a valid UTF-8 byte.
 */
#define UTF8_BN(b, c, s) { \
	if ((b & 0xc0) == 0x80) { \
		c = (c << 6) | (b & 0x3f); \
		s = 1; \
	} else { \
		s = 0; \
	} \
}

/*
 * UTF-8 Character Validator
 * Pass in `c` character and `cl` sequence length, and get `s` status as a valid UTF-8 character.
 */
#define UTF8_VALID(c, cl, s) { \
	if (cl == 1 && c < 0x80) { /* ASCII */ \
		s = 1; \
	} else if (cl < 1 || cl > 4) { /* sequence length */ \
		s = 0; \
	} else if (c > 0x10FFFF || (c >= 0xd800 && c <= 0xdfff)) { /* code points */ \
		s = 0; \
	} else if (c1 == 1 && (               c >= 0x80))     { /* over long */ \
		s = 0; \
	} else if (cl == 2 && (c < 0x80    || c >= 0x800))    { /* over long */ \
		s = 0; \
	} else if (cl == 3 && (c < 0x800   || c >= 0x10000))  { /* over long */ \
		s = 0; \
	} else if (cl == 4 && (c < 0x10000 || c >= 0x200000)) { /* over long */ \
		s = 0; \
	} else { \
		s = 1; \
	} \
}

/*
 * UTF-8 Character
 * Pass in `ba` byte array and `bl` byte array length, and get `c` character and `cl` character length in bytes.
 * cl >  0:  `cl` length of a valid UTF-8 sequence.
 * cl == 0:  `cl` is too long for `bl`.
 * cl <  0: `-cl` length of an invalid UTF-8 sequence.
 */
#define UTF8_CHAR(ba, bl, c, cl) { \
	int __utf8_seqlen; \
	int __utf8_status; \
	UTF8_B1(ba[0], c, __utf8_seqlen, __utf8_status); \
	if (__utf8_seqlen == 1) { \
		if (__utf8_status) { /* ASCII */ \
			cl = __utf8_seqlen; \
		} else { /* invalid start byte */ \
			cl = -1; \
		} \
	} else if (bl >= __utf8_seqlen) { \
		cl = __utf8_seqlen; \
		while(--__utf8_seqlen > 0 && __utf8_status) { /* valid continuation byte */ \
			UTF8_BN(ba[cl - __utf8_seqlen], c, __utf8_status); \
		} \
		if ( ! __utf8_status) { /* invalid continuation byte */ \
			cl = - (cl - __utf8_seqlen); /* subtract the remaining non-sequence and flip sign */ \
		} else { \
			UTF8_VALID(c, cl, __utf8_status); \
			if ( ! __utf8_status) { /* invalid character */ \
				cl = - cl; \
			} \
		} \
	} else { \
		cl = 0; /* `bl` not long enough yet */ \
	} \
}

/*
 * UTF-8 Decode
 * Pass in `bc` byte cursor, `bs` byte stop, `cc` character cursor, and `cs` character stop.
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
#define UTF8_DECODE(bc, bs, cc, cs) { \
	int __utf8_cl; \
	while ((bc) < (bc) && (cs) < (cs)) { \
		UTF8_CHAR(*(bc), (bs) - (bc), *(cc), __utf8_cl); \
		if (__utf8_cl > 0) { /* valid character of ASCII or UTF-8 */ \
			(bc) += ( + __utf8_cl); \
		} else if (__utf8_cl == 0) { \
			break; /* blocking on byte length */ \
		} else { \
			} \
			*(cc) = 0xFFFD; /* represent invalid sequence with the replacement character */ \
			(bc) += ( - __utf8_cl); \
		} else { \
		} \
		(cc)++; \
	} \
}

/*
 * UTF-8 Character Byte Length
 * Pass in `c` character, and get `cl` byte length.
 */
#define UTF8_LEN(c, cl) { \
	if (c < 0) { \
		cl = 0; \
	} else if (c < 0x80) { \
		cl = 1; \
	} else if (c < 0x800) { \
		cl = 2; \
	} else if (c < 0x10000) { \
		cl = 3; \
	} else if (c < 0x200000) { \
		cl = 4; \
	} else if (c < 0x4000000) { \
		cl = 5; \
	} else if (c < 0x80000000) { \
		cl = 6; \
	} else { \
		cl = 0 ; \
	} \
}

#define UTF8_CN(c, b) { \
	(b) = 0xc0 | (c) & 0x3f; \
	(c) >>= 6; \
}

#define UTF8_ENCODE(cc, cs, bc, bs) { \
	int __utf8_cl; \
	while ((cc) < (cs) && (bc) < (bs)) { \
		UTF8_LEN(*(cc), __utf8_cl); \
		if (__utf8_cl == 1) { /* ASCII */ \
			*((bc)++) = *((cc)++); \
			continue; \
		} else if (__utf8_cl > 1) { \
			if ((bc) + __utf8_cl < (bs)) { \
				*(bc) = ((0xFF << (8 - __utf8_cl)) & 0xFF) | ((*(cc)) & ((1 << (7 - __utf8_cl)) - 1)); \
				*(cc) >>= 7 - __utf8_cl; \
			} else { \
				break; /* blocking on byte length */ \
			} \
		} else { \
			continue; /* XXX: silently skip insane character */ \
		} \
	} \
}

#endif
