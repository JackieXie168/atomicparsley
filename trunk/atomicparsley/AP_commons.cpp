//==================================================================//
/*
    AtomicParsley - AP_commons.cpp

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

    Copyright ©2006 puck_lock
																																		*/
//==================================================================//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
		 
#include "AP_commons.h"
#include "AP_iconv.h"
#include "AtomicParsley.h"

///////////////////////////////////////////////////////////////////////////////////////
//                             File reading routines                                 //
///////////////////////////////////////////////////////////////////////////////////////

uint8_t APar_read8(FILE* m4afile, uint32_t pos) {
	uint8_t a_byte = 0;
	fseek(m4afile, pos, SEEK_SET);
	fread(&a_byte, 1, 1, m4afile);
	return a_byte;
}

uint16_t APar_read16(char* &buffer, FILE* m4afile, uint32_t pos) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 2, m4afile);
	return UInt16FromBigEndian(buffer);
}

uint32_t APar_read32(char* &buffer, FILE* m4afile, uint32_t pos) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 4, m4afile);
	return UInt32FromBigEndian(buffer);
}

void APar_readX(char* &buffer, FILE* m4afile, uint32_t pos, uint32_t length) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, length, m4afile);
	return;
}

uint32_t APar_FindValueInAtom(char* &uint32_buffer, FILE* m4afile, short an_atom, uint32_t start_position, uint32_t eval_number) {
	uint32_t current_pos = start_position;
	memset(uint32_buffer, 0, 5);
	while (current_pos <= parsedAtoms[an_atom].AtomicLength) {
		current_pos ++;
		if (eval_number > 65535) {
			//current_pos +=4;
			if (APar_read32(uint32_buffer, m4afile, parsedAtoms[an_atom].AtomicStart + current_pos) == eval_number) {
				break;
			}
		} else {
			//current_pos +=2;
			if (APar_read16(uint32_buffer, m4afile, parsedAtoms[an_atom].AtomicStart + current_pos) == (uint16_t)eval_number) {
				break;
			}
		}
		if (current_pos >= parsedAtoms[an_atom].AtomicLength) {
			current_pos =  0;
			break;
		}
	}
	return current_pos;
}

///////////////////////////////////////////////////////////////////////////////////////
//                                Language specifics                                 //
///////////////////////////////////////////////////////////////////////////////////////

void APar_UnpackLanguage(unsigned char lang_code[], uint16_t packed_language) {
	lang_code[3] = 0;
	lang_code[2] = (packed_language &0x1F) + 0x60;
	lang_code[1] = ((packed_language >> 5) &0x1F) + 0x60;
	lang_code[0] = ((packed_language >> 10) &0x1F) + 0x60;
	return;
}

uint16_t PackLanguage(const char* language_code) { //?? is there a problem here? und does't work http://www.w3.org/WAI/ER/IG/ert/iso639.htm
	//I think Apple's 3gp asses decoder is a little off. First, it doesn't support a lot of those 3 letter language codes above on that page. for example 'zul' blocks *all* metadata from showing up. 'fre' is a no-no, but 'fra' is fine.
	//then, the spec calls for all strings to be null terminated. So then why does a '© 2005' (with a NULL at the end) show up as '© 2005' in 'pol', but '© 2005 ?' in 'fas' Farsi? Must be Apple's implementation, because the files are identical except for the uint16_t lang setting.
	
	uint16_t packed_language = 0;
	
	//fprintf(stdout, "%i, %i, %i\n", language_code[5], language_code[6], language_code[7]);
	
	if (language_code[5] < 97 || language_code[5] > 122 ||
			language_code[6] < 97 || language_code[6] > 122 ||
			language_code[7] < 97 || language_code[7] > 122) {
			
		return packed_language;
	}
	
	packed_language = (((language_code[5] - 0x60) & 0x1F) << 10 ) | 
	                   ((language_code[6] - 0x60) & 0x1F) << 5  | 
										  (language_code[7] - 0x60) & 0x1F;
	return packed_language;
}


///////////////////////////////////////////////////////////////////////////////////////
//                                platform specifics                                 //
///////////////////////////////////////////////////////////////////////////////////////

#if defined (_MSC_VER)
int lroundf(float a) {
	return a/1;
}
#endif

#if ( defined (WIN32) && !defined (__CYGWIN__) && !defined (_LIBC) ) || defined (_MSC_VER)
// use glibc's strsep only on windows when cygwin & libc are undefined; otherwise the internal strsep will be used
// This marks the point where a ./configure & makefile combo would make this easier

/* Copyright (C) 1992, 93, 96, 97, 98, 99, 2004 Free Software Foundation, Inc.
   This strsep function is part of the GNU C Library - v2.3.5; LGPL.
*/

char *strsep (char **stringp, const char *delim)
{
  char *begin, *end;

  begin = *stringp;
  if (begin == NULL)
    return NULL;

  //A frequent case is when the delimiter string contains only one character.  Here we don't need to call the expensive `strpbrk' function and instead work using `strchr'.
  if (delim[0] == '\0' || delim[1] == '\0')
    {
      char ch = delim[0];

      if (ch == '\0')
	end = NULL;
      else
	{
	  if (*begin == ch)
	    end = begin;
	  else if (*begin == '\0')
	    end = NULL;
	  else
	    end = strchr (begin + 1, ch);
	}
    }
  else

    end = strpbrk (begin, delim); //Find the end of the token.

  if (end)
    {
      *end++ = '\0'; //Terminate the token and set *STRINGP past NUL character.
      *stringp = end;
    }
  else
    *stringp = NULL; //No more delimiters; this is the last token.

  return begin;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
//                                     strings                                       //
///////////////////////////////////////////////////////////////////////////////////////

wchar_t* Convert_multibyteUTF16_to_wchar(char* input_unicode, size_t glyph_length, bool skip_BOM) { //TODO: is this like wcstombs?
	int BOM_mark_bytes = 0;
	if (skip_BOM) {
		BOM_mark_bytes = 2;
	}
	
	wchar_t* utf16_data = (wchar_t*)malloc( sizeof(wchar_t)* glyph_length ); //just to be sure there will be a trailing NULL
	wmemset(utf16_data, 0, glyph_length);
						
	for(size_t i = 0; i <= glyph_length; i++) {
#if defined (__ppc__) || defined (__ppc64__)
		utf16_data[i] = (input_unicode[2*i + BOM_mark_bytes] & 0x00ff) << 8 | (input_unicode[2*i + 1 + BOM_mark_bytes]) << 0; //+2 & +3 to skip over the BOM
#else
		utf16_data[i] = (input_unicode[2*i + BOM_mark_bytes] << 8) | ((input_unicode[2*i + 1 + BOM_mark_bytes]) & 0x00ff) << 0; //+2 & +3 to skip over the BOM
#endif
	}
	return utf16_data;
}

unsigned char* Convert_multibyteUTF16_to_UTF8(char* input_utf8, size_t glyph_length, size_t byte_count) {
	unsigned char* utf8_data = (unsigned char*)malloc(sizeof(unsigned char)* glyph_length );
	memset(utf8_data, 0, glyph_length);
						
	UTF16BEToUTF8(utf8_data, glyph_length, (unsigned char*)input_utf8 + 2, byte_count);
	return utf8_data;
}

wchar_t* Convert_multibyteUTF8_to_wchar(char* input_utf8) { //TODO: is this like mbstowcs?
	size_t string_length = strlen(input_utf8) + 1;  //account for terminating NULL
	size_t char_glyphs = mbstowcs(NULL, input_utf8, string_length); //passing NULL pre-calculates the size of wchar_t needed
			
	unsigned char* utf16_conversion = (unsigned char*)malloc( sizeof(unsigned char)* string_length * 2 );
	memset(utf16_conversion, 0, string_length * 2 );
			
	int utf_16_glyphs = UTF8ToUTF16BE(utf16_conversion, char_glyphs * 2, (unsigned char*)input_utf8, string_length);
	return Convert_multibyteUTF16_to_wchar((char*)utf16_conversion, (size_t)utf_16_glyphs, false );
}

///////////////////////////////////////////////////////////////////////////////////////
//                                     generics                                      //
///////////////////////////////////////////////////////////////////////////////////////

uint16_t UInt16FromBigEndian(const char *string) {
#if defined (__ppc__) || defined (__ppc64__)
	uint16_t test;
	memcpy(&test,string,2);
	return test;
#else
	return ((string[0] & 0xff) << 8 | string[1] & 0xff) << 0;
#endif
}

uint32_t UInt32FromBigEndian(const char *string) {
#if defined (__ppc__) || defined (__ppc64__)
	uint32_t test;
	memcpy(&test,string,4);
	return test;
#else
	return ((string[0] & 0xff) << 24 | (string[1] & 0xff) << 16 | (string[2] & 0xff) << 8 | string[3] & 0xff) << 0;
#endif
}

uint64_t UInt64FromBigEndian(const char *string) {
#if defined (__ppc__) || defined (__ppc64__)
	uint64_t test;
	memcpy(&test,string,8);
	return test;
#else
	return ((string[0] & 0xff) >> 0 | (string[1] & 0xff) >> 8 | (string[2] & 0xff) >> 16 | (string[3] & 0xff) >> 24 | 
					(string[4] & 0xff) << 24 | (string[5] & 0xff) << 16 | (string[6] & 0xff) << 8 | string[7] & 0xff) << 0;
#endif
}

void char4TOuint32(uint32_t lnum, char* data) {
	data[0] = (lnum >> 24) & 0xff;
	data[1] = (lnum >> 16) & 0xff;
	data[2] = (lnum >>  8) & 0xff;
	data[3] = (lnum >>  0) & 0xff;
	return;
}

void char8TOuint64(uint64_t ullnum, char* data) {
	data[0] = (ullnum >> 56) & 0xff;
	data[1] = (ullnum >> 48) & 0xff;
	data[2] = (ullnum >> 40) & 0xff;
	data[3] = (ullnum >> 32) & 0xff;
	data[4] = (ullnum >> 24) & 0xff;
	data[5] = (ullnum >> 16) & 0xff;
	data[6] = (ullnum >>  8) & 0xff;
	data[7] = (ullnum >>  0) & 0xff;
	return;
}
