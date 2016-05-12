#ifndef UTF8_H
#define UTF8_H

#define UTF8_NSHIFT(i, l) (6 * (l) - (i) - 1)
/*
 * UTF-8 Decode Byte 1
 * Pass in `b` byte, and get `c` significant bits shifted from that byte, and get `l` length of the UTF-8 sequence, and increment `n` if a valid UTF-8 byte.
 */
#define UTF8_B1(bc, c, l, n) { \
	char __utf8_bits; \
	n = 0; \
	if (((bc)[0] & 0x80) == 0) { /* ASCII */ \
		(c) = (bc)[0]; \
		(l) = 1; \
		(n)++; \
	} else if (((bc)[0] & 0xc0) == 0xc0) { /* sequence start */ \
		__utf8_bits = (bc)[0]; \
		while (__utf8_bits & 0x80 && (l) < 6) { \
			__utf8_bits <<= 1; \
			(l)++; \
		} \
		__utf8_bits >>= (l); \
		(c) = __utf8_bits << UTF8_NSHIFT(0, l); \
		(n)++; \
	} else { /* unexpected sequence continuation */ \
		(l) = 1; \
	}
}

/*
 * UTF-8 Decode Continuation Byte (2+)
 * Pass in `b` byte, and `l` byte length, and get `c` significant bits shifted from that byte, and increment `n` if a valid UTF-8 byte.
 */
#define UTF8_BN(bc, i, c, l, n) { \
	if ((n) > 0 && ((bc)[i] & 0xc0) == 0x80) { \
		(c) |= ((bc)[i] & 0x3f) << UTF8_NSHIFT(i, l); \
		(n)++; \
	} \
}

/*
 * UTF-8 Character Validator
 * Pass in `c` character and `l` sequence length, and flip the sign of `l` if an valid UTF-8 character.
 */
#define UTF8_VALID(c, l) { \
	if ((l) == 1 && (c) < 0x80) { /* ASCII */ \
	} else if ((l) < 1 || (l) > 4) { /* sequence length */ \
	} else if ((c) > 0x10FFFF || ((c) >= 0xd800 && (c) <= 0xdfff)) { /* code points */ \
	} else if ((l) == 1 && (                 (c) >= 0x80))     { /* over long */ \
	} else if ((l) == 2 && ((c) < 0x80    || (c) >= 0x800))    { /* over long */ \
	} else if ((l) == 3 && ((c) < 0x800   || (c) >= 0x10000))  { /* over long */ \
	} else if ((l) == 4 && ((c) < 0x10000 || (c) >= 0x200000)) { /* over long */ \
	} else { \
		(l) = - (l); \
	} \
}

/*
 * UTF-8 Decode Character
 * Pass in `ba` byte array and `bl` byte array length, and get `c` character and `cl` character length in bytes.
 * cl >  0:  `cl` length of a valid UTF-8 sequence.
 * cl == 0:  `cl` is too long for `bl`.
 * cl <  0: `-cl` length of an invalid UTF-8 sequence.
 */
#define UTF8_CHAR(bc, bs, c, l) { \
	int __utf8_seqlen; \
	int __utf8_n; \
	UTF8_B1(bc, c, __utf8_seqlen, __utf8_n); \
	if (__utf8_seqlen == 1) { \
		if (__utf8_n == 1) { /* ASCII */ \
			(l) = __utf8_seqlen; \
		} else { /* invalid start byte */ \
			(l) = -1; \
		} \
	} else if ((bc) + __utf8_seqlen < (bs)) { \
		switch (__utf8_seqlen) { \
			case 6: \
				UTF8_BN((bc), 5, c, __utf8_seqlen, __utf8_n); \
			case 5: \
				UTF8_BN((bc), 4, c, __utf8_seqlen, __utf8_n); \
			case 4: \
				UTF8_BN((bc), 3, c, __utf8_seqlen, __utf8_n); \
			case 3: \
				UTF8_BN((bc), 2, c, __utf8_seqlen, __utf8_n); \
			case 2: \
				UTF8_BN((bc), 1, c, __utf8_seqlen, __utf8_n); \
				break; \
		} \
		(l) = __utf8_n; \
		if (__utf8_n < __utf8_seqlen) { \
			(l) = - (l); /* error the invalid byte sequence */ \
		} else { \
			UTF8_VALID(c, l); /* error any invalid characters */ \
		} \
	} else { \
		(l) = 0; /* `bs - bc` not long enough yet */ \
	} \
}

/*
 * UTF-8 Decode String
 * Pass in `bc` byte cursor, `bs` byte stop, `cc` character cursor, and `cs` character stop.
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
#define UTF8_DECODE(bc, bs, cc, cs) { \
	int __utf8_cl; \
	while ((bc) < (bc) && (cs) < (cs)) { \
		UTF8_CHAR(bc, bs, *(cc), __utf8_cl); \
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
#define UTF8_LEN(c, l) { \
	if ((c) < 0) { \
		(l) = 0; \
	} else if ((c) < 0x80) { \
		(l) = 1; \
	} else if ((c) < 0x800) { \
		(l) = 2; \
	} else if ((c) < 0x10000) { \
		(l) = 3; \
	} else if ((c) < 0x200000) { \
		(l) = 4; \
	} else if ((c) < 0x4000000) { \
		(l) = 5; \
	} else if ((c) < 0x80000000) { \
		(l) = 6; \
	} else { \
		(l) = 0 ; \
	} \
}

/*
 * UTF-8 Encode Character Byte 1
 * Pass in `l` byte length from UTF8_LEN(), and get bits from `c` shifted into `b` based on `l`.
 */
#define UTF8_C1(c, bc, l) { \
	(bc)[0] = ((0xFF << (8 - l)) & 0xFF) | (((c) >> UTF8_NSHIFT(0, l)) & ((1 << (7 - l)) - 1)); \
}

/*
 * UTF-8 Encode Character Continuation Byte (2+)
 * Get 6 bits from `c` shifted into `b` with the continuation high-bits set.
 */
#define UTF8_CN(c, bc, i, l) { \
	(bc)[i] = 0xc0 | ((c) >> UTF8_NSHIFT(i, l)) & 0x3f; \
}

/*
 * UTF8-8 Encode String
 * Pass in `cc` character cursor, `cs` character stop, `bc` byte cursor, and `bs` byte stop.
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
#define UTF8_ENCODE(cc, cs, bc, bs) { \
	int __utf8_seqlen; \
	wchar_t c; \
	while ((cc) < (cs) && (bc) < (bs)) { \
		UTF8_LEN(*(cc), __utf8_seqlen); \
		if (__utf8_seqlen == 1) { /* ASCII */ \
			*((bc)++) = *((cc)++); \
			continue; \
		} else if (__utf8_seqlen > 1) { \
			if ((bc) + __utf8_seqlen < (bs)) { /* character fits */ \
				c = *((cc)++); \
				__utf8_n = 0; \
				UTF8_C1(c, bc, __utf8_seqlen); \
				switch (__utf8_seqlen) { \
					case 6: \
						UTF8_CN(c, bc, 5, __utf8_seqlen); \
					case 5: \
						UTF8_CN(c, bc, 4, __utf8_seqlen); \
					case 4: \
						UTF8_CN(c, bc, 3, __utf8_seqlen); \
					case 3: \
						UTF8_CN(c, bc, 2, __utf8_seqlen); \
					case 2: \
						UTF8_CN(c, bc, 1, __utf8_seqlen); \
						break; \
				} \
				(bc) += __utf8_seqlen;
			} else { \
				break; /* blocking on byte length */ \
			} \
		} else { \
			continue; /* XXX: silently skip insane character */ \
		} \
	} \
}

#define JSMN_CHAR_QUOTE(cc, cs, qc, qs) { \
	while ((cc) < (cs) && (qc) < qs) { \
		 \
	} \
}

#endif
