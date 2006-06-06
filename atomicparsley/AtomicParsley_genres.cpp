//==================================================================//
/*
    AtomicParsley - AtomicParsley_genres.cpp

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

    Copyright ©2005-2006 puck_lock
		
		----------------------
    Code Contributions by:
		
    * Mellow_Flow - fix genre matching/verify genre limits
                                                                   */
//==================================================================//

//#include <sys/types.h>
//#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//just so win32/msvc can get uint8_t defined
#include "AP_commons.h"

//////////////

static const char* ID3v1GenreList[] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco",
		"Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
		"New Age", "Oldies", "Other", "Pop", "R&B",
		"Rap", "Reggae", "Rock", "Techno", "Industrial",
		"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
		"Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
    "Fusion", "Trance", "Classical", "Instrumental", "Acid",
		"House", "Game", "Sound Clip", "Gospel", "Noise",
		"AlternRock", "Bass", "Soul", "Punk", "Space", 
		"Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", 
		"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", 
		"Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
		"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
		"Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
		"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
		"Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
		"Folk", "Folk/Rock", "National Folk", "Swing", "Fast Fusion",
		"Bebob", "Latin", "Revival", "Celtic", "Bluegrass",
		"Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
		"Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", 
		"Humour", "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", 
		"Symphony", "Booty Bass", "Primus", "Porn Groove", 
		"Satire", "Slow Jam", "Club", "Tango", "Samba", 
		"Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", 
		"Duet", "Punk Rock", "Drum Solo", "A capella", "Euro-House",
		"Dance Hall" };
		/*
		"Goa", "Drum & Bass", "Club House", "Hardcore", 
		"Terror", "Indie", "BritPop", "NegerPunk", "Polsk Punk", 
		"Beat", "Christian Gangsta", "Heavy Metal", "Black Metal", "Crossover", 
		"Contemporary C", "Christian Rock", "Merengue", "Salsa", "Thrash Metal", 
		"Anime", "JPop", "SynthPop",
}; */  //apparently the other winamp id3v1 extensions aren't valid

int GenreIntToString(char** genre_string, int genre) {
  if (genre > 0 &&  genre <= (int)(sizeof(ID3v1GenreList)/sizeof(*ID3v1GenreList))) {
			*genre_string = (char*)malloc((strlen(ID3v1GenreList[genre-1])+1)*sizeof(char));
			memset(*genre_string, 0, (strlen(ID3v1GenreList[genre-1])+1)*sizeof(char));
			strcpy(*genre_string, ID3v1GenreList[genre-1]);
        return 0;
    } else {
			*genre_string = (char*)malloc(2*sizeof(char));
			memset(*genre_string, 0, 2*sizeof(char));
			return 1;
    }
}

uint8_t StringGenreToInt(const char* genre_string) {
	uint8_t return_genre = 0;
	uint8_t total_genres = (uint8_t)(sizeof(ID3v1GenreList)/sizeof(*ID3v1GenreList));
	uint8_t genre_length = strlen(genre_string);

	for(uint8_t i = 0; i < total_genres; i++) {
		if (memcmp(genre_string, ID3v1GenreList[i], genre_length) == 0) {
			return_genre = i+1; //the list starts at 0; the embedded genres start at 1
			//fprintf(stdout, "Genre %s is %i\n", ID3v1GenreList[i], return_genre);
			break;
		}
	}
	if ( return_genre > total_genres ) {
		return_genre = 0;
	}
	return return_genre;
}
