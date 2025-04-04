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

#ifndef __LIB_INFLATE_H_INCLUDED__
#define __LIB_INFLATE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#if 1
// UBLOX stuff / config
#define ASSERT(x) // dont use
#define EXECUTE(p)
#define UNUSED(v) (void)v
#define U4 unsigned int
#define U2 unsigned short
#define U1 unsigned char
#define I int
#define READ_U2(p) (*(U2*)(p))
#define READ_U4(p) (*(U4*)(p))
#define lib_crc32 lib_inflate_crc32
#endif

#define LIB_INFLATE_CRC_ENABLED
#define LIB_INFLATE_ERROR_ENABLED

/**
 * Status codes returned.
 *
 * @see lib_inflate_uncompress, lib_inflate_gzip_uncompress, lib_inflate_zlib_uncompress
 */
#if defined(LIB_INFLATE_CRC_ENABLED) || defined(LIB_INFLATE_ERROR_ENABLED)
typedef enum {
	LIB_INFLATE_SUCCESS    =  0, //*< Success
#ifdef LIB_INFLATE_CRC_ENABLED
  LIB_INFLATE_CRC_ERROR  = -3, //*< Checksum error
#endif
#ifdef LIB_INFLATE_ERROR_ENABLED
	LIB_INFLATE_DATA_ERROR = -3, //*< Input error
	LIB_INFLATE_BUF_ERROR  = -5, //*< Not enough room for output
#endif
} lib_inflate_error_code;
#else
#define lib_inflate_error_code void
#define LIB_INFLATE_SUCCESS
#endif

#ifdef LIB_INFLATE_ERROR_ENABLED
 #define lib_inflate_data_error_code  lib_inflate_error_code
 #define LIB_INFLATE_DATA_SUCCESS      LIB_INFLATE_SUCCESS
#else 
 #define lib_inflate_data_error_code void 
 #define LIB_INFLATE_DATA_SUCCESS
#endif

/**
 * Decompress `len` bytes of deflate data from `pSrc` to `pDest`.
 *
 * The variable `pLen` points to must contain the size of `pDest` on entry,
 * and will be set to the size of the decompressed data on success.
 *
 * Reads at most `len` bytes from `pSrc`.
 * Writes at most `*pLen` bytes to `pDest`.
 *
 * @param pDest pointer to where to place decompressed data
 * @param pLen pointer to variable containing size of `pDest`
 * @param pSrc pointer to compressed data
 * @param len size of compressed data
 * @return `SUCCESS` on success, error code on error
 */
lib_inflate_data_error_code lib_inflate_uncompress(
                            void *pDest, U4 *pLen,
                            const void *pSrc, U4 len);

/**
 * Decompress `len` bytes of gzip data from `pSrc` to `pDest`.
 *
 * The variable `pLen` points to must contain the size of `dest` on entry,
 * and will be set to the size of the decompressed data on success.
 *
 * Reads at most `len` bytes from `pSrc`.
 * Writes at most `*pLen` bytes to `pDest`.
 *
 * @param pDest pointer to where to place decompressed data
 * @param pLen pointer to variable containing size of `pDest`
 * @param pSrc pointer to compressed data
 * @param len size of compressed data
 * @return `SUCCESS` on success, error code on error
 */
lib_inflate_error_code lib_inflate_gzip_uncompress(
                            void *pDest, U4 *pLen,
                            const void *pSrc, U4 len);

/**
 * get the size of uncompressed gzip data from `pSrc`.
 *
 * @param pSrc pointer to compressed data
 * @param iSrc size of compressed data
 * @return the size of the uncompressed data
 */
U4 lib_inflate_gzip_size(const void *pSrc, U4 len);

/**
 * execute a gziped image
 *
 * @param pDest pointer to where to place decompressed data and execute it
 * @param pSrc pointer to compressed data
 * @param iSrc size of compressed data
 * @return the size of the uncompressed data
 */
 lib_inflate_error_code lib_inflate_gzipromExecute(void *pDest, const void *pSrc, U4 len);

extern const U4 lib_inflate_gzromSize;
extern const U1 lib_inflate_gzromFile[];

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __LIB_INFLATE_H_INCLUDED__