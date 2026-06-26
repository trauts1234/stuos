#include "wchar.h"
#include "stdint.h"
#include "string.h"
#include "uapi/stddef.h"
#include "wctype.h"
#include "errno.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned char tab[];

static const unsigned char rulebases[512];
static const int rules[];

static const unsigned char exceptions[][2];

#include "casemap.h"

//nabbed from musl
static const unsigned char alpha_table[] = {
#include "alpha.h"
};
static const unsigned char punct_table[] = {
#include "punct.h"
};

size_t wcslen(const wchar_t *s)
{
	const wchar_t *a;
	for (a=s; *s; s++);
	return s-a;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
	if (!c) return (wchar_t *)s + wcslen(s);
	for (; *s && *s != c; s++);
	return *s ? (wchar_t *)s : 0;
}

wctype_t wctype(const char *in) {
	wctype_t result[] = {
		iswalnum,iswalpha,iswblank,iswcntrl,iswdigit,iswgraph,iswlower,iswprint,iswpunct,iswspace,iswupper,iswxdigit
	};
	char *match[] = {
		"alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit"
	};
	uint64_t count = sizeof(match)/sizeof(char*);

	for(uint64_t i=0; i<count; i++) {
		if(strcmp(in, match[i]) == 0) {
			return result[i];
		}
	}

	return 0;
}

int iswalnum(wint_t wc)
{
	return iswdigit(wc) || iswalpha(wc);
}
int iswalpha(wint_t wc)
{
	if (wc<0x20000U)
		return (alpha_table[alpha_table[wc>>8]*32+((wc&255)>>3)]>>(wc&7))&1;
	if (wc<0x2fffeU)
		return 1;
	return 0;
}
int iswblank(wint_t wc) {
	wchar_t values[] = {0x9, 0x20, 0x1680, 0x180E, 0x2000, 0x2006, 0x2008, 0x200A, 0x205F, 0x3000};

	for(uint64_t i=0; i<sizeof(values)/sizeof(wchar_t); i++) {
		if(wc == values[i]) return true;
	}
	return false;
}
int iswcntrl(wint_t wc)
{
	return (unsigned)wc < 32
	    || (unsigned)(wc-0x7f) < 33
	    || (unsigned)(wc-0x2028) < 2
	    || (unsigned)(wc-0xfff9) < 3;
}
int iswdigit(wint_t wc)
{
	return (unsigned)wc-'0' < 10;
}
int iswgraph(wint_t wc)
{
	/* ISO C defines this function as: */
	return !iswspace(wc) && iswprint(wc);
}
int iswlower(wint_t wc)
{
	return towupper(wc) != wc;
}
int iswprint(wint_t wc)
{
	if (wc < 0xffU)
		return (wc+1 & 0x7f) >= 0x21;
	if (wc < 0x2028U || wc-0x202aU < 0xd800-0x202a || wc-0xe000U < 0xfff9-0xe000)
		return 1;
	if (wc-0xfffcU > 0x10ffff-0xfffc || (wc&0xfffe)==0xfffe)
		return 0;
	return 1;
}
int iswpunct(wint_t wc)
{
	if (wc<0x20000U)
		return (punct_table[punct_table[wc>>8]*32+((wc&255)>>3)]>>(wc&7))&1;
	return 0;
}
int iswupper(wint_t wc)
{
	return towlower(wc) != wc;
}
int iswxdigit(wint_t wc)
{
	return (unsigned)(wc-'0') < 10 || (unsigned)((wc|32)-'a') < 6;
}

static int casemap(unsigned c, int dir)
{
	unsigned b, x, y, v, rt, xb, xn;
	int r, rd, c0 = c;

	if (c >= 0x20000) return c;

	b = c>>8;
	c &= 255;
	x = c/3;
	y = c%3;

	/* lookup entry in two-level base-6 table */
	v = tab[tab[b]*86+x];
	static const int mt[] = { 2048, 342, 57 };
	v = (v*mt[y]>>11)%6;

	/* use the bit vector out of the tables as an index into
	 * a block-specific set of rules and decode the rule into
	 * a type and a case-mapping delta. */
	r = rules[rulebases[b]+v];
	rt = r & 255;
	rd = r >> 8;

	/* rules 0/1 are simple lower/upper case with a delta.
	 * apply according to desired mapping direction. */
	if (rt < 2) return c0 + (rd & -(rt^dir));

	/* binary search. endpoints of the binary search for
	 * this block are stored in the rule delta field. */
	xn = rd & 0xff;
	xb = (unsigned)rd >> 8;
	while (xn) {
		unsigned try = exceptions[xb+xn/2][0];
		if (try == c) {
			r = rules[exceptions[xb+xn/2][1]];
			rt = r & 255;
			rd = r >> 8;
			if (rt < 2) return c0 + (rd & -(rt^dir));
			/* Hard-coded for the four exceptional titlecase */
			return c0 + (dir ? -1 : 1);
		} else if (try > c) {
			xn /= 2;
		} else {
			xb += xn/2;
			xn -= xn/2;
		}
	}
	return c0;
}

wint_t towlower(wint_t wc)
{
	return casemap(wc, 0);
}

wint_t towupper(wint_t wc)
{
	return casemap(wc, 1);
}

//expected_length > 0, expected_remaining_bytes <= expected_length
static size_t handle_continuation_bytes(wchar_t *output, const char *input, size_t input_bytes, mbstate_t *state) {
	for(size_t i=0; i<input_bytes && state->expected_remaining_bytes > 0; i++) {
		const char next = input[i];
		//expect a continuation character which has high bits 10
		if((next & 0b11000000) != 0b10000000) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		state->buffer <<= 6;
		state->buffer |= (next & 0b00111111);
		state->expected_remaining_bytes--;
		if(state->expected_remaining_bytes == 0) {
			//all done now
			bool error = false;
			switch(state->expected_length) {
				case 1:
				error = (state->buffer > 0x7F);break;
				case 2:
				error = (state->buffer < 0x80 || state->buffer > 0x7FF);break;
				case 3:
				error = (state->buffer < 0x800 || state->buffer > 0xFFFF);break;
				case 4:
				error = (state->buffer < 0x10000 || state->buffer > 0x10FFFF);break;
				default:
				error = true;
			}
			if(error) {
				errno = EILSEQ;
				return (size_t)-1;
			}

			if(output) *output = state->buffer;
			return i+1;// +1 since i is zero based
		}
	}
	//failed to complete character
	return (size_t)-2;
} 

size_t mbrtowc(wchar_t *output, const char *input, size_t input_bytes, mbstate_t *state) {
	static mbstate_t internal;
	if(!state) state = &internal;

	if(!input) {
		if(state->expected_remaining_bytes == 0) {
			//reset
			*state = (mbstate_t){};
			return 0;
		} else {
			errno = EILSEQ;
			return (size_t)-1;
		}
	}
	if(input_bytes == 0) {
		return (size_t)-2;
	}

	//partially completed character
	if(state->expected_remaining_bytes > 0) {
		return handle_continuation_bytes(output, input, input_bytes, state);
	}

	const char next = input[0];
	if(next & 0b10000000) {
		//count leading 1s
		int count = 0;
		for(; next & (0b10000000 >> count); count++);

		if(count < 2 || count > 4) {
			//unexpected number of leading 1s
			*state = (mbstate_t){};
			errno = EILSEQ;
			return (size_t)-1;
		}

		state->expected_remaining_bytes = count-1;
		state->expected_length = count;
		state->buffer = next & ((0b10000000 >> count)-1);
		//now handle the remainder
		size_t rem = handle_continuation_bytes(output, input+1, input_bytes-1, state);
		if((long)rem > 0) rem++;// +1 since I handled input[0]
		return rem;
	} else {
		//ascii
		if(output) *output = next;
		return next != '\0';// null character returns 0
	}
}

size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps) {
	if(!dest) len = (size_t)-1;//if dest is NULL, len is ignored

	static mbstate_t internal = {};
	if(!ps) ps = &internal;//if ps is a NULL pointer, a static anonymous state only known to the mbsrtowcs() function is used instead.

	const char* curr = *src;

	for(size_t i=0; i<len; i++) {
		//parse one wchar
		size_t res = mbrtowc(dest, curr, (size_t)-1, ps);
		if(res == (size_t)-1) {
			//fail
			*ps = (mbstate_t){};
			*src = curr;
			errno = EILSEQ;
			return (size_t)-1;
		} else if(res == (size_t)-2) {
			//uh oh
			printf("PANIC in mbrstowcs\n");
			abort();
		}
		curr += res;//jump past the character
		if(dest) {
			dest++;//jump to next slot	
		}
		if(res == 0) {
			//null char found, so all done
			if(dest) *src = NULL;
			return i;// +0 since the null terminator isn't counted
		}
	}

	//len reached
	if(dest) *src = curr;
	return len;

}
