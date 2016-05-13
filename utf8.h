#ifndef UTF8_H
#define UTF8_H
#include <stddef.h>

/*
 * `bc` is byte cursor
 * `bs` is byte stop
 * `cc` is character cusor
 * `cs` is character stop
 * `c`  is character (`*cc`)
 * `l`  is character byte length
 * `i'  is index into `bc`
 * `n`  is the number of valid bytes
 */

/*
 * UTF-8 Byte Shift
 * Given a sequence byte index `i`, and a sequence byte length `l', return the bit shift amount for the bits in this byte.
 */
#define UTF8_NSHIFT(i, l) (6 * ((l) - (i) - 1))

/*
 * UTF-8 Character Byte Length from Byte 1
 * Measures the number of contiguous high set bits.
 *   l == 0: ASCII
 *   l == 1: continuation byte
 *   l >= 2: UTF-8 sequence length
 *   l >  6: malformed byte
 */
#define UTF8_BLEN(bc, l) { \
	for (l = 0; l < 6; l++) { \
		if ((((bc)[0]) & (0x80 >> (l))) == 0) { \
			break; \
		} \
	} \
}

/*
 * UTF-8 Decode Byte 1
 * Set bits from the first byte into the character.
 * Increment `n` on success.
 */
#define UTF8_B1(bc, c, l, n) { \
	if ((l) == 0) {  /* ASCII */ \
		(c) = (bc)[0]; \
		(l) = 1; \
		(n)++; \
	} else if ((l) >= 2 && (l) <= 6) { /* sequence start */ \
		(c) = ((bc)[0] & ((1 << (7 - (l))) - 1)) << UTF8_NSHIFT(0, l); \
		(n)++; \
	} else if ((l) == 1) { /* unexpected sequence continuation */ \
		(l) = 1; \
	} else if ((l) >  6) { /* malformed */ \
		(l) = 1; \
	} else { /* insane negative value */ \
		(l) = 1; \
	} \
}

/*
 * UTF-8 Decode Continuation Byte (2+)
 * Set bits from the continuation byte into the character.
 * Increment `n` on success.
 */
#define UTF8_BN(bc, i, c, l, n) { \
	if ((n) > 0 && ((bc)[i] & 0xc0) == 0x80) { \
		(c) |= ((bc)[i] & 0x3f) << UTF8_NSHIFT(i, l); \
		(n)++; \
	} \
}

/*
 * UTF-8 Character Validator
 * Flip the sign of `l` if the chracter is invalid.
 */
#define UTF8_VALID(c, l) { \
	if ((l) == 1 && (c) < 0x80) { /* ASCII */ \
	} else if ( \
			((l) < 1 || (l) > 4)                                 /* sequence length range */ \
		||	((c) > 0x10FFFF || ((c) >= 0xd800 && (c) <= 0xdfff)) /* code point range */ \
		||	((l) == 1 && (                 (c) >= 0x80))         /* over long */ \
		||	((l) == 2 && ((c) < 0x80    || (c) >= 0x800))        /* over long */ \
		||	((l) == 3 && ((c) < 0x800   || (c) >= 0x10000))      /* over long */ \
		||	((l) == 4 && ((c) < 0x10000 || (c) >= 0x200000))     /* over long */ \
	) { \
		(l) = - (l); \
	} \
}

/*
 * UTF-8 Decode Character
 * Set the character bits from the byte cursor.
 * Set `l`:
 *   l >  0:  `l` length of a valid UTF-8 sequence.
 *   l == 0:  `l` is too long for `bl`.
 *   l <  0: `-l` length of an invalid UTF-8 sequence.
 */
#define UTF8_CHAR(bc, bs, c, l) { \
	int __utf8_seqlen; \
	int __utf8_n = 0; \
	UTF8_BLEN(bc, __utf8_seqlen); \
	UTF8_B1(bc, c, __utf8_seqlen, __utf8_n); \
	if (__utf8_seqlen == 1) { \
		if (__utf8_n == 1) { /* ASCII */ \
			(l) = __utf8_seqlen; \
		} else { /* invalid start byte */ \
			(l) = -1; \
		} \
	} else if ((bc) + __utf8_seqlen <= (bs)) { \
		switch (__utf8_seqlen) { \
			case 6: \
				UTF8_BN(bc, 5, c, __utf8_seqlen, __utf8_n); \
			case 5: \
				UTF8_BN(bc, 4, c, __utf8_seqlen, __utf8_n); \
			case 4: \
				UTF8_BN(bc, 3, c, __utf8_seqlen, __utf8_n); \
			case 3: \
				UTF8_BN(bc, 2, c, __utf8_seqlen, __utf8_n); \
			case 2: \
				UTF8_BN(bc, 1, c, __utf8_seqlen, __utf8_n); \
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
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
#define UTF8_DECODE(bc, bs, cc, cs) { \
	int __utf8_seqlen2; \
	while ((bc) < (bs) && (cc) < (cs)) { \
		UTF8_CHAR(bc, bs, *(cc), __utf8_seqlen2); \
		if (__utf8_seqlen2 > 0) { /* valid character of ASCII or UTF-8 */ \
			(bc) += ( + __utf8_seqlen2); \
		} else if (__utf8_seqlen2 == 0) { \
			break; /* blocking on byte length */ \
		} else { \
			*(cc) = 0xFFFD; /* represent invalid sequence with the replacement character */ \
			(bc) += ( - __utf8_seqlen2); \
		} \
		(cc)++; \
	} \
}

/*
 * UTF-8 Character Byte Length
 * Set the byte length from the character,
 */
#define UTF8_CLEN(c, l) { \
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
 * Sets bits from the character into the first byte, and set the sequence start high-bits.
 */
#define UTF8_C1(c, bc, l) { \
	(bc)[0] = ((0xFF << (8 - l)) & 0xFF) | (((c) >> UTF8_NSHIFT(0, l)) & ((1 << (7 - l)) - 1)); \
}

/*
 * UTF-8 Encode Character Continuation Byte (2+)
 * Sets bits from the chraacter into the continuation byte as index `i`, and set the continuation high-bits.
 */
#define UTF8_CN(c, bc, i, l) { \
	(bc)[i] = 0x80 | ((c) >> UTF8_NSHIFT(i, l)) & 0x3f; \
}

/*
 * UTF8-8 Encode String
 * The cursors will be updated as UTF-8 is parsed and characters are emitted, until:
 *  1. a cursor reaches a stop address.
 *  2. a complete sequence would run past the byte stop address.
 */
#define UTF8_ENCODE(cc, cs, bc, bs) { \
	int __utf8_seqlen; \
	wchar_t c; \
	while ((cc) < (cs) && (bc) < (bs)) { \
		UTF8_CLEN(*(cc), __utf8_seqlen); \
		if (__utf8_seqlen == 1) { /* ASCII */ \
			*((bc)++) = *((cc)++); \
		} else if (__utf8_seqlen > 1) { \
			if ((bc) + __utf8_seqlen <= (bs)) { /* character fits */ \
				c = *((cc)++); \
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
				(bc) += __utf8_seqlen; \
			} else { \
				break; /* blocking on byte length */ \
			} \
		} else { \
			(cc)++; \
			/* XXX: silently skip insane character */ \
		} \
	} \
}

#define JSMN_CHAR_QUOTE(cc, cs, qc, qs) { \
	while ((cc) < (cs) && (qc) < qs) { \
		 \
	} \
}

#ifdef UTF8_TEST
/*
 * $ cc -DUTF8_TEST utf8.h -o utf8_test
 * $ ./utf8_test
 */
#include <stdio.h>
#include <string.h>
int main() {
	wchar_t in_c;
	char    im_b[6];
	wchar_t out_c;

	for (in_c = 1; in_c < 0x110000; in_c++) {
		wchar_t *in_cc  =  &in_c;
		wchar_t *in_cs  = (&in_c) + 1;
		char    *im_bc  =  im_b;
		char    *im_bs  =  im_b + 6;
		wchar_t *out_cc =  &out_c;
		wchar_t *out_cs = (&out_c) + 1;

		memset(im_b, 0, 6);

		if (in_c == 0xd800) {
			/* skip */
			in_c = 0xe000;
		}

		UTF8_ENCODE(in_cc, in_cs, im_bc, im_bs);

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
		UTF8_DECODE(im_bc, im_bs, out_cc, out_cs);
		if (in_c != out_c) {
			printf("Error on character %i = %i\n.", (int) in_c, (int) out_c);
			/* return 1; */
		}
	}

	printf("Succeeded converting all characters.\n");

	return 0;
}
#endif

#endif
