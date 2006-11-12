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
		
		----------------------
    Code Contributions by:
		
    * SLarew - prevent writing past array in Convert_multibyteUTF16_to_wchar bugfix
		
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



#if defined (_MSC_VER)

///////////////////////////////////////////////////////////////////////////////////////
//                 Win32 function to support 2-4GB large files                       //
///////////////////////////////////////////////////////////////////////////////////////

int fseeko(FILE *stream, uint64_t pos, int whence) { //only using SEEK_SET here
	if (whence == SEEK_SET) {
		fpos_t fpos = pos;
		return fsetpos(stream, &fpos);
	} else {
		return -1;
	}
	return -1;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
//                             File reading routines                                 //
///////////////////////////////////////////////////////////////////////////////////////

uint8_t APar_read8(FILE* m4afile, uint32_t pos) {
	uint8_t a_byte = 0;
	fseeko(m4afile, pos, SEEK_SET);
	fread(&a_byte, 1, 1, m4afile);
	return a_byte;
}

uint16_t APar_read16(char* buffer, FILE* m4afile, uint32_t pos) {
	fseeko(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 2, m4afile);
	return UInt16FromBigEndian(buffer);
}

uint32_t APar_read32(char* buffer, FILE* m4afile, uint32_t pos) {
	fseeko(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 4, m4afile);
	return UInt32FromBigEndian(buffer);
}

void APar_readX(char* buffer, FILE* m4afile, uint32_t pos, uint32_t length) {
	fseeko(m4afile, pos, SEEK_SET);
	fread(buffer, 1, length, m4afile);
	return;
}

uint32_t APar_FindValueInAtom(char* uint32_buffer, FILE* m4afile, short an_atom, uint32_t start_position, uint32_t eval_number) {
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

uint16_t PackLanguage(const char* language_code, uint8_t lang_offset) { //?? is there a problem here? und does't work http://www.w3.org/WAI/ER/IG/ert/iso639.htm
	//I think Apple's 3gp asses decoder is a little off. First, it doesn't support a lot of those 3 letter language codes above on that page. for example 'zul' blocks *all* metadata from showing up. 'fre' is a no-no, but 'fra' is fine.
	//then, the spec calls for all strings to be null terminated. So then why does a '© 2005' (with a NULL at the end) show up as '© 2005' in 'pol', but '© 2005 ?' in 'fas' Farsi? Must be Apple's implementation, because the files are identical except for the uint16_t lang setting.
	
	uint16_t packed_language = 0;
	
	//fprintf(stdout, "%i, %i, %i\n", language_code[0+lang_offset], language_code[1+lang_offset], language_code[2+lang_offset]);
	
	if (language_code[0+lang_offset] < 97 || language_code[0+lang_offset] > 122 ||
			language_code[1+lang_offset] < 97 || language_code[1+lang_offset] > 122 ||
			language_code[2+lang_offset] < 97 || language_code[2+lang_offset] > 122) {
			
		return packed_language;
	}
	
	packed_language = (((language_code[0+lang_offset] - 0x60) & 0x1F) << 10 ) | 
	                   ((language_code[1+lang_offset] - 0x60) & 0x1F) << 5  | 
										  (language_code[2+lang_offset] - 0x60) & 0x1F;
	return packed_language;
}

///////////////////////////////////////////////////////////////////////////////////////
//                                platform specifics                                 //
///////////////////////////////////////////////////////////////////////////////////////

#if (!defined HAVE_LROUNDF) && (!defined (__GLIBC__))
int lroundf(float a) {
	return a/1;
}
#endif

#ifndef HAVE_STRSEP
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
						
	for(size_t i = 0; i < glyph_length; i++) {
#if defined (__ppc__) || defined (__ppc64__)
		utf16_data[i] = (input_unicode[2*i + BOM_mark_bytes] & 0x00ff) << 8 | (input_unicode[2*i + 1 + BOM_mark_bytes]) << 0; //+2 & +3 to skip over the BOM
#else
		utf16_data[i] = (input_unicode[2*i + BOM_mark_bytes] << 8) | ((input_unicode[2*i + 1 + BOM_mark_bytes]) & 0x00ff) << 0; //+2 & +3 to skip over the BOM
#endif
	}
	return utf16_data;
}

unsigned char* Convert_multibyteUTF16_to_UTF8(char* input_utf16, size_t glyph_length, size_t byte_count) {
	unsigned char* utf8_data = (unsigned char*)malloc(sizeof(unsigned char)* glyph_length );
	memset(utf8_data, 0, glyph_length);
						
	UTF16BEToUTF8(utf8_data, glyph_length, (unsigned char*)input_utf16 + 2, byte_count);
	return utf8_data;
}

wchar_t* Convert_multibyteUTF8_to_wchar(const char* input_utf8) { //TODO: is this like mbstowcs?
	size_t string_length = strlen(input_utf8) + 1;  //account for terminating NULL
	size_t char_glyphs = mbstowcs(NULL, input_utf8, string_length); //passing NULL pre-calculates the size of wchar_t needed
			
	unsigned char* utf16_conversion = (unsigned char*)malloc( sizeof(unsigned char)* string_length * 2 );
	memset(utf16_conversion, 0, string_length * 2 );
			
	int utf_16_glyphs = UTF8ToUTF16BE(utf16_conversion, char_glyphs * 2, (unsigned char*)input_utf8, string_length);
	return Convert_multibyteUTF16_to_wchar((char*)utf16_conversion, (size_t)utf_16_glyphs, false );
}

//these flags from id3v2 2.4
//0x00 = ISO-8859-1 & terminate with 0x00.
//0x01 = UTF-16 with BOM. All frames have same encoding & terminate with 0x0000. 
//0x02 = UTF-16BE without BOM & terminate with 0x0000.
//0x03 = UTF-8 & terminated with 0x00.
//buffer can hold either ut8 or utf16 carried on 8-bit char which requires a cast	
/*----------------------
findstringNULLterm
  in_string - pointer to location of a string (can be either 8859-1, utf8 or utf16be/utf16be needing a cast to wchar)
	encodingFlag - used to denote the encoding of instring (derived from id3v2 2.4 encoding flags)
	max_len - the length of given string - there may be no NULL terminaiton, in which case it will only count to max_len

    Either find the NULL if it exists and return how many bytes into in_string that NULL exists, or it won't find a NULL and return max_len
----------------------*/
uint32_t findstringNULLterm (char* in_string, uint8_t encodingFlag, uint32_t max_len) {
	uint32_t byte_count = 0;
	
	if (encodingFlag == 0x00 || encodingFlag == 0x03) {
		char* bufptr = in_string;
		while (bufptr <= in_string+max_len) {
			if (*bufptr == 0x00) {
				break;
			}
			bufptr++;
			byte_count++;
		}		
	} else if ((encodingFlag == 0x01 || encodingFlag == 0x02) && max_len >= 2) {
		short wbufptr;
		while (byte_count <= max_len) {
			wbufptr = (*(in_string+byte_count) << 8) | *(in_string+byte_count+1);
			if (wbufptr == 0x0000) {
				break;
			}
			byte_count+=2;
		}	
	}
	if (byte_count > max_len) return max_len;
	return byte_count;
}

uint32_t skipNULLterm (char* in_string, uint8_t encodingFlag, uint32_t max_len) {
	uint32_t byte_count = 0;
	
	if (encodingFlag == 0x00 || encodingFlag == 0x03) {
		char* bufptr = in_string;
		while (bufptr <= in_string+max_len) {
			if (*bufptr == 0x00) {
				byte_count++;
				break;
			}
			bufptr++;
		}		
	} else if ((encodingFlag == 0x01 || encodingFlag == 0x02) && max_len >= 2) {
		short wbufptr;
		while (byte_count <= max_len) {
			wbufptr = (*(in_string+byte_count) << 8) | *(in_string+byte_count+1);
			if (wbufptr == 0x0000) {
				byte_count+=2;
				break;
			}
		}	
	}
	return byte_count;
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

void UInt16_TO_String2(uint16_t snum, char* data) {
	data[0] = (snum >>  8) & 0xff;
	data[1] = (snum >>  0) & 0xff;
	return;
}

void UInt32_TO_String4(uint32_t lnum, char* data) {
	data[0] = (lnum >> 24) & 0xff;
	data[1] = (lnum >> 16) & 0xff;
	data[2] = (lnum >>  8) & 0xff;
	data[3] = (lnum >>  0) & 0xff;
	return;
}

void UInt64_TO_String8(uint64_t ullnum, char* data) {
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

///////////////////////////////////////////////////////////////////////////////////////
//                        3gp asset support (for 'loci')                             //
///////////////////////////////////////////////////////////////////////////////////////

uint32_t float_to_16x16bit_fixed_point(double floating_val) {
	uint32_t fixedpoint_16bit = 0;
	int16_t long_integer = (int16_t)floating_val;
	//to get a fixed 16-bit decimal, work on the decimal part along; multiply by (2^8 * 2) which moves the decimal over 16 bits to create our int16_t
	//now while the degrees can be negative (requiring a int16_6), the decimal portion is always positive (and thus requiring a uint16_t)
	uint16_t long_decimal = (int16_t) ((floating_val - long_integer) * (double)(65536) ); 
	fixedpoint_16bit = long_integer * 65536 + long_decimal; //same as bitshifting, less headache doing it
	return fixedpoint_16bit;
}

double fixed_point_16x16bit_to_double(uint32_t fixed_point) {
	double return_val = 0.0;
	int16_t long_integer = fixed_point / 65536;
	uint16_t long_decimal = fixed_point - (long_integer * 65536) ;
	return_val = long_integer + ( (double)long_decimal / 65536);
	
	if (return_val < 0.0) {
		return_val-=1.0;
	}
	
	return return_val;
}

uint32_t widechar_len(char* instring, uint32_t _bytes_) {
	uint32_t wstring_len = 0;
	for (uint32_t i = 0; i <= _bytes_/2 ; i++) {
		if ( instring[0] == 0 && instring[1] == 0) {
			break;
		} else {
			instring+=2;
			wstring_len++;
		}
	}
	return wstring_len;
}

bool APar_assert(bool expression, int error_msg, char* supplemental_info) {
	bool force_break = true;
	if (!expression) {
		force_break = false;
		switch (error_msg) {
			case 1 : { //trying to set an iTunes-style metadata tag on an 3GP/MobileMPEG-4
				fprintf(stdout, "AP warning:\n\tSetting the %s tag is for ordinary MPEG-4 files.\n\tIt is not supported on 3gp/amc files.\nSkipping\n", supplemental_info);
				break;
			}
			
			case 2 : { //trying to set a 3gp asset on an mpeg-4 file with the improper brand
				fprintf(stdout, "AP warning:\n\tSetting the %s asset is only available on 3GPP files.\n\tIt is not supported on ordinary MPEG-4 files.\nSkipping\n", supplemental_info);
				break;
			}
			
			case 3 : { //trying to set a 3gp album asset on an early 3gp file that only came into being with 3gp6
				fprintf(stdout, "AtomicParsley warning: \n\tthe 'albm' tag is only allowed on files of '3gp6' brand & later.\nSkipping.\n");
				break;
			}
			case 4 : { //trying to set a 3gp album asset on an early 3gp file that only came into being with 3gp6
				fprintf(stdout, "Major brand of given file: %s\n", supplemental_info);
				break;
			}
			case 5 : { //trying to set metadata on track 33 when there are only 3 tracks
				fprintf(stdout, "AP warning: skipping non-existing track number setting user data atom: %s.\n", supplemental_info);
				break;
			}
			case 6 : { //trying to set id3 metadata on track 33 when there are only 3 tracks
				fprintf(stdout, "AP warning: skipping non-existing track number setting id3 tag: %s.\n", supplemental_info);
				break;
			}
			case 7 : { //trying to set id3 metadata on track 33 when there are only 3 tracks
				fprintf(stdout, "AP warning: the 'meta' atom is being hangled by a %s handler.\n  Remove the 'meta' atom and its contents and try again.\n", supplemental_info);
				break;
			}
			case 8 : { //trying to create an ID32 atom when there is a primary item atom present signaling referenced data (local or external)
				fprintf(stdout, "AP warning: unsupported external or referenced items were detected. Skipping this frame: %s\n", supplemental_info);
				break;
			}
			case 9 : { //trying to eliminate an id3 frame that doesn't exist
				fprintf(stdout, "AP warning: id3 frame %s cannot be deleted because it does not exist.\n", supplemental_info);
				break;
			}
			case 10 : { //trying to eliminate an id3 frame that doesn't exist
				fprintf(stdout, "AP warning: skipping setting unknown %s frame\n", supplemental_info);
				break;
			}
			case 11 : { //insuffient memory to malloc an id3 field (probably picture or encapuslated object)
				fprintf(stdout, "AP error: memory was not alloctated for frame %s. Exiting.\n", supplemental_info);
				break;
			}
		}
	}
	return force_break;
}
