//==================================================================//
/*
    AtomicParsley - AP_iconv.cpp

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

    Copyright �2005-2006 puck_lock

		----------------------
    Code Contributions by:
		
    * Mike Brancato - Debian patches & build support
                                                                   */
//==================================================================//

//#include <stdlib.h>
//#include <stdio.h>
#include <string.h>

//#include <wchar.h>

/*
wchar_t* char_to_wchar(char * in_string) {
	wchar_t* unicode_text;
	size_t string_length;
 
	string_length = mbstowcs(NULL, in_string, 0); //passing NULL pre-calculates the size of wchar_t needed
 
	if (string_length == -1) {
		return(NULL); // some error in conversion
	}
 
	unicode_text = (wchar_t*)malloc((string_length + 1) * sizeof(wchar_t)); //+1 for platforms where mbstowc doesn't convert with a terminating L'\0' 
	mbstowcs(unicode_text, in_string, string_length); 
	unicode_text[string_length] = L'\0';
 
	return(unicode_text);
 }
 
char * wchar_to_char(wchar_t * in_string) {
	char* multibyte_text;
	size_t string_length;
 
	string_length = wcstombs(NULL, str, 0); //passing NULL pre-calculates the size of char needed (probably could use wcslen too...)
 
	if (string_length == -1) {
		return(NULL); // some error in conversion
	}
 
	unicode_text = (char*)malloc((string_length + 1) * sizeof(char));
	wcstombs(multibyte_text, in_string, string_length);
	multibyte_text[string_length] = '\0';
	
	return(multibyte_text);
}
*/

//==================================================================//
// fltk's utf8 functions also seem promising
//==================================================================//
// utf conversion functions from libxml2
/*

 Copyright (C) 1998-2003 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.

*/

// Original code for IsoLatin1 and UTF-16 by "Martin J. Duerst" <duerst@w3.org>

static int xmlLittleEndian = 1;

void xmlInitEndianDetection() {
	unsigned short int tst = 0x1234;
	unsigned char *ptr = (unsigned char *) &tst;

	if (*ptr == 0x12) xmlLittleEndian = 0;
    else if (*ptr == 0x34) xmlLittleEndian = 1;
		
	return;
}

/**
 * isolat1ToUTF8:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of ISO Latin 1 chars
 * @inlen:  the length of @in
 *
 * Take a block of ISO Latin 1 chars in and try to convert it to an UTF-8
 * block of chars out.
 * Returns the number of bytes written if success, or -1 otherwise
 * The value of @inlen after return is the number of octets consumed
 *     if the return value is positive, else unpredictable.
 * The value of @outlen after return is the number of octets consumed.
 */
int isolat1ToUTF8(unsigned char* out, int outlen, const unsigned char* in, int inlen) {
    unsigned char* outstart = out;
    const unsigned char* base = in;
    unsigned char* outend;
    const unsigned char* inend;
    const unsigned char* instop;

    if ((out == NULL) || (in == NULL) || (outlen == 0) || (inlen == 0))
	return(-1);

    outend = out + outlen;
    inend = in + (inlen);
    instop = inend;
    
    while (in < inend && out < outend - 1) {
    	if (*in >= 0x80) {
	    *out++ = (((*in) >>  6) & 0x1F) | 0xC0;
        *out++ = ((*in) & 0x3F) | 0x80;
	    ++in;
	}
	if (instop - in > outend - out) instop = in + (outend - out); 
	while (in < instop && *in < 0x80) {
	    *out++ = *in++;
	}
    }	
    if (in < inend && out < outend && *in < 0x80) {
        *out++ = *in++;
    }
    outlen = out - outstart;
    inlen = in - base;
    return(outlen);
}

/**
 * UTF8Toisolat1:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @in:  a pointer to an array of UTF-8 chars
 * @inlen:  the length of @in
 *
 * Take a block of UTF-8 chars in and try to convert it to an ISO Latin 1
 * block of chars out.
 *
 * Returns the number of bytes written if success, -2 if the transcoding fails,
           or -1 otherwise
 * The value of @inlen after return is the number of octets consumed
 *     if the return value is positive, else unpredictable.
 * The value of @outlen after return is the number of octets consumed.
 */
int UTF8Toisolat1(unsigned char* out, int outlen, const unsigned char* in, int inlen) {
    const unsigned char* processed = in;
    const unsigned char* outend;
    const unsigned char* outstart = out;
    const unsigned char* instart = in;
    const unsigned char* inend;
    unsigned int c, d;
    int trailing;

    if ((out == NULL) || (outlen == 0) || (inlen == 0)) return(-1);
    if (in == NULL) {
        /*
	 * initialization nothing to do
	 */
	outlen = 0;
	inlen = 0;
	return(0);
    }
    inend = in + (inlen);
    outend = out + (outlen);
    while (in < inend) {
	d = *in++;
	if      (d < 0x80)  { c= d; trailing= 0; }
	else if (d < 0xC0) {
	    /* trailing byte in leading position */
	    outlen = out - outstart;
	    inlen = processed - instart;
	    return(-2);
        } else if (d < 0xE0)  { c= d & 0x1F; trailing= 1; }
        else if (d < 0xF0)  { c= d & 0x0F; trailing= 2; }
        else if (d < 0xF8)  { c= d & 0x07; trailing= 3; }
	else {
	    /* no chance for this in IsoLat1 */
	    outlen = out - outstart;
	    inlen = processed - instart;
	    return(-2);
	}

	if (inend - in < trailing) {
	    break;
	} 

	for ( ; trailing; trailing--) {
	    if (in >= inend)
		break;
	    if (((d= *in++) & 0xC0) != 0x80) {
		outlen = out - outstart;
		inlen = processed - instart;
		return(-2);
	    }
	    c <<= 6;
	    c |= d & 0x3F;
	}

	/* assertion: c is a single UTF-4 value */
	if (c <= 0xFF) {
	    if (out >= outend)
		break;
	    *out++ = c;
	} else {
	    /* no chance for this in IsoLat1 */
	    outlen = out - outstart;
	    inlen = processed - instart;
	    return(-2);
	}
	processed = in;
    }
    outlen = out - outstart;
    inlen = processed - instart;
    return(outlen);
}

/**
 * UTF16BEToUTF8:
 * @out:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @out
 * @inb:  a pointer to an array of UTF-16 passed as a byte array
 * @inlenb:  the length of @in in UTF-16 chars
 *
 * Take a block of UTF-16 ushorts in and try to convert it to an UTF-8
 * block of chars out. This function assumes the endian property
 * is the same between the native type of this machine and the
 * inputed one.
 *
 * Returns the number of bytes written, or -1 if lack of space, or -2
 *     if the transcoding fails (if *in is not a valid utf16 string)
 * The value of *inlen after return is the number of octets consumed
 *     if the return value is positive, else unpredictable.
 */
int UTF16BEToUTF8(unsigned char* out, int outlen, const unsigned char* inb, int inlenb)
{
    unsigned char* outstart = out;
    const unsigned char* processed = inb;
    unsigned char* outend = out + outlen;
    unsigned short* in = (unsigned short*) inb;
    unsigned short* inend;
    unsigned int c, d, inlen;
    unsigned char *tmp;
    int bits;

    if ((inlenb % 2) == 1)
        (inlenb)--;
    inlen = inlenb / 2;
    inend= in + inlen;
    while (in < inend) {
	if (xmlLittleEndian) {
	    tmp = (unsigned char *) in;
	    c = *tmp++;
	    c = c << 8;
	    c = c | (unsigned int) *tmp;
	    in++;
	} else {
	    c= *in++;
			
			if (c == 0xFEFF) {
				c= *in++; //skip BOM
			}
	} 
        if ((c & 0xFC00) == 0xD800) {    /* surrogates */
	    if (in >= inend) {           /* (in > inend) shouldn't happens */
		outlen = out - outstart;
		inlenb = processed - inb;
	        return(-2);
	    }
	    if (xmlLittleEndian) {
		tmp = (unsigned char *) in;
		d = *tmp++;
		d = d << 8;
		d = d | (unsigned int) *tmp;
		in++;
	    } else {
		d= *in++;
	    }
            if ((d & 0xFC00) == 0xDC00) {
                c &= 0x03FF;
                c <<= 10;
                c |= d & 0x03FF;
                c += 0x10000;
            }
            else {
		outlen = out - outstart;
		inlenb = processed - inb;
	        return(-2);
	    }
        }

	/* assertion: c is a single UTF-4 value */
        if (out >= outend) 
	    break;
        if      (c <    0x80) {  *out++=  c;                bits= -6; }
        else if (c <   0x800) {  *out++= ((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
        else if (c < 0x10000) {  *out++= ((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
        else                  {  *out++= ((c >> 18) & 0x07) | 0xF0;  bits= 12; }
 
        for ( ; bits >= 0; bits-= 6) {
            if (out >= outend) 
	        break;
            *out++= ((c >> bits) & 0x3F) | 0x80;
        }
	processed = (const unsigned char*) in;
    }
    outlen = out - outstart;
    inlenb = processed - inb;
    return(outlen);
}
	 
/**
 * UTF8ToUTF16BE:
 * @outb:  a pointer to an array of bytes to store the result
 * @outlen:  the length of @outb
 * @in:  a pointer to an array of UTF-8 chars
 * @inlen:  the length of @in
 *
 * Take a block of UTF-8 chars in and try to convert it to an UTF-16BE
 * block of chars out.
 *
 * Returns the number of byte written, or -1 by lack of space, or -2
 *     if the transcoding failed. 
 */
int UTF8ToUTF16BE(unsigned char* outb, int outlen, const unsigned char* in, int inlen)
{
    unsigned short* out = (unsigned short*) outb;
    const unsigned char* processed = in;
    const unsigned char *const instart = in;
    unsigned short* outstart= out;
    unsigned short* outend;
    const unsigned char* inend= in+inlen;
    unsigned int c, d;
    int trailing;
    unsigned char *tmp;
    unsigned short tmp1, tmp2;

    /* UTF-16BE has no BOM */
    if ((outb == NULL) || (outlen == 0) || (inlen == 0)) return(-1);
    if (in == NULL) {
	outlen = 0;
	inlen = 0;
	return(0);
    }
    outend = out + (outlen / 2);
    while (in < inend) {
      d= *in++;
      if      (d < 0x80)  { c= d; trailing= 0; }
      else if (d < 0xC0)  {
          /* trailing byte in leading position */
	  outlen = out - outstart;
	  inlen = processed - instart;
	  return(-2);
      } else if (d < 0xE0)  { c= d & 0x1F; trailing= 1; }
      else if (d < 0xF0)  { c= d & 0x0F; trailing= 2; }
      else if (d < 0xF8)  { c= d & 0x07; trailing= 3; }
      else {
          /* no chance for this in UTF-16 */
	  outlen = out - outstart;
	  inlen = processed - instart;
	  return(-2);
      }

      if (inend - in < trailing) {
          break;
      } 

      for ( ; trailing; trailing--) {
          if ((in >= inend) || (((d= *in++) & 0xC0) != 0x80))  break;
          c <<= 6;
          c |= d & 0x3F;
      }

      /* assertion: c is a single UTF-4 value */
        if (c < 0x10000) {
            if (out >= outend)  break;
	    if (xmlLittleEndian) {
		tmp = (unsigned char *) out;
		*tmp = c >> 8;
		*(tmp + 1) = c;
		out++;
	    } else {
		*out++ = c;
	    }
        }
        else if (c < 0x110000) {
            if (out+1 >= outend)  break;
            c -= 0x10000;
	    if (xmlLittleEndian) {
		tmp1 = 0xD800 | (c >> 10);
		tmp = (unsigned char *) out;
		*tmp = tmp1 >> 8;
		*(tmp + 1) = (unsigned char) tmp1;
		out++;

		tmp2 = 0xDC00 | (c & 0x03FF);
		tmp = (unsigned char *) out;
		*tmp = tmp2 >> 8;
		*(tmp + 1) = (unsigned char) tmp2;
		out++;
	    } else {
		*out++ = 0xD800 | (c >> 10);
		*out++ = 0xDC00 | (c & 0x03FF);
	    }
        }
        else
	    break;
	processed = in;
    }
    outlen = (out - outstart) * 2;
    inlen = processed - instart;
    return(outlen);
}

