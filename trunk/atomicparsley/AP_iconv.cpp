//==================================================================//
/*
    AtomicParsley - AP_iconv.cpp

    AtomicParsley is GPL software; you can freely distribute, 
    redistribute, modify & use under the terms of the GNU General
    Public License; either version 2 or its successor.

    AtomicParsley is distributed under the GPL "AS IS", without
    any warranty; without the implied warranty of merchantability
    or fitness for either a expressly or implied particular purpose.

    Please see the included GNU General Public License (GPL) for 
    your rights and further details; see the file COPYING. If you
    cannot, write to the Free Software Foundation, 59 Temple Place
    Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

    Copyright ©2005 puck_lock
                                                                   */
//==================================================================//

#include <iconv.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
		//char* encoded_string = (char *)malloc(sizeof(char)*BUF_SIZE);
		//int string_len = strlen(string_value);
		//strncpy(encoded_string, string_value, string_len);
		
		//StringReEncode(encoded_string, string_len, "UTF-16", "UTF-8");
		//StringReEncode(encoded_string, string_len, "LATIN1", "UTF-8");

void StringReEncode(char *a_string, char *tocode, char *fromcode) {
	int length = strlen(a_string);
	//fprintf (stdout, "length %i\n", (int) length);
	size_t result;
	iconv_t frt;
	int len = length;
	int len1 = 2*length;

	char a_string_before[length];
	char a_string_after[len1];
	char *asbptr = a_string_before;
	char *asaptr = a_string_after;
	
	strncpy(a_string_before, a_string, length);
	frt = iconv_open(tocode, fromcode);
	if (frt == (iconv_t) - 1)
	{
	   fprintf(stderr,"Error iconv_open()!\n");
		return;
	}
#if defined (__ppc__) || defined (__ppc64__)
	result = iconv(frt, (const char**) &asbptr, (size_t*) &len, &asaptr, (size_t*) &len1);
#else
	result = iconv(frt, &asbptr, (size_t*) &len, &asaptr, (size_t*) &len1);
#endif
	//fprintf (stdout, "Output string %s\n", a_string_after);
	if (result == (size_t) - 1)
	{
	   //fprintf(stderr,"Error iconv()!\n");
		//return;
		switch (errno) {
			case EILSEQ: {
				//cerr << "Invalid input sequence at " << copy_of_s <<  endl;
				fprintf (stdout, "Invalid input sequence at %s\n", a_string_before);
				exit (-1);
			} 
			case EINVAL: {
				//cerr << "Incomplete multibyte sequence" << endl;
				fprintf (stdout, "Incomplete multibyte sequence.\n");
				exit (-1);
			}
			case E2BIG: {
				//cerr << "Insufficient buffer for transcoding" << endl;
				fprintf (stdout, "Insufficient buffer for transcoding.\n");
				exit (-1);
			}
			default: {
				//cerr << "Bogus errno " << errno << endl;
				fprintf (stdout, "Unknown errno %i\n", (int) result);
				exit (-1);
			}
		}
		return;
	}
	else {
		strncpy(a_string, a_string_after, length);
		//a_string[length]='\0';
	}
	//fprintf(stdout, "%s\n", a_string);
	if (iconv_close(frt) != 0)
		if (iconv_close(frt) != 0)
			fprintf(stderr,"Error iconv_close()!\n");
	return;
}


//NOTES:
// I can determine the encoding from Terminal by reading its preferences ("defaults read com.apple.Terminal StringEncoding")
// 4 = utf8; 30 = Mac OS Roman
// HOWEVER, this is only the default prefences for Terminal.app; each individual window CAN be different, but I can't find where each window/session stores its encoding.
