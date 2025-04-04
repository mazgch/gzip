


/*
 * tinf - tiny inflate library (inflate, gzip, zlib)
 *
 * Copyright (c) 2003-2019 Joergen Ibsen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must
 *      not claim that you wrote the original software. If you use this
 *      software in a product, an acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must
 *      not be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 */
 
 #include "lib_inflate.h"

typedef enum {
	FTEXT    = 1,
	FHCRC    = 2,
	FEXTRA   = 4,
	FNAME    = 8,
	FCOMMENT = 16
} tinf_gzip_flag;

// -- Iernal data structures -- 

struct lib_inflate_tree {
	U2 counts[16]; // Number of codes with a given length 
	U2 symbols[288]; // Symbols sorted by code 
	I max_sym;
};

struct lib_inflate_data {
	const U1 *source;
	const U1 *source_end;
	U4 tag;
	I bitcount;
	I overflow;

	U1 *dest_start;
	U1 *dest;
	U1 *dest_end;

	struct lib_inflate_tree ltree; // Literal/length tree 
	struct lib_inflate_tree dtree; // Distance tree 
};

#ifdef LIB_INFLATE_CRC_ENABLED
static U4 lib_inflate_crc32(const void *data, U4 length)
{
	static const U4 crc32tab[16] = {
		0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190,
		0x6B6B51F4, 0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344,
		0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278,
		0xBDBDF21C
	};
	const U1 *buf = (const U1 *) data;
	U4 crc = 0xFFFFFFFF;
	U4 i;
	if (length == 0) {
		return 0;
	}
	for (i = 0; i < length; ++i) {
		crc ^= buf[i];
		crc = crc32tab[crc & 0x0F] ^ (crc >> 4);
		crc = crc32tab[crc & 0x0F] ^ (crc >> 4);
	}
	return crc ^ 0xFFFFFFFF;
}
#endif

// Build fixed Huffman trees 
static void lib_inflate_build_fixed_trees(struct lib_inflate_tree *lt, struct lib_inflate_tree *dt)
{
	I i;

	// Build fixed literal/length tree 
	for (i = 0; i < 16; ++i) {
		lt->counts[i] = 0;
	}

	lt->counts[7] = 24;
	lt->counts[8] = 152;
	lt->counts[9] = 112;

	for (i = 0; i < 24; ++i) {
		lt->symbols[i] = 256 + i;
	}
	for (i = 0; i < 144; ++i) {
		lt->symbols[24 + i] = i;
	}
	for (i = 0; i < 8; ++i) {
		lt->symbols[24 + 144 + i] = 280 + i;
	}
	for (i = 0; i < 112; ++i) {
		lt->symbols[24 + 144 + 8 + i] = 144 + i;
	}

	lt->max_sym = 285;

	// Build fixed distance tree 
	for (i = 0; i < 16; ++i) {
		dt->counts[i] = 0;
	}

	dt->counts[5] = 32;

	for (i = 0; i < 32; ++i) {
		dt->symbols[i] = i;
	}

	dt->max_sym = 29;
}

// Given an array of code lengths, build a tree 
static lib_inflate_data_error_code lib_inflate_build_tree(
													struct lib_inflate_tree *t, const U1 *lengths,
                          U4 num)
{
	U2 offs[16];
	U4 i, num_codes, available;

	ASSERT(num <= 288);

	for (i = 0; i < 16; ++i) {
		t->counts[i] = 0;
	}

	t->max_sym = -1;

	// Count number of codes for each non-zero length 
	for (i = 0; i < num; ++i) {
		ASSERT(lengths[i] <= 15);

		if (lengths[i]) {
			t->max_sym = i;
			t->counts[lengths[i]]++;
		}
	}

	// Compute offset table for distribution sort 
	for (available = 1, num_codes = 0, i = 0; i < 16; ++i) {
		U4 used = t->counts[i];

		// Check length contains no more codes than available 
#ifdef LIB_INFLATE_ERROR_ENABLED
		if (used > available) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif
		available = 2 * (available - used);

		offs[i] = num_codes;
		num_codes += used;
	}

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Check all codes were used, or for the special case of only one
	// code that it has length 1
	if ((num_codes > 1 && available > 0)
	 || (num_codes == 1 && t->counts[1] != 1)) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif

	// Fill in symbols sorted by code 
	for (i = 0; i < num; ++i) {
		if (lengths[i]) {
			t->symbols[offs[lengths[i]]++] = i;
		}
	}

	// For the special case of only one code (which will be 0) add a
	// code 1 which results in a symbol that is too large
	 
	if (num_codes == 1) {
		t->counts[1] = 2;
		t->symbols[1] = t->max_sym + 1;
	}

	return LIB_INFLATE_DATA_SUCCESS;
}

// -- Decode functions -- 

static void lib_inflate_refill(struct lib_inflate_data *d, I num)
{
	ASSERT(num >= 0 && num <= 32);

	// Read bytes until at least num bits available 
	while (d->bitcount < num) {
		if (d->source != d->source_end) {
			d->tag |= (U4) *d->source++ << d->bitcount;
		}
		else {
			d->overflow = 1;
		}
		d->bitcount += 8;
	}

	ASSERT(d->bitcount <= 32);
}

static U4 lib_inflate_getbits_no_refill(struct lib_inflate_data *d, I num)
{
	U4 bits;

	ASSERT(num >= 0 && num <= d->bitcount);

	// Get bits from tag 
	bits = d->tag & ((1UL << num) - 1);

	// Remove bits from tag 
	d->tag >>= num;
	d->bitcount -= num;

	return bits;
}

// Get num bits from source stream 
static U4 lib_inflate_getbits(struct lib_inflate_data *d, I num)
{
	lib_inflate_refill(d, num);
	return lib_inflate_getbits_no_refill(d, num);
}

// Read a num bit value from stream and add base 
static U4 lib_inflate_getbits_base(struct lib_inflate_data *d, I num, I base)
{
	return base + (num ? lib_inflate_getbits(d, num) : 0);
}

// Given a data stream and a tree, decode a symbol 
static I lib_inflate_decode_symbol(struct lib_inflate_data *d, const struct lib_inflate_tree *t)
{
	I base = 0, offs = 0;
	I len;

	/*
	 * Get more bits while code index is above number of codes
	 *
	 * Rather than the actual code, we are computing the position of the
	 * code in the sorted order of codes, which is the index of the
	 * corresponding symbol.
	 *
	 * Conceptually, for each code length (level in the tree), there are
	 * counts[len] leaves on the left and Iernal nodes on the right.
	 * The index we have decoded so far is base + offs, and if that
	 * falls within the leaves we are done. Otherwise we adjust the range
	 * of offs and add one more bit to it.
	 */
	for (len = 1; ; ++len) {
		offs = 2 * offs + lib_inflate_getbits(d, 1);

		ASSERT(len <= 15);

		if (offs < t->counts[len]) {
			break;
		}

		base += t->counts[len];
		offs -= t->counts[len];
	}

	ASSERT(base + offs >= 0 && base + offs < 288);

	return t->symbols[base + offs];
}

// Given a data stream, decode dynamic trees from it 
static lib_inflate_data_error_code lib_inflate_decode_trees(
														struct lib_inflate_data *d, 
														struct lib_inflate_tree *lt,
                            struct lib_inflate_tree *dt)
{
	U1 lengths[288 + 32];

	// Special ordering of code length codes 
	static const U1 clcidx[19] = {
		16, 17, 18, 0,  8, 7,  9, 6, 10, 5,
		11,  4, 12, 3, 13, 2, 14, 1, 15
	};

	U4 hlit, hdist, hclen;
	U4 i, num, length;
#ifdef LIB_INFLATE_ERROR_ENABLED
	lib_inflate_data_error_code res;
#endif
	// Get 5 bits HLIT (257-286) 
	hlit = lib_inflate_getbits_base(d, 5, 257);

	// Get 5 bits HDIST (1-32) 
	hdist = lib_inflate_getbits_base(d, 5, 1);

	// Get 4 bits HCLEN (4-19) 
	hclen = lib_inflate_getbits_base(d, 4, 4);

#ifdef LIB_INFLATE_ERROR_ENABLED
	/*
	 * The RFC limits the range of HLIT to 286, but lists HDIST as range
	 * 1-32, even though distance codes 30 and 31 have no meaning. While
	 * we could allow the full range of HLIT and HDIST to make it possible
	 * to decode the fixed trees with this function, we consider it an
	 * error here.
	 *
	 * See also: https://github.com/madler/zlib/issues/82
	 */
	if (hlit > 286 || hdist > 30) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif

	for (i = 0; i < 19; ++i) {
		lengths[i] = 0;
	}

	// Read code lengths for code length alphabet 
	for (i = 0; i < hclen; ++i) {
		// Get 3 bits code length (0-7) 
		U4 clen = lib_inflate_getbits(d, 3);

		lengths[clcidx[i]] = clen;
	}

	// Build code length tree (in literal/length tree to save space) 
#ifdef LIB_INFLATE_ERROR_ENABLED
	res = 
#endif
	lib_inflate_build_tree(lt, lengths, 19);

#ifdef LIB_INFLATE_ERROR_ENABLED
	if (res != LIB_INFLATE_DATA_SUCCESS) {
		return res;
	}

	// Check code length tree is not empty 
	if (lt->max_sym == -1) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif

	// Decode code lengths for the dynamic trees 
	for (num = 0; num < hlit + hdist; ) {
		I sym = lib_inflate_decode_symbol(d, lt);

#ifdef LIB_INFLATE_ERROR_ENABLED
		if (sym > lt->max_sym) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif

		switch (sym) {
		case 16:
			// Copy previous code length 3-6 times (read 2 bits) 
#ifdef LIB_INFLATE_ERROR_ENABLED
			if (num == 0) {
				return LIB_INFLATE_DATA_ERROR;
			}
#endif
			sym = lengths[num - 1];
			length = lib_inflate_getbits_base(d, 2, 3);
			break;
		case 17:
			// Repeat code length 0 for 3-10 times (read 3 bits) 
			sym = 0;
			length = lib_inflate_getbits_base(d, 3, 3);
			break;
		case 18:
			// Repeat code length 0 for 11-138 times (read 7 bits) 
			sym = 0;
			length = lib_inflate_getbits_base(d, 7, 11);
			break;
		default:
			// Values 0-15 represent the actual code lengths 
			length = 1;
			break;
		}

#ifdef LIB_INFLATE_ERROR_ENABLED
		if (length > hlit + hdist - num) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif

		while (length--) {
			lengths[num++] = sym;
		}
	}

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Check EOB symbol is present 
	if (lengths[256] == 0) {
		return LIB_INFLATE_DATA_ERROR;
	}

	// Build dynamic trees 
	res = 
#endif
	lib_inflate_build_tree(lt, lengths, hlit);
#ifdef LIB_INFLATE_ERROR_ENABLED
	if (res != LIB_INFLATE_SUCCESS) {
		return res;
	}

	res = 
#endif
	lib_inflate_build_tree(dt, lengths + hlit, hdist);

#ifdef LIB_INFLATE_ERROR_ENABLED
	if (res != LIB_INFLATE_DATA_SUCCESS) {
		return res;
	}

	return LIB_INFLATE_DATA_SUCCESS;
#endif
}

// -- Block inflate functions -- 

// Given a stream and two trees, inflate a block of data 
static lib_inflate_data_error_code lib_inflate_inflate_block_data(
																	struct lib_inflate_data *d, 
																	struct lib_inflate_tree *lt,
                                  struct lib_inflate_tree *dt)
{
	// Extra bits and base tables for length codes 
	static const U1 length_bits[30] = {
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
		1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
		4, 4, 4, 4, 5, 5, 5, 5, 0, 127
	};

	static const U2 length_base[30] = {
		 3,  4,  5,   6,   7,   8,   9,  10,  11,  13,
		15, 17, 19,  23,  27,  31,  35,  43,  51,  59,
		67, 83, 99, 115, 131, 163, 195, 227, 258,   0
	};

	// Extra bits and base tables for distance codes 
	static const U1 dist_bits[30] = {
		0, 0,  0,  0,  1,  1,  2,  2,  3,  3,
		4, 4,  5,  5,  6,  6,  7,  7,  8,  8,
		9, 9, 10, 10, 11, 11, 12, 12, 13, 13
	};

	static const U2 dist_base[30] = {
		   1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
		  33,   49,   65,   97,  129,  193,  257,   385,   513,   769,
		1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
	};

	for (;;) {
		I sym = lib_inflate_decode_symbol(d, lt);

#ifdef LIB_INFLATE_ERROR_ENABLED
		// Check for overflow in bit reader 
		if (d->overflow) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif
		if (sym < 256) {
#ifdef LIB_INFLATE_ERROR_ENABLED
			if (d->dest == d->dest_end) {
				return LIB_INFLATE_BUF_ERROR;
			}
#endif
			*d->dest++ = sym;
		}
		else {
			I length, dist, offs;
			I i;

			// Check for end of block 
			if (sym == 256) {
				return LIB_INFLATE_DATA_SUCCESS;
			}

#ifdef LIB_INFLATE_ERROR_ENABLED
			// Check sym is within range and distance tree is not empty 
			if (sym > lt->max_sym || sym - 257 > 28 || dt->max_sym == -1) {
				return LIB_INFLATE_DATA_ERROR;
			}
#endif
			sym -= 257;

			// Possibly get more bits from length code 
			length = lib_inflate_getbits_base(d, length_bits[sym],
			                           length_base[sym]);

			dist = lib_inflate_decode_symbol(d, dt);

#ifdef LIB_INFLATE_ERROR_ENABLED
			// Check dist is within range 
			if (dist > dt->max_sym || dist > 29) {
				return LIB_INFLATE_DATA_ERROR;
			}
#endif
			// Possibly get more bits from distance code 
			offs = lib_inflate_getbits_base(d, dist_bits[dist],
			                         dist_base[dist]);

#ifdef LIB_INFLATE_ERROR_ENABLED
			if (offs > d->dest - d->dest_start) {
				return LIB_INFLATE_DATA_ERROR;
			}

			if (d->dest_end - d->dest < length) {
				return LIB_INFLATE_BUF_ERROR;
			}
#endif
			// Copy match 
			for (i = 0; i < length; ++i) {
				d->dest[i] = d->dest[i - offs];
			}

			d->dest += length;
		}
	}
}

// Inflate an uncompressed block of data 
static lib_inflate_data_error_code lib_inflate_inflate_uncompressed_block(struct lib_inflate_data *d)
{
	U4 length;
#ifdef LIB_INFLATE_ERROR_ENABLED
	U4 invlength;

	if (d->source_end - d->source < 4) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif
	// Get length 
	length = READ_U2(d->source);

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Get one's complement of length 
	invlength = READ_U2(d->source + 2);

	// Check length 
	if (length != (~invlength & 0x0000FFFF)) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif
	d->source += 4;

#ifdef LIB_INFLATE_ERROR_ENABLED
	if (d->source_end - d->source < length) {
		return LIB_INFLATE_DATA_ERROR;
	}

	if (d->dest_end - d->dest < length) {
		return LIB_INFLATE_BUF_ERROR;
	}
#endif
	// Copy block 
	while (length--) {
		*d->dest++ = *d->source++;
	}

	// Make sure we start next block on a byte boundary 
	d->tag = 0;
	d->bitcount = 0;

	return LIB_INFLATE_DATA_SUCCESS;
}

// Inflate a block of data compressed with fixed Huffman trees 
static lib_inflate_data_error_code lib_inflate_inflate_fixed_block(struct lib_inflate_data *d)
{
	// Build fixed Huffman trees 
	lib_inflate_build_fixed_trees(&d->ltree, &d->dtree);

	// Decode block using fixed trees 
	return lib_inflate_inflate_block_data(d, &d->ltree, &d->dtree);
}

// Inflate a block of data compressed with dynamic Huffman trees 
static lib_inflate_data_error_code lib_inflate_inflate_dynamic_block(struct lib_inflate_data *d)
{
	// Decode trees from stream 
#ifdef LIB_INFLATE_ERROR_ENABLED
	lib_inflate_data_error_code res = 
#endif
	lib_inflate_decode_trees(d, &d->ltree, &d->dtree);
#ifdef LIB_INFLATE_ERROR_ENABLED
	if (res != LIB_INFLATE_SUCCESS) {
		return res;
	}
#endif
	// Decode block using decoded trees 
	return lib_inflate_inflate_block_data(d, &d->ltree, &d->dtree);
}

// EXTERNAL Library API 

// Inflate stream from source to dest 
lib_inflate_data_error_code lib_inflate_uncompress(
										void *pDest, U4 *pLen,
                    const void *pSrc, U4 len)
{
	struct lib_inflate_data d;
	I bfinal;

	// Initialise data 
	d.source = (const U1 *) pSrc;
	d.source_end = d.source + len;
	d.tag = 0;
	d.bitcount = 0;
	d.overflow = 0;

	d.dest = (U1*) pDest;
	d.dest_start = d.dest;
	d.dest_end = d.dest + *pLen;

	do {
		U4 btype;
#ifdef LIB_INFLATE_ERROR_ENABLED
		lib_inflate_data_error_code res;
#endif

		// Read final block flag 
		bfinal = lib_inflate_getbits(&d, 1);

		// Read block type (2 bits) 
		btype = lib_inflate_getbits(&d, 2);

		// Decompress block 
		switch (btype) {
		case 0:
			// Decompress uncompressed block 
#ifdef LIB_INFLATE_ERROR_ENABLED
			res = 
#endif
			lib_inflate_inflate_uncompressed_block(&d);
			break;
		case 1:
			// Decompress block with fixed Huffman trees 
#ifdef LIB_INFLATE_ERROR_ENABLED
			res = 
#endif
			lib_inflate_inflate_fixed_block(&d);
			break;
		case 2:
			// Decompress block with dynamic Huffman trees 
#ifdef LIB_INFLATE_ERROR_ENABLED
			res = 
#endif
			lib_inflate_inflate_dynamic_block(&d);
			break;
#ifdef LIB_INFLATE_ERROR_ENABLED
		default:
			res = LIB_INFLATE_DATA_ERROR;
			break;
#endif
		}
#ifdef LIB_INFLATE_ERROR_ENABLED
		if (res != LIB_INFLATE_SUCCESS) {
			return res;
		}
#endif
	} while (!bfinal);

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Check for overflow in bit reader 
	if (d.overflow) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif
	*pLen = d.dest - d.dest_start;
	return LIB_INFLATE_DATA_SUCCESS;
}

lib_inflate_error_code lib_inflate_gzip_uncompress(
												void *pDest, U4 *pLen,
                        const void *pSrc, U4 len)
{
	const U1 *src = (const U1 *) pSrc;
	U1 *dst = (U1 *) pDest;
	const U1 *start;
#if defined(LIB_INFLATE_ERROR_ENABLED) || defined(LIB_INFLATE_CRC_ENABLED)
	U4 dlen;
#endif
#ifdef LIB_INFLATE_CRC_ENABLED
	U4 crc32;
#endif
#ifdef LIB_INFLATE_ERROR_ENABLED
	lib_inflate_error_code res;
#endif
	tinf_gzip_flag flg;

	// -- Check header -- 

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Check room for at least 10 byte header and 8 byte trailer 
	if (len < 18) {
		return LIB_INFLATE_DATA_ERROR;
	}

	// Check id bytes 
	if (src[0] != 0x1F || src[1] != 0x8B) {
		return LIB_INFLATE_DATA_ERROR;
	}

	// Check method is deflate 
	if (src[2] != 8) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif

	// Get flag byte 
	flg = src[3];

#ifdef LIB_INFLATE_ERROR_ENABLED
	// Check that reserved bits are zero 
	if (flg & 0xE0) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif

	// -- Find start of compressed data -- 

	// Skip base header of 10 bytes 
	start = src + 10;
	// Skip extra data if present 
	if (flg & FEXTRA) {
		U4 xlen = READ_U2(start);
#ifdef LIB_INFLATE_ERROR_ENABLED
		if (xlen > len - 12) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif
		start += xlen + 2;
	}

	// Skip file name if present 
	if (flg & FNAME) {
		do {
#ifdef LIB_INFLATE_ERROR_ENABLED
			if (start - src >= len) {
				return LIB_INFLATE_DATA_ERROR;
			}
#endif
		} while (*start++);
	}

	// Skip file comment if present 
	if (flg & FCOMMENT) {
		do {
#ifdef LIB_INFLATE_ERROR_ENABLED
			if (start - src >= len) {
				return LIB_INFLATE_DATA_ERROR;
			}
#endif
		} while (*start++);
	}

	// Check header crc if present 
	if (flg & FHCRC) {
#ifdef LIB_INFLATE_CRC_ENABLED
		U4 hcrc;
#endif
#ifdef LIB_INFLATE_ERROR_ENABLED
		if (start - src > len - 2) {
			return LIB_INFLATE_DATA_ERROR;
		}
#endif
#ifdef LIB_INFLATE_CRC_ENABLED
		hcrc = READ_U2(start);
		if (hcrc != (lib_crc32(src, start - src) & 0x0000FFFF)) {
			return LIB_INFLATE_CRC_ERROR;
		}
#endif
		start += 2;
	}

	// -- Get decompressed length -- 
#if defined(LIB_INFLATE_ERROR_ENABLED) || defined(LIB_INFLATE_CRC_ENABLED)
	dlen = READ_U4(&src[len - 4]);
#endif
#ifdef LIB_INFLATE_ERROR_ENABLED
	if (dlen > *pLen) {
		return LIB_INFLATE_BUF_ERROR;
	}
#endif
#ifdef LIB_INFLATE_CRC_ENABLED
	// -- Get CRC32 checksum of original data -- 
	crc32 = READ_U4(&src[len - 8]);
#endif

#ifdef LIB_INFLATE_ERROR_ENABLED
	// -- Decompress data -- 
	if ((src + len) - start < 8) {
		return LIB_INFLATE_DATA_ERROR;
	}
	res = 
#endif
	lib_inflate_uncompress(dst, pLen, start,
	                      (src + len) - start - 8);
#ifdef LIB_INFLATE_ERROR_ENABLED
	if (res != LIB_INFLATE_DATA_SUCCESS) {
		return LIB_INFLATE_DATA_ERROR;
	}
	if (*pLen != dlen) {
		return LIB_INFLATE_DATA_ERROR;
	}
#endif
#ifdef LIB_INFLATE_CRC_ENABLED
	// -- Check CRC32 checksum -- 
	if (crc32 != lib_crc32(dst, dlen)) {
		return LIB_INFLATE_CRC_ERROR;
	}
#endif

	return LIB_INFLATE_SUCCESS;
}

U4 lib_inflate_gzip_size(const void *pSrc, U4 len) {
	const U1 *src = (const U1 *) pSrc;
	return READ_U4(&src[len - 4]);
}

lib_inflate_error_code lib_inflate_gzipromExecute(void *pDest, const void *pSrc, U4 len) {
  const U4 destSize = lib_inflate_gzip_size(pSrc, len);
  U4 decSize = destSize;
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
  lib_inflate_error_code res =
#endif
  lib_inflate_gzip_uncompress(pDest, &decSize, pSrc, len);
  if (
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
    (res == LIB_INFLATE_SUCCESS) &&
#endif
    (decSize == destSize)) {
    EXECUTE(pDest);
  }
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
  return res;
#endif
}