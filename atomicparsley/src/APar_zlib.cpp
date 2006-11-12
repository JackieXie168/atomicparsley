//==================================================================//
/*
    AtomicParsley - APar_zlib.cpp

    AtomicParsley is GPL software; you can freely distribute, 
    redistribute, modify & use under the terms of the GNU General
    Public License; either version 2 or its successor.

    AtomicParsley is distributed under the GPL "AS IS", without
    any warranty; without the implied warranty of merchantability
    or fitness for either an expressed or implied particular purpose.

    Please see the included GNU General Public License (GPL) for 
    your rights and further details; see the file COPYING. If you
    cannot, write to the Free Software Foundation, 59 Temple Place
    Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

    Copyright �2006 puck_lock
                                                                   */
//==================================================================//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "AP_commons.h"

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#if defined HAVE_ZLIB_H

#if defined (WIN32) || defined (__CYGWIN__)
#include "zlib.h"   //QT 
#else
#include <zlib.h>   //QT 
#endif

#if defined (WIN32)
#include <windows.h>
static bool zlibdll_loaded = 0;
static HINSTANCE zlib_library;

//http://www.winradio.com/home/g303_sdk-exa-c.htm
typedef int (__stdcall *inflate)(z_streamp strm, int flush);
typedef int (__stdcall *inflateInit)(z_streamp strm, int level);
typedef int (__stdcall *inflateEnd)(z_streamp strm);
typedef int (__stdcall *deflate)(z_streamp strm, int flush);
typedef int (__stdcall *deflateInit)(z_streamp strm, int level);
typedef int (__stdcall *deflateEnd)(z_streamp strm);

// or 
static int (*inflate)(z_streamp strm, int flush);
static int (*inflateInit)(z_streamp strm, int level);
static int (*inflateEnd)(z_streamp strm);
static int (*deflate)(z_streamp strm, int flush);
static int (*deflateInit)(z_streamp strm, int level);
static int (*deflateEnd)(z_streamp strm);

bool APar_win32_zlib_LoadLibrary() {
	HINSTANCE zlib_lib;
	if (zlibdll_loaded) return true;
	
	zlib_lib = LoadLibrary("zlib1.dll");
	if (zlib_lib == NULL){
		fprintf(stdout,"AtomicParsley warning: zlib library missing. Compression is disabled.\n");
		return false;
	}
	
	inflateInit=GetProcAddress(zlib_lib,"inflateInit");
	inflate=GetProcAddress(zlib_lib,"inflate");
	inflateEnd=GetProcAddress(zlib_lib,"inflateEnd");
	deflateInit=GetProcAddress(zlib_lib,"deflateInit");
	deflate=GetProcAddress(zlib_lib,"deflate");
	deflateEnd=GetProcAddress(zlib_lib,"deflateEnd");
	
	//or
	(int (*)(z_streamp strm, int level))inflateInit=GetProcAddress(zlib_lib,"inflateInit");
	(int (*)(z_streamp strm, int flush))inflate=GetProcAddress(zlib_lib,"inflate");
	(int (*)(z_streamp strm))inflateEnd=GetProcAddress(zlib_lib,"inflateEnd");
	(int (*)(z_streamp strm, int level))deflateInit=GetProcAddress(zlib_lib,"deflateInit");
	(int (*)(z_streamp strm, int flush))deflate=GetProcAddress(zlib_lib,"deflate");
	(int (*)(z_streamp strm))deflateEnd=GetProcAddress(zlib_lib,"deflateEnd");
	
	if (inflateInit && inflate && inflateEnd && deflateInit && deflate && deflateEnd)
		zlibdll_loaded = true;
		zlib_library = zlib_lib;
	} else {
		fprintf(stdout,"AtomicParsley warning: zlib library addressing failed. Compression is disabled.\n");
		return false;
	}
	return true;
}

void APar_win32_zlib_FreeLibrary() {
   if (!zlibdll_loaded) return;
   FreeLibrary(zlib_library);
   zlibdll_loaded = false;
}

#endif

#endif //HAVE_ZLIB_H

static void* zalloc(void *opaque, unsigned int items, unsigned int size) {
	return calloc(items, size);
}

static void zfree(void *opaque, void *ptr) {
	free(ptr);
}

// char* out_buffer = (char*)calloc(1, expanded_size+1);
/*----------------------
APar_zlib_inflate
  in_buffer - pointer to already compressed data
	in_buf_len - length of compressed data
	out_buffer - pointer to a buffer to store decompressed/inflated data
	out_buf_len - length of the out_buffer/max allowable decompressed size

    fill
----------------------*/
void APar_zlib_inflate(char* in_buffer, uint32_t in_buf_len, char* out_buffer, uint32_t out_buf_len) {
#if defined HAVE_ZLIB_H
	z_stream zlib;

	// Decompress to another buffer
  zlib.zalloc = zalloc;
  zlib.zfree = zfree;
	zlib.opaque = NULL;
	zlib.avail_out = out_buf_len +1;
	zlib.next_out = (unsigned char*)out_buffer;
	zlib.avail_in = in_buf_len;
	zlib.next_in = (unsigned char*)in_buffer;
	inflateInit(&zlib);
	inflate(&zlib, Z_PARTIAL_FLUSH);
	inflateEnd(&zlib);
#endif
	return ;
}

uint32_t APar_zlib_deflate(char* in_buffer, uint32_t in_buf_len, char* out_buffer, uint32_t out_buf_len) {
	uint32_t compressed_bytes = 0;
	
#if defined HAVE_ZLIB_H
	z_stream zlib;

	// Compress(default level 6) to another buffer
  zlib.zalloc = zalloc;
  zlib.zfree = zfree;
	zlib.opaque = NULL;
	zlib.avail_out = out_buf_len +1;
	zlib.next_out = (unsigned char*)out_buffer;
	zlib.avail_in = in_buf_len;
	zlib.next_in = (unsigned char*)in_buffer;
	zlib.total_out = 0;
	deflateInit(&zlib, Z_DEFAULT_COMPRESSION);
	if (Z_STREAM_END == deflate(&zlib, Z_FINISH) ) {
		compressed_bytes = (uint32_t)zlib.total_out;
		deflateEnd(&zlib);
	}
#endif
	return compressed_bytes;
}
