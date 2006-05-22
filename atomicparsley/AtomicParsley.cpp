//==================================================================//
/*
    AtomicParsley - AtomicParsley.cpp

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
		
    * Mike Brancato - Debian patches & build support
    * Lowell Stewart - null-termination bugfix for Apple compliance
		* Brian Story - native Win32 patches; memset/framing/leaks fixes
                                                                   */
//==================================================================//

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <wchar.h>

#include "AP_commons.h"
#include "AtomicParsley.h"
#include "AP_iconv.h"
#include "AtomicParsley_genres.h"

#if defined (DARWIN_PLATFORM)
#include "AP_NSImage.h"
#include "AP_NSFile_utils.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////
//                               Global Variables                                    //
///////////////////////////////////////////////////////////////////////////////////////

bool modified_atoms = false;
bool alter_original = false;

bool cvs_build = true; //controls which type of versioning - cvs build-time stamp
//bool cvs_build = false; //controls which type of versioning - release number

FILE* source_file;
off_t file_size;

struct AtomicInfo parsedAtoms[MAX_ATOMS]; //(most I've seen is 144 for an untagged mp4)
short atom_number = 0;
short generalAtomicLevel = 1;

bool file_opened = false;
bool parsedfile = false;
bool Create__udta_meta_hdlr__atom = false;
bool move_mdat_atoms = true;
bool skip_meta_hdlr_creation = true;
int metadata_style = UNDEFINED_STYLE;

uint32_t max_buffer = 4096*125; // increased to 512KB

uint32_t bytes_before_mdat=0;
uint32_t bytes_into_mdat = 0;
uint64_t mdat_supplemental_offset = 0;
uint32_t removed_bytes_tally = 0;
uint32_t new_file_size = 0; //used for the progressbar

bool contains_unsupported_64_bit_atom = false; //reminder that there are some 64-bit files that aren't yet supported (and where that limit is set)

#if defined (WIN32) || defined (__CYGWIN__)
short max_display_width = 45;
#else
short max_display_width = 75; //ah, figured out grub - vga=773 makes a whole new world open up
#endif
char* file_progress_buffer=(char*)malloc( sizeof(char)* (max_display_width+10) ); //+5 for any overflow in "%100", or "|"

struct PicPrefs myPicturePrefs;
bool parsed_prefs = false;

EmployedCodecs track_codecs = {false, false, false, false, false, false, false, false, false, false};

///////////////////////////////////////////////////////////////////////////////////////
//                                Versioning                                         //
///////////////////////////////////////////////////////////////////////////////////////

void ShowVersionInfo() {

#if defined (UTF8_ENABLED)
#define unicode_enabled	"(utf8)"
#else

#if defined (UTF16_ENABLED)
#define unicode_enabled	"(utf16)"
//its utf16 in the sense that any text entering on a modern Win32 system enters as utf16le - but gets converted immediately after AP.exe starts to utf8
//all arguments, strings, filenames, options are sent around as utf8. For modern Win32 systems, filenames get converted to utf16 for output as needed.
//Any strings to be set as utf16 in 3gp assets are converted to utf16be as needed (true for all OS implementations).
//Printing out to the console is a mixed bag of vanilla ascii & utf16le. Redirected output should be utf8. TODO: Win32 output should be uniformly utf16le.
#else
#define unicode_enabled	""
#endif

#endif

	if (cvs_build) {  //below is the versioning from cvs if used; remember to switch to AtomicParsley_version for a release

		fprintf(stdout, "AtomicParsley from svn built on %s %s\n", __DATE__, unicode_enabled);
	
	} else {  //below is the release versioning

		fprintf(stdout, "AtomicParsley version: %s %s\n", AtomicParsley_version, unicode_enabled); //release version
	}

	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                               Generic Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

// http://www.flipcode.com/articles/article_advstrings01.shtml
bool IsUnicodeWinOS() {
#if defined (_MSC_VER)
  OSVERSIONINFOW		os;
  memset(&os, 0, sizeof(OSVERSIONINFOW));
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
  return (GetVersionExW(&os) != 0);
#else
	return false;
#endif
}

/*----------------------
findFileSize
  utf8_filepath - a pointer to a string (possibly utf8) of the full path to the file

    take an ascii/utf8 filepath (which if under a unicode enabled Win32 OS was already converted from utf16le to utf8 at program start) and test if
		AP is running on a unicode enabled Win32 OS. If it is, convert the utf8 filepath to a utf16 (native-endian) filepath & pass that to a wide stat.
		Or stat it with a utf8 filepath on Unixen.
----------------------*/
off_t findFileSize(const char *utf8_filepath) {
	if ( IsUnicodeWinOS() ) {
#if defined (_MSC_VER)
		wchar_t* utf16_filepath = Convert_multibyteUTF8_to_wchar(utf8_filepath);
		
		struct _stat fileStats;
		_wstat(utf16_filepath, &fileStats);
		
		free(utf16_filepath);
		utf16_filepath = NULL;
		return fileStats.st_size;
#endif
	} else {
		struct stat fileStats;
		stat(utf8_filepath, &fileStats);
		return fileStats.st_size;
	}
	return 0; //won't ever get here
}

/*----------------------
APar_OpenFile
  utf8_filepath - a pointer to a string (possibly utf8) of the full path to the file
	file_flags - 3 bytes max for the flags to open the file with (read, write, binary mode....)

    take an ascii/utf8 filepath (which if under a unicode enabled Win32 OS was already converted from utf16le to utf8 at program start) and test if
		AP is running on a unicode enabled Win32 OS. If it is, convert the utf8 filepath to a utf16 (native-endian) filepath & pass that to a wide fopen
		with the 8-bit file flags changed to 16-bit file flags. Or open a utf8 file with vanilla fopen on Unixen.
----------------------*/
FILE* APar_OpenFile(const char* utf8_filepath, const char* file_flags) {
	FILE* aFile = NULL;
	if ( IsUnicodeWinOS() ) {
#if defined (_MSC_VER)
		wchar_t* Lfile_flags = (wchar_t *)malloc(sizeof(wchar_t)*4);
		memset(Lfile_flags, 0, sizeof(wchar_t)*4);
		mbstowcs(Lfile_flags, file_flags, strlen(file_flags) );
		
		wchar_t* utf16_filepath = Convert_multibyteUTF8_to_wchar(utf8_filepath);
		
		aFile = _wfopen(utf16_filepath, Lfile_flags);
		
		free(utf16_filepath);
		utf16_filepath = NULL;
#endif
	} else {
		aFile = fopen(utf8_filepath, file_flags);
	}
	return aFile;
}

/*----------------------
openSomeFile
  utf8_filepath - a pointer to a string (possibly utf8) of the full path to the file
	open - flag to either open or close (function does both)

    take an ascii/utf8 filepath and either open or close it; used for the main ISO Base Media File; store the resulting FILE* in a global source_file
----------------------*/
FILE* openSomeFile(const char* utf8file, bool open) {
	FILE* aFile = NULL;
	if ( open && !file_opened) {
		source_file = APar_OpenFile(utf8file, "rb");
		if (source_file != NULL) {
			file_opened = true;
		}
	} else {
		fclose(source_file);
		file_opened = false;
	}
	return source_file;
}

void TestFileExistence(const char *filePath, bool errorOut) {
	FILE *a_file = NULL;
	a_file = APar_OpenFile(filePath, "rb");
	if( (a_file == NULL) && errorOut ){
		fprintf(stderr, "AtomicParsley error: can't open %s for reading: %s\n", filePath, strerror(errno));
		exit(1);
	} else {
		fclose(a_file);
	}
}

char* extractAtomName(char *fileData, int name_position) {
//name_position = 1 for normal atoms and needs to be done first; 2 for uuid atoms (which can only occur after we first find the atomName == "uuid")
	char *scan_atom_name=(char *)malloc(sizeof(char)*5);
	memset(scan_atom_name, 0, sizeof(char)*5);

	for (int i=0; i < 4; i++) {
		scan_atom_name[i] = fileData[i + (name_position * 4) ]; //we want the 4 byte 'atom' in data [4,5,6,7] (or the uuid atom name in 8,9,10,11)
	}

	return scan_atom_name;
	free(scan_atom_name);
}

void APar_FreeMemory() {
	for(int iter=0; iter < atom_number; iter++) {
		if (parsedAtoms[iter].AtomicData != NULL) {
			free(parsedAtoms[iter].AtomicData);
			parsedAtoms[iter].AtomicData = NULL;
		}
	}
	
	return;
}

int APar_TestArtworkBinaryData(const char* artworkPath) {
	int artwork_dataType = 0;
	FILE *artfile = APar_OpenFile(artworkPath, "rb");
	if (artfile != NULL) {
		char *pic_data=(char *)malloc(sizeof(char)*9 + 1);
		memset(pic_data, 0, sizeof(char)*9 + 1);
		
		fread(pic_data, 1, 8, artfile);
		if ( strncmp(pic_data, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0 ) {
			artwork_dataType = AtomicDataClass_PNGBinary;
		} else if ( strncmp(pic_data, "\xFF\xD8\xFF\xE0", 4) == 0 ) {
			artwork_dataType = AtomicDataClass_JPEGBinary;
		} else {
			fprintf(stdout, "AtomicParsley error: %s\n\t image file is not jpg/png and cannot be embedded.\n", artworkPath);
			exit(1);
		}
		fclose(artfile);
		free(pic_data);
		
	} else {
		fprintf(stdout, "AtomicParsley error: %s\n\t image file could not be opened.\n", artworkPath);
		exit(1);
	}
	return artwork_dataType;
}

#if defined (DARWIN_PLATFORM)
// enables writing out the contents of a single memory-resident atom out to a text file; for in-house testing purposes only - and unused in some time
void APar_AtomicWriteTest(short AtomicNumber, bool binary) {
	AtomicInfo anAtom = parsedAtoms[AtomicNumber];
	
	char* indy_atom_path = (char *)malloc(sizeof(char)*MAXPATHLEN); //this malloc can escape memset because its only for in-house testing
	strcat(indy_atom_path, "/Users/");
	strcat(indy_atom_path, getenv("USER") );
	strcat(indy_atom_path, "/Desktop/singleton_atom.txt");

	FILE* single_atom_file;
	single_atom_file = fopen(indy_atom_path, "wb");
	if (single_atom_file != NULL) {
		
		if (binary) {
			fwrite(anAtom.AtomicData, (size_t)(anAtom.AtomicLength - 12), 1, single_atom_file);
		} else {
			char* data = (char*)malloc(sizeof(char)*4);
			char4TOuint32(anAtom.AtomicLength, data);
		
			fwrite(data, 4, 1, single_atom_file);
			fwrite(anAtom.AtomicName, 4, 1, single_atom_file);
		
			char4TOuint32((uint32_t)anAtom.AtomicDataClass, data);
			fwrite(data, 4, 1, single_atom_file);

			fwrite(anAtom.AtomicData, strlen(anAtom.AtomicData)+1, 1, single_atom_file);
			free(data);
		}
	}
	fclose(single_atom_file);
	free(indy_atom_path);
	return;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
//                        Picture Preferences Functions                              //
///////////////////////////////////////////////////////////////////////////////////////

PicPrefs ExtractPicPrefs(char* env_PicOptions) {
	if (!parsed_prefs) {
		
		parsed_prefs = true; //only set default values & parse once
		
		myPicturePrefs.max_dimension=0; //dimensions won't be used to alter image
		myPicturePrefs.dpi = 72;
		myPicturePrefs.max_Kbytes = 0; //no target size to shoot for
		myPicturePrefs.allJPEG = false;
		myPicturePrefs.allPNG = false;
		myPicturePrefs.addBOTHpix = false;
		char* this_pref;
		while (env_PicOptions != NULL) {
			this_pref = strsep(&env_PicOptions,":");
			char* a_pref = strdup(this_pref);
			if (strncmp(a_pref,"MaxDimensions=",14) == 0) {
				char* MaxDimPref = strsep(&a_pref,"=");
				long DimensionNumber=strtol(a_pref, NULL, 10);
				//fprintf(stdout, "dimensions %i\n", (int)DimensionNumber);
				myPicturePrefs.max_dimension = (int)DimensionNumber;
				
			} else if (strncmp(a_pref,"DPI=",4) == 0) {
				char* TotalDPI = strsep(&a_pref,"=");
				long dpiNumber=strtol(a_pref, NULL, 10);
				//fprintf(stdout, "dpi %i\n", (int)dpiNumber);
				myPicturePrefs.dpi = (int)dpiNumber;
				
			} else if (strncmp(a_pref,"MaxKBytes=",10) == 0) {
				char* MaxBytes = strsep(&a_pref,"=");
				long bytesNumber=strtol(a_pref, NULL, 10);
				//fprintf(stdout, "dpi %i\n", (int)dpiNumber);
				myPicturePrefs.max_Kbytes = (int)bytesNumber*1024;
				
			} else if (strncmp(a_pref,"AllPixJPEG=",11) == 0) {
				char* onlyJPEG = strsep(&a_pref,"=");
				if (strncmp(a_pref, "true", 4) == 0) {
					//fprintf(stdout, "it's true\n");
					myPicturePrefs.allJPEG = true;
				}
				
			} else if (strncmp(a_pref,"AllPixPNG=",10) == 0) {
				char* onlyPNG = strsep(&a_pref,"=");
				if (strncmp(a_pref, "true", 4) == 0) {
					//fprintf(stdout, "it's true\n");
					myPicturePrefs.allPNG = true;
				}
				
			} else if (strncmp(a_pref,"AddBothPix=",11) == 0) {
				char* addBoth = strsep(&a_pref,"=");
				if (strncmp(a_pref, "true", 4) == 0) {
					//fprintf(stdout, "it's true\n");
					myPicturePrefs.addBOTHpix = true;
				}
				
			} else if (strncmp(a_pref,"SquareUp",7) == 0) {
				myPicturePrefs.squareUp = true;
				
			} else if (strncmp(a_pref,"removeTempPix",7) == 0) {
				myPicturePrefs.removeTempPix = true;
			}
		}
	}
	return myPicturePrefs;
}

///////////////////////////////////////////////////////////////////////////////////////
//                            Track Level Atom Info                                  //
///////////////////////////////////////////////////////////////////////////////////////

void APar_TrackInfo(uint8_t &total_tracks, uint8_t &track_num, short &codec_atom) { //returns the codec used for each 'trak' atom; used under Mac OS X only
	uint8_t track_tally = 0;
	short iter = 0;
	
	while (parsedAtoms[iter].NextAtomNumber != 0) {
	
		if ( strncmp(parsedAtoms[iter].AtomicName, "trak", 4) == 0) {
			track_tally += 1;
			if (track_num == 0) {
				total_tracks += 1;
				
			} else if (track_num == track_tally) {
				//drill down into stsd
				short next_atom = parsedAtoms[iter].NextAtomNumber;
				while (parsedAtoms[next_atom].AtomicLevel > parsedAtoms[iter].AtomicLevel) {
					
					if (strncmp(parsedAtoms[next_atom].AtomicName, "stsd", 4) == 0) {
					
						codec_atom = parsedAtoms[next_atom].AtomicNumber;
						//return with the stsd atom - its stsd_codec uint32_t holds the 4CC name of the codec for the trak
						//(mp4v, avc1, drmi, mp4a, drms, alac, mp4s, text, tx3g or jpeg)
						return;
					} else {
						next_atom = parsedAtoms[next_atom].NextAtomNumber;
					}
				}
			}
		}
		iter=parsedAtoms[iter].NextAtomNumber;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                            Locating/Finding Atoms                                 //
///////////////////////////////////////////////////////////////////////////////////////

bool APar_SimpleExistsAtom(const char* atom_name) {
	bool exists_atom = false;
	short iter = 0;
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		if ( strncmp(parsedAtoms[iter].AtomicName, atom_name, 4) == 0) {
			exists_atom = true;
			break;
		} else {
			iter=parsedAtoms[iter].NextAtomNumber;
		}
	}
	return exists_atom;
}

short APar_FindParentAtom(int order_in_tree, short this_atom_level) {
	short thisAtom = 0;
	for(short iter=order_in_tree-1; iter >= 0; iter--) {
		if (parsedAtoms[iter].AtomicLevel == this_atom_level-1) {
			thisAtom = iter;
			break;
		}
	}
	return thisAtom;
}

short APar_FindPrecedingAtom(short an_atom_num) {
	short precedingAtom = 0;
	short iter = 0;
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		if (parsedAtoms[iter].NextAtomNumber == parsedAtoms[an_atom_num].NextAtomNumber) {
			break;
		} else {
			precedingAtom = iter;
			iter=parsedAtoms[iter].NextAtomNumber;
		}
	}
	return precedingAtom;
}

short Apar_FindParent_AtLevel(short starting_atom, short parental_level) {
	short parent_atom = starting_atom;
	while (true) {
		if (parsedAtoms[parent_atom].AtomicLevel > 1) {
			parent_atom = APar_FindPrecedingAtom(parent_atom);
			//fprintf(stdout, "Preceding %s is %s\n", parsedAtoms[starting_atom].AtomicName, parsedAtoms[parent_atom].AtomicName);
		} else {
			break;
		}
		if (parsedAtoms[parent_atom].AtomicLevel == parental_level) {
			break;
		}
	}
	return parent_atom;
}

bool APar_Eval_ChunkOffsetImpact(short an_atom_num) {
	bool impact_calculations_directly = false;
	short iter = 0;
	
	while (true) {
		if ( strncmp(parsedAtoms[iter].AtomicName, "mdat", 4) == 0) {
			if (an_atom_num < iter && parsedAtoms[an_atom_num].AtomicLevel == 1) {
				impact_calculations_directly = true;
			}
			break;
		} else {
			iter=parsedAtoms[iter].NextAtomNumber;
		}
		if (iter == 0) {
			break;
		}
	}
	return impact_calculations_directly;
}

short APar_FindLastAtom() {
	short this_atom_num = 0; //start our search with the first atom
	while (parsedAtoms[this_atom_num].NextAtomNumber != 0) {
		this_atom_num = parsedAtoms[this_atom_num].NextAtomNumber;
	}
	return this_atom_num;
}

short APar_FindEndingAtom() {
	short end_atom_num = 0; //start our search with the first atom
	while (true) {
		if ( (parsedAtoms[end_atom_num].NextAtomNumber == 0) || (end_atom_num == atom_number-1) ) {
			break;
		} else {
			end_atom_num = parsedAtoms[end_atom_num].NextAtomNumber;
		}
	}
	return end_atom_num;
}

short APar_FindLastChild_of_ParentAtom(short thisAtom) {
	short hierarchy_ending = thisAtom;
	
	while (true) {
		if (parsedAtoms[ parsedAtoms[hierarchy_ending].NextAtomNumber ].AtomicLevel > parsedAtoms[thisAtom].AtomicLevel) {
			hierarchy_ending = parsedAtoms[hierarchy_ending].NextAtomNumber;		
		} else {
			break;
		}
		
		if (hierarchy_ending == 0) {
			break;
		}
	}
	//return (hierarchy_ending == 0 ? 0 : parsedAtoms[hierarchy_ending].AtomicNumber);
	return hierarchy_ending;
}

bool APar_AtomHasChildren(short thisAtom) {
	bool is_parent = false;
	if (parsedAtoms[parsedAtoms[thisAtom].NextAtomNumber].AtomicLevel > parsedAtoms[thisAtom].AtomicLevel) {
		is_parent = true;
	}
	return is_parent;
}

AtomicInfo APar_FindAtom(const char* atom_name, bool createMissing, bool uuid_atom_type, bool findChild, bool directFind) {
	short present_atom_level = 1; //from where our generalAtomicLevel starts
	char* atom_hierarchy = strdup(atom_name);
	char* found_hierarchy = (char *)malloc(sizeof(char)*400); //that should hold it
	memset(found_hierarchy, 0, sizeof(char)*400);

	bool is_uuid_atom = false;
	char* uuid_name = (char *)malloc(sizeof(char)*5);
	AtomicInfo thisAtom = { 0 };
	char* parent_name = (char *)malloc(sizeof(char)*5);
	memset(uuid_name, 0, sizeof(char)*5);
	memset(parent_name, 0, sizeof(char)*5);
	
	char *search_atom_name = strsep(&atom_hierarchy,".");
	char* dup_search_atom_name;
	int search_atom_start_num = 0;
	
	while (search_atom_name != NULL) {
		
		uuid_name = strstr(search_atom_name, "uuid=");
		if (uuid_atom_type && (uuid_name != NULL) ) {
			dup_search_atom_name = strdup(search_atom_name);
			uuid_name = strsep(&dup_search_atom_name,"="); //search_atom_name will still retain a "uuid=ATOM" if needed for APar_CreateSparseAtom
			is_uuid_atom = true;
		} else {
			is_uuid_atom = false;
 		}
		
		for(int iter=search_atom_start_num; iter < atom_number; iter++) {
		
			if (directFind && strncmp(parsedAtoms[iter].AtomicName, search_atom_name, 4) == 0) {
				if (atom_hierarchy == NULL) {
					thisAtom = parsedAtoms[iter];
				}
				search_atom_start_num=iter;
				if (present_atom_level == thisAtom.AtomicLevel) {
					break;
				}
			}
			if ( ((strncmp(parsedAtoms[iter].AtomicName, search_atom_name, 4) == 0) && (parsedAtoms[iter].AtomicLevel == present_atom_level)) ||
				     (uuid_atom_type && is_uuid_atom && (strncmp(parsedAtoms[iter].AtomicName, dup_search_atom_name, 4) == 0) && parsedAtoms[iter].uuidAtomType) ) {
					
				if (uuid_atom_type && is_uuid_atom && (strncmp(parsedAtoms[iter].AtomicName, dup_search_atom_name, 4) == 0) && parsedAtoms[iter].uuidAtomType) {
					thisAtom = parsedAtoms[iter];
					break;
				}
					
				if (atom_hierarchy == NULL) {
				
					//we have found the proper hierarchy we want
					AtomicInfo parent_atom = parsedAtoms[ APar_FindParentAtom(parsedAtoms[iter].AtomicNumber, parsedAtoms[iter].AtomicLevel) ];

					if (parent_atom.AtomicLevel > 1 && !directFind) {
						if ( strncmp(parent_atom.AtomicName, parent_name, 4) == 0) {
							thisAtom = parsedAtoms[iter];
							break;
							
						} else {
							int next_atom = parent_atom.NextAtomNumber; //int next_atom = parsedAtoms[iter].NextAtomNumber;
							while (true) {

								if ( (strncmp(parsedAtoms[next_atom].AtomicName, parent_name, 4) == 0) && (uuid_atom_type == parsedAtoms[next_atom].uuidAtomType) ) {
									if (findChild) { //this won't find a covr atoms's last child (which isn't necessarily the next one), but who cares: APar_CreateSparseAtom will
										next_atom = parsedAtoms[next_atom].NextAtomNumber;
										thisAtom = parsedAtoms[next_atom]; //this is IT!!!
									} else {
										thisAtom = parsedAtoms[next_atom]; //this is IT!!!
									}
									break;
									
								}
								next_atom = parsedAtoms[next_atom].NextAtomNumber;
								if (next_atom == 0) {
									break;
								}
							}
							break;
						}

					} else {
						//we've come to the right level, & hieararchy, now strncmp through the atoms in this level
						if (directFind) {
							//directFind doesn't use parent atoms to search - iTunes metadata 'data' atoms are DEFINED by their parent atoms, so parent_atoms are essential for them, but little use for a 'moov.udta.meta.hdlr' atom
							int test_atom_num = iter; //int test_atom_num = parsedAtoms[iter].NextAtomNumber;
							while (true) {
								//fprintf(stdout, "eval %s\n", parsedAtoms[test_atom_num].AtomicName);
								if ( (present_atom_level > parsedAtoms[test_atom_num].NextAtomNumber) || (test_atom_num == 0) ) {
									break; //we've gone UP past our level in the hierarchy
								}
								if ( strncmp(parsedAtoms[test_atom_num].AtomicName, search_atom_name, 4) == 0 ) {
									thisAtom = parsedAtoms[test_atom_num]; //this is IT!!!
									search_atom_start_num=thisAtom.AtomicNumber;
									break;
								}
								test_atom_num = parsedAtoms[test_atom_num].NextAtomNumber;
							}

							break; //break out of the entire for loop now that we've exited the while loop							
						}
					}
				}
				if (atom_hierarchy != NULL) {
					present_atom_level++;
					search_atom_start_num=iter; //this keeps advancing the starting point for the searches in the hierarchy.

					break;
				}
			}	
			if (iter+1 == atom_number) {
				//fprintf(stdout, "Atoms that need creation: \"%s\" - %s\n", search_atom_name, found_hierarchy);
				if (atom_hierarchy == NULL) {
					if (createMissing) {
						thisAtom = APar_CreateSparseAtom(found_hierarchy, search_atom_name, atom_hierarchy, present_atom_level, true);
					}
				} else {
					while (atom_hierarchy != NULL) {
						if (createMissing) {
							thisAtom = APar_CreateSparseAtom(found_hierarchy, search_atom_name, atom_hierarchy, present_atom_level, true);
						}
						search_atom_name = strsep(&atom_hierarchy,".");
						present_atom_level++;
					}
				}
				break;
			}
		}
		if (present_atom_level > 2) {
			free(parent_name); //<-----NEWNEWNEW verified leak fix
			parent_name=NULL; //<-----NEWNEWNEW verified leak fix
			parent_name= strdup(search_atom_name);
			strcat(found_hierarchy, ".");
		}
		
		strcat(found_hierarchy, search_atom_name);
		search_atom_name = strsep(&atom_hierarchy,".");
	}
	free(atom_hierarchy);
	free(found_hierarchy);
	free(parent_name);
	free(uuid_name);
	atom_hierarchy = NULL;
	found_hierarchy = NULL;
	parent_name = NULL; //<-----NEWNEWNEW verified leak fix
	uuid_name = NULL;

  return thisAtom;
}

short APar_LocateParentHierarchy(const char* the_hierarchy) { //This only gets used when we are adding atoms at the end of the hierarchy
	short last_atom = 0;
	char* atom_hierarchy = strdup(the_hierarchy);
	char* search_atom_name = strsep(&atom_hierarchy,".");
	char* found_hierarchy = (char *)malloc(sizeof(char)*400); //change the allocation to better detect leaks
	memset(found_hierarchy, 0, sizeof(char)*400);
	
	AtomicInfo parent_atom;
	
	strcat(found_hierarchy, search_atom_name);
	
	while (atom_hierarchy != NULL) { //test that the atom doesn't end with data; we want the parent to data; and then locate the parent of THAT atom and return the last atom in parent to the metadata (typically ilst's last child or meta's last child for hdlr)
		search_atom_name = strsep(&atom_hierarchy,".");
		if (atom_hierarchy != NULL) {
			if (strncmp(atom_hierarchy, "data", 4) == 0) {
				break; // found_hierarchy will now contain the path to the parent to this "data" atom; search_atom_name will = "ilst" then
			}
		}
		strcat(found_hierarchy, ".");
		strcat(found_hierarchy, search_atom_name);
	}
	
	parent_atom = APar_FindAtom(found_hierarchy, false, false, true, false);
	if (parent_atom.AtomicNumber < 0 || parent_atom.AtomicNumber > atom_number) {
		return (atom_number-1); 
	}
	
	short this_atom_num = parent_atom.NextAtomNumber;

	if ( (this_atom_num > atom_number) || (this_atom_num < 1) ) {
		last_atom = APar_FindEndingAtom();
		
		free(found_hierarchy);
		free(atom_hierarchy);
		found_hierarchy = NULL;
		atom_hierarchy = NULL;
	
		return (atom_number-1); 
	}		

	last_atom = this_atom_num;
	
	while (true) {
		if (parsedAtoms[this_atom_num].AtomicLevel > parent_atom.AtomicLevel) {
			last_atom = this_atom_num;
			this_atom_num = parsedAtoms[this_atom_num].NextAtomNumber;
			
			if (strncmp(parsedAtoms[last_atom].AtomicName, search_atom_name, 4) == 0) {
				if (parsedAtoms[last_atom].AtomicLevel > parsedAtoms[this_atom_num].AtomicLevel) {
					break;
				}
			}
			
			if (this_atom_num == 0) { //and the end of the line it is....
				break;
			}
		} else {
				break;
		}
	}
	free(found_hierarchy);
	free(atom_hierarchy);
	found_hierarchy = NULL;
	atom_hierarchy = NULL;
		
	return last_atom;
}

AtomicInfo APar_LocateAtomInsertionPoint(const char* the_hierarchy, bool findLastChild) {
	//fprintf(stdout, "Searching for this path %s\n", the_hierarchy);
	AtomicInfo InsertionPointAtom = { 0 };
	short present_atom_level = 1;
	int nextAtom = 0;
	int pre_parent_atom = 0;
	char* atom_hierarchy = strdup(the_hierarchy);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	bool is_uuid_atom = false;
	char* uuid_name = (char *)malloc(sizeof(char)*5);
	memset(uuid_name, 0, sizeof(char)*5);
	
	while (search_atom_name != NULL) {
		AtomicInfo thisAtom;
		
		uuid_name = strstr(search_atom_name, "uuid=");
		if (uuid_name != NULL) {
			search_atom_name = strsep(&search_atom_name,"="); //we can strsep directly into search_atom_name because search_atom_name isn't passed anywhere
			is_uuid_atom = true;
		} else {
			is_uuid_atom = false;
		}
		
		if ( (atom_hierarchy == NULL) && (present_atom_level == 1) ) {
			
			for (int i=0; i < atom_number; i++) {
				if (is_uuid_atom) {
					//this would probably be a PSP-style uuid atom which isn't supported for writing (it isn't text), but this is where it would be determined
					if ( parsedAtoms[i].AtomicLevel > 1 ) {//this will add a Level1 atom just before any hierarchy forms
						InsertionPointAtom = parsedAtoms[pre_parent_atom]; //returns the atom just before 'moov'; either 'ftyp' or the last uuid='ATOM' atom
						break;
					}
				}
				if (findLastChild) {
					
					if ( strncmp(search_atom_name, parsedAtoms[i].AtomicName, 4) == 0 ) { //this essentially searches for 'moov' atom children
						int child_atom_num = parsedAtoms[i].NextAtomNumber;
						while (parsedAtoms[child_atom_num].AtomicLevel > parsedAtoms[i].AtomicLevel) {
							if (parsedAtoms[i].AtomicLevel == parsedAtoms[parsedAtoms[child_atom_num].NextAtomNumber].AtomicLevel) {
								break; //we found the last child (in all likelyhood of a "moov" atom)
							}
						child_atom_num = parsedAtoms[child_atom_num].NextAtomNumber;
						}
						InsertionPointAtom = parsedAtoms[child_atom_num];
						break;
						
					//this will add a Level1 atom at the end of all atoms (nowhere else, just the end)
					} else if ( parsedAtoms[i].NextAtomNumber == 0 ) {
						InsertionPointAtom = parsedAtoms[i];
						break;
					}
				} else { //this will add a Level1 atom just before any hierarchy forms
					if ( parsedAtoms[i].AtomicLevel > 1 ) {
						InsertionPointAtom = parsedAtoms[pre_parent_atom];
						break;
					}
				}
				pre_parent_atom = nextAtom;
				nextAtom =i;
			}
			break;
		}
		
		thisAtom = parsedAtoms[nextAtom]; //follow the (possibly modified) atom tree hierarcy
		//fprintf(stdout, "Try to find atom \"%s\", found %s (found level: %i, current search level %i)\n", search_atom_name, thisAtom.AtomicName, thisAtom.AtomicLevel, present_atom_level);
		if ( (strncmp(thisAtom.AtomicName,search_atom_name,4) == 0) && (thisAtom.AtomicLevel == present_atom_level) ) {
			
			if (findLastChild) {
				
				if (atom_hierarchy == NULL ) {
					while (nextAtom != 0) {
						//fprintf(stdout, "Testing atom \"%s\", level %i(%i)\n", parsedAtoms[nextAtom].AtomicName, parsedAtoms[nextAtom].AtomicLevel, present_atom_level);
						if (parsedAtoms[nextAtom].AtomicLevel >= present_atom_level) {
							if ( (parsedAtoms[nextAtom].AtomicLevel == present_atom_level) && (pre_parent_atom != 0) ) {
								//we've already hit an atom on the same level we want; the last child came just before this then
								InsertionPointAtom = parsedAtoms[pre_parent_atom];
								break;
							}
							pre_parent_atom = nextAtom;
							nextAtom = parsedAtoms[nextAtom].NextAtomNumber;
							if ( (parsedAtoms[nextAtom].AtomicLevel < present_atom_level) && (pre_parent_atom != 0) ) {
								InsertionPointAtom = parsedAtoms[pre_parent_atom];
								break;
							} else if ( parsedAtoms[nextAtom].NextAtomNumber == 0) {
								//end o' the line
								InsertionPointAtom = parsedAtoms[nextAtom];
								break;
							}
						} else {
							InsertionPointAtom = parsedAtoms[pre_parent_atom];
							break;
						}
					}
				}
			} else {
				if (atom_hierarchy == NULL) {
					InsertionPointAtom = thisAtom;
				}
			}
			search_atom_name = strsep(&atom_hierarchy,".");
			present_atom_level++;
		}
		nextAtom = thisAtom.NextAtomNumber;
		
		if (thisAtom.NextAtomNumber == 0 ) {
			break;
		}

	}		
	free(atom_hierarchy);	// A "Deallocation of a pointer not malloced" occured for a screwed up m4a file (for gnre & ©grp ONLY oddly) & for APar_Find atom that returns short
	free(uuid_name);
	atom_hierarchy = NULL;
	uuid_name = NULL;
	
	if (InsertionPointAtom.NextAtomNumber == 0) {
		InsertionPointAtom = parsedAtoms[APar_LocateParentHierarchy(the_hierarchy)];
	}
	
	return InsertionPointAtom;
}

///////////////////////////////////////////////////////////////////////////////////////
//                   'data'/'stco'/3GP asset Atom extraction                         //
///////////////////////////////////////////////////////////////////////////////////////

void APar_AtomicRead(short this_atom_number) {
	//fprintf(stdout, "Reading %u bytes\n", parsedAtoms[this_atom_number].AtomicLength-12 );
	parsedAtoms[this_atom_number].AtomicData = (char*)malloc(sizeof(char)* (size_t)(parsedAtoms[this_atom_number].AtomicLength) );
	memset(parsedAtoms[this_atom_number].AtomicData, 0, sizeof(char)* (size_t)(parsedAtoms[this_atom_number].AtomicLength) );
	
	fseek(source_file, parsedAtoms[this_atom_number].AtomicStart+12, SEEK_SET);
	fread(parsedAtoms[this_atom_number].AtomicData, 1, parsedAtoms[this_atom_number].AtomicLength-12, source_file);
	return;
}

#if defined (_MSC_VER)
void APar_unicode_win32Printout(wchar_t* unicode_out) { //based on http://blogs.msdn.com/junfeng/archive/2004/02/25/79621.aspx
	//its possible that this isn't even available on windows95
	DWORD dwBytesWritten;
	DWORD fdwMode;
	HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	// ThreadLocale adjustment, resource loading, etc. is skipped
	if ( (GetFileType(outHandle) & FILE_TYPE_CHAR) && GetConsoleMode( outHandle, &fdwMode) ) {
		if ( wmemcmp(unicode_out, L"\xEF\xBB\xBF", 3) != 0 ) { //skip BOM when writing directly to the console
			WriteConsoleW( outHandle, unicode_out, wcslen(unicode_out), &dwBytesWritten, 0);
		}
	} else {
		//http://www.ansi.edu.pk/library/WSLP/book4/html/ch08e.htm
		//WriteConsoleW above works best, but doesn't redirect well out to a file (AP.exe foo.mp4 -t > E:\tags.txt); here is where any redirection gets shunted
		//any utf8 that gets redirected here first gets read from the mpeg-4 file, converted to utf16 (carried on unsigned chars), put into wchar_t, and soon be converted back into utf8 for output to file - some round trip!
		
		int found_codepage = GetConsoleOutputCP();
		int set_codepage = SetConsoleCP(CP_UTF8);
		int charCount = WideCharToMultiByte(CP_UTF8, 0, unicode_out, -1, 0, 0, 0, 0);
		char* szaStr = (char*) malloc(charCount);
		WideCharToMultiByte( CP_UTF8, 0, unicode_out, -1, szaStr, charCount, 0, 0);
		WriteFile(outHandle, szaStr, charCount-1, &dwBytesWritten, 0);
		set_codepage = SetConsoleCP(found_codepage); //restore it back to what it was
		free(szaStr);
	}
	return;
}
#endif

void APar_fprintf_UTF8_data(char* utf8_encoded_data) {
#if defined (_MSC_VER) && defined (UTF16_ENABLED)
	if (GetVersion() & 0x80000000) {
		fprintf(stdout, "%s", utf8_encoded_data); //just printout the raw utf8 bytes (not characters) under pre-NT windows
	} else {
		wchar_t* utf16_data = Convert_multibyteUTF8_to_wchar(utf8_encoded_data);
		fflush(stdout);
		
		APar_unicode_win32Printout(utf16_data);
		
		fflush(stdout);
		free(utf16_data);
		utf16_data = NULL;
	}
#else
	fprintf(stdout, "%s", utf8_encoded_data);
#endif
	return;
}

void APar_PrintUnicodeAssest(char* unicode_string, int asset_length) { //3gp files
	if (memcmp(unicode_string, "\xFE\xFF", 2) == 0 ) { //utf16
		fprintf(stdout, " (utf16)] : ");

#if defined (_MSC_VER)
		if (GetVersion() & 0x80000000) { //pre-NT (pish, thats my win98se, and without unicows support convert utf16toutf8 and output raw bytes)
			unsigned char* utf8_data = Convert_multibyteUTF16_to_UTF8(unicode_string, (asset_length -13) * 6, asset_length-14);
			fprintf(stdout, "%s", utf8_data);
		
			free(utf8_data);
			utf8_data = NULL;
		
		} else {
			wchar_t* utf16_data = Convert_multibyteUTF16_to_wchar(unicode_string, ((asset_length - 16) / 2) + 1, true);
			APar_unicode_win32Printout(utf16_data);
		
			free(utf16_data);
			utf16_data = NULL;
		}
#else
		unsigned char* utf8_data = Convert_multibyteUTF16_to_UTF8(unicode_string, (asset_length-13) * 6, asset_length-14);
		fprintf(stdout, "%s", utf8_data);
		
		free(utf8_data);
		utf8_data = NULL;
#endif
	
	} else { //utf8
		fprintf(stdout, " (utf8)] : ");
		
		APar_fprintf_UTF8_data(unicode_string);
	
	}
	return;
}

//the difference between APar_PrintUnicodeAssest above and APar_SimplePrintUnicodeAssest below is:
//APar_PrintUnicodeAssest contains the entire contents of the atom, NULL bytes and all
//APar_SimplePrintUnicodeAssest contains a purely unicode string (either utf8 or utf16 with BOM)
//and slight output formatting differences

void APar_SimplePrintUnicodeAssest(char* unicode_string, int asset_length, bool print_encoding) { //3gp files
	if (memcmp(unicode_string, "\xFE\xFF", 2) == 0 ) { //utf16
		if (print_encoding) {
			fprintf(stdout, " (utf16): ");
		}

#if defined (_MSC_VER)
		if (GetVersion() & 0x80000000) { //pre-NT (pish, thats my win98se, and without unicows support convert utf16toutf8 and output raw bytes)
			unsigned char* utf8_data = Convert_multibyteUTF16_to_UTF8(unicode_string, asset_length * 6, asset_length-14);
			fprintf(stdout, "%s", utf8_data);
		
			free(utf8_data);
			utf8_data = NULL;
		
		} else {
			wchar_t* utf16_data = Convert_multibyteUTF16_to_wchar(unicode_string, (asset_length / 2) + 1, true);
			APar_unicode_win32Printout(utf16_data);
		
			free(utf16_data);
			utf16_data = NULL;
		}
#else
		unsigned char* utf8_data = Convert_multibyteUTF16_to_UTF8(unicode_string, asset_length * 6, asset_length);
		fprintf(stdout, "%s", utf8_data);
		
		free(utf8_data);
		utf8_data = NULL;
#endif
	
	} else { //utf8
		if (print_encoding) {
			fprintf(stdout, " (utf8): ");
		}
		
		APar_fprintf_UTF8_data(unicode_string);
	
	}
	return;
}

void APar_PrintUserDataAssests() { //3gp files

#if defined (UTF8_ENABLED)
	fprintf(stdout, "\xEF\xBB\xBF"); //Default to output of a UTF-8 BOM (except under win32's WriteConsoleW where it gets transparently eliminated)
#elif defined (_MSC_VER) && defined (UTF16_ENABLED)
	APar_unicode_win32Printout(L"\xEF\xBB\xBF");
#endif

	AtomicInfo udtaAtom = APar_FindAtom("moov.udta", false, false, false, true);
	
	//due to the somewhat lax nature of APar_FindAtom, it will return the first atom

	for (int i=udtaAtom.NextAtomNumber; i < atom_number; i++) {
		if ( parsedAtoms[i].AtomicLevel <= udtaAtom.AtomicLevel ) { //we've gone too far
			//fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
			break;
		}
		if (parsedAtoms[i].AtomicLevel == udtaAtom.AtomicLevel + 1) {
			//fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
			
			uint32_t box = UInt32FromBigEndian(parsedAtoms[i].AtomicName);
			
			char* bitpacked_lang = (char*)malloc(sizeof(char)*3);
			memset(bitpacked_lang, 0, 3);
			
			switch (box) {
				case 0x7469746C : //'titl'
				case 0x64736370 : //'dscp'
				case 0x63707274 : //'cprt'
				case 0x70657266 : //'perf'
				case 0x61757468 : //'auth'
				case 0x676E7265 : //'gnre'
				case 0x616C626D : //'albm'
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					int box_length = parsedAtoms[i].AtomicLength;
					
					uint16_t packed_lang = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + 12);
					unsigned char unpacked_lang[3];
					APar_UnpackLanguage(unpacked_lang, packed_lang);
					
					char* box_data = (char*)malloc(sizeof(char)*box_length);
					memset(box_data, 0, box_length);
					
					APar_readX(box_data, source_file, parsedAtoms[i].AtomicStart + 14, box_length-14); //4bytes length, 4 bytes name, 4 bytes flags, 2 bytes lang
					
					//get tracknumber *after* we read the whole tag; if we have a utf16 tag, it will have a BOM, indicating if we have to search for 2 NULLs or a utf8 single NULL, then the ****optional**** tracknumber
					uint16_t track_num = 1000; //tracknum is a uint8_t, so setting it > 256 means a number wasn't found
					if (box == 0x616C626D) { //'albm' has an *optional* uint8_t at the end for tracknumber; if the last byte in the tag is not 0, then it must be the optional tracknum (or a non-compliant, non-NULL-terminated string). This byte is the length - (14 bytes +1tracknum) or -15
						if (box_data[box_length - 15] != 0) {
							track_num = (uint16_t)box_data[box_length - 15];
							box_data[box_length - 15] = 0; //NULL out the last byte if found to be not 0 - it will impact unicode conversion if it remains
						}
					}
					
					fprintf(stdout, "[lang=%s", unpacked_lang);

					APar_PrintUnicodeAssest(box_data, box_length);
					
					if (box == 0x616C626D && track_num != 1000) {
						fprintf(stdout, "  |  Track: %u", track_num);
					}
					fprintf(stdout, "\n");
					
					free(box_data);
					box_data = NULL;
					
					break;
					}
				
				case 0x72746E67 : //'rtng'
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					int box_length = parsedAtoms[i].AtomicLength;
					char* box_data = (char*)malloc(sizeof(char)*box_length);
					memset(box_data, 0, box_length);
					APar_readX(box_data, source_file, parsedAtoms[i].AtomicStart + 12, 4);
					
					fprintf(stdout, "[Rating Entity=%s", box_data);
					//fprintf(stdout, " Rating Criteria: %u%u%u%u", box_data[4], box_data[5], box_data[6], box_data[7]);
					memset(box_data, 0, box_length);
					APar_readX(box_data, source_file, parsedAtoms[i].AtomicStart + 16, 4);
					fprintf(stdout, " | Criteria=%s", box_data);
					
					uint16_t packed_lang = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + 20);
					unsigned char unpacked_lang[3];
					APar_UnpackLanguage(unpacked_lang, packed_lang);
					fprintf(stdout, " lang=%s", unpacked_lang);
					
					memset(box_data, 0, box_length);
					APar_readX(box_data, source_file, parsedAtoms[i].AtomicStart + 22, box_length-8);
					
					APar_PrintUnicodeAssest(box_data, box_length-8);
					fprintf(stdout, "\n");
					break;
					}
				
				case 0x636C7366 : //'clsf'
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					int box_length = parsedAtoms[i].AtomicLength;
					char* box_data = (char*)malloc(sizeof(char)*box_length);
					memset(box_data, 0, box_length);
					APar_readX(box_data, source_file, parsedAtoms[i].AtomicStart + 12, box_length-12); //4bytes length, 4 bytes name, 4 bytes flags, 2 bytes lang
					
					fprintf(stdout, "[Classification Entity=%s", box_data);
					fprintf(stdout, " | Index=%u", UInt16FromBigEndian(box_data + 4) );
					
					uint16_t packed_lang = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + 18);
					unsigned char unpacked_lang[3];
					APar_UnpackLanguage(unpacked_lang, packed_lang);
					fprintf(stdout, " lang=%s", unpacked_lang);
						
					APar_PrintUnicodeAssest(box_data +8, box_length-8);
					fprintf(stdout, "\n");
					break;
					}

				case 0x6B797764 : //'kywd'
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					uint32_t box_length = parsedAtoms[i].AtomicLength;
					uint32_t box_offset = 12;
					
					uint16_t packed_lang = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + box_offset);
					box_offset+=2;
					
					unsigned char unpacked_lang[3];
					APar_UnpackLanguage(unpacked_lang, packed_lang);
					
					uint8_t keyword_count = APar_read8(source_file, parsedAtoms[i].AtomicStart + box_offset);
					box_offset++;
					fprintf(stdout, "[Keyword count=%u", keyword_count);
					fprintf(stdout, " lang=%s]", unpacked_lang);
					
					char* keyword_data = (char*)malloc(sizeof(char)* box_length * 2);
					
					for(uint8_t x = 1; x <= keyword_count; x++) {
						memset(keyword_data, 0, box_length * 2);
						uint8_t keyword_length = APar_read8(source_file, parsedAtoms[i].AtomicStart + box_offset);
						box_offset++;
						
						APar_readX(keyword_data, source_file, parsedAtoms[i].AtomicStart + box_offset, (uint32_t)keyword_length);
						box_offset+=keyword_length;
						APar_SimplePrintUnicodeAssest(keyword_data, keyword_length, true);
					}
					free(keyword_data);
					keyword_data = NULL;
					
					fprintf(stdout, "\n");
					break;
					}
					
				case 0x6C6F6369 : //'loci' aka The Most Heinous Metadata Atom Every Invented - decimal meters? fictional location? Astromical Body? Say I shoot it on the International Space Station? That isn't a Astronimical Body. And 16.16 alt only goes up to 20.3 miles (because of negatives, its really 15.15) & the ISS is at 230 miles. Oh, pish.... what ever shall I do? I fear I am on the horns of a dilema.
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					uint32_t box_length = parsedAtoms[i].AtomicLength;
					uint32_t box_offset = 12;
					
					uint16_t packed_lang = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + box_offset);
					box_offset+=2;
					
					unsigned char unpacked_lang[3];
					APar_UnpackLanguage(unpacked_lang, packed_lang);
					
					char* location_string = (char*)malloc(sizeof(char)* box_length);
					APar_readX(location_string, source_file, parsedAtoms[i].AtomicStart + box_offset, box_length);
					fprintf(stdout, "[lang=%s] ", unpacked_lang);
					
					//the length of the location string is unknown (max is box lenth), but the long/lat/alt/body/notes needs to be retrieved.
					//test if the location string is utf16; if so search for 0x0000 (or if utf8, find the first NULL).
					if ( memcmp(location_string, "\xFE\xFF", 2) == 0 ) {
						uint32_t new_tally = widechar_len(location_string, box_length);
						box_offset+= 2 * widechar_len(location_string, box_length) + 2; //*2 for utf16 (double-byte); +2 for the terminating NULL
						fprintf(stdout, "(utf16) ");
					} else {
						fprintf(stdout, "(utf8) ");
						box_offset+= strlen(location_string) + 1; //+1 for the terminating NULL
					}
					fprintf(stdout, "Location: ");
					APar_SimplePrintUnicodeAssest(location_string, box_length, false);
					
					uint8_t location_role = APar_read8(source_file, parsedAtoms[i].AtomicStart + box_offset);
					box_offset++;
					switch(location_role) {
						case 0 : {
							fprintf(stdout, " (Role: shooting location) ");
							break;
						}
						case 1 : {
							fprintf(stdout, " (Role: real location) ");
							break;
						}
						case 2 : {
							fprintf(stdout, " (Role: fictional location) ");
							break;
						}
						default : {
							fprintf(stdout, " (Role: [reserved]) ");
							break;
						}
					}
					
					char* float_buffer = (char*)malloc(sizeof(char)* 5);
					memset(float_buffer, 0, 5);
					
					fprintf(stdout, "[Long %lf", fixed_point_16x16bit_to_double( APar_read32(float_buffer, source_file, parsedAtoms[i].AtomicStart + box_offset) ) );
					box_offset+=4;
					fprintf(stdout, " Lat %lf", fixed_point_16x16bit_to_double( APar_read32(float_buffer, source_file, parsedAtoms[i].AtomicStart + box_offset) ) );
					box_offset+=4;
					fprintf(stdout, " Alt %lf ", fixed_point_16x16bit_to_double( APar_read32(float_buffer, source_file, parsedAtoms[i].AtomicStart + box_offset) ) );
					box_offset+=4;
					free(float_buffer);
					float_buffer = NULL;
					
					if (box_offset < box_length) {
						fprintf(stdout, " Body: ");
						APar_SimplePrintUnicodeAssest(location_string+box_offset-14, box_length-box_offset, false);
						if ( memcmp(location_string+box_offset-14, "\xFE\xFF", 2) == 0 ) {
							uint32_t new_tally = widechar_len(location_string+box_offset-14, box_length-box_offset);
							box_offset+= 2 * widechar_len(location_string+box_offset-14, box_length-box_offset) + 2; //*2 for utf16 (double-byte); +2 for the terminating NULL
						} else {
							box_offset+= strlen(location_string+box_offset-14) + 1; //+1 for the terminating NULL
						}
					}
					fprintf(stdout, "]");
					
					if (box_offset < box_length) {
						fprintf(stdout, " Notes: ");
						APar_SimplePrintUnicodeAssest(location_string+box_offset-14, box_length-box_offset, false);
					}
					
					fprintf(stdout, "\n");
					break;
					}
					
				case 0x79727263 : //'yrrc'
					{
					fprintf(stdout, "User data \"%s\" ", parsedAtoms[i].AtomicName);
					
					uint16_t recording_year = APar_read16(bitpacked_lang, source_file, parsedAtoms[i].AtomicStart + 12);
					fprintf(stdout, ": %u\n", recording_year);
					break;
					}
				
				default : 
					{
					break;
					}
			}
		}
	}
	return;
}

void APar_ExtractAAC_Artwork(short this_atom_num, char* pic_output_path, short artwork_count) {
	char *base_outpath=(char *)malloc(sizeof(char)*MAXPATHLEN+1);
	memset(base_outpath, 0, MAXPATHLEN +1);
	
	strcpy(base_outpath, pic_output_path);
	strcat(base_outpath, "_artwork");
	sprintf(base_outpath, "%s_%d", base_outpath, artwork_count);
	
	char* art_payload = (char*)malloc( sizeof(char) * (parsedAtoms[this_atom_num].AtomicLength-16) +1 );	
	memset(art_payload, 0, (parsedAtoms[this_atom_num].AtomicLength-16) +1 );
			
	fseek(source_file, parsedAtoms[this_atom_num].AtomicStart+16, SEEK_SET);
	fread(art_payload, 1, parsedAtoms[this_atom_num].AtomicLength-16, source_file);
	
	char* suffix = (char *)malloc(sizeof(char)*5);
	memset(suffix, 0, sizeof(char)*5);
	
	if (strncmp((char *)art_payload, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {//casts uchar* to char* (2)
				suffix = ".png";
	}	else if (strncmp((char *)art_payload, "\xFF\xD8\xFF\xE0", 4) == 0) {//casts uchar* to char* (2)
				suffix = ".jpg";
	}
	
	strcat(base_outpath, suffix);
	
	FILE *outfile = APar_OpenFile(base_outpath, "wb");
	if (outfile != NULL) {
		fwrite(art_payload, (size_t)(parsedAtoms[this_atom_num].AtomicLength-16), 1, outfile);
		fclose(outfile);
		fprintf(stdout, "Extracted artwork to file: ");
		APar_fprintf_UTF8_data(base_outpath);
		fprintf(stdout, "\n");
	}
	free(base_outpath);
	free(art_payload);
  return;
}

void APar_ExtractDataAtom(int this_atom_number) {
	if ( source_file != NULL ) {
		AtomicInfo thisAtom = parsedAtoms[this_atom_number];
		char* genre_string;
		char* parent_atom_name=(char*)malloc( sizeof(char)*5);
		memset(parent_atom_name, 0, sizeof(char)*5);
		
		AtomicInfo parent_atom_stats = parsedAtoms[this_atom_number-1];
		parent_atom_name = parent_atom_stats.AtomicName;
		//fprintf(stdout, "\t\t\t parent atom %s\n", parent_atom_name);

		uint32_t min_atom_datasize = 12;
		uint32_t atom_header_size = 16;
		
		if (thisAtom.uuidAtomType) {
			min_atom_datasize += 4;
			atom_header_size += 4;
		}
 
		if (thisAtom.AtomicLength > min_atom_datasize ) {
			char* data_payload = (char*)malloc( sizeof(char) * (thisAtom.AtomicLength - atom_header_size +1) );
			memset(data_payload, 0, sizeof(char) * (thisAtom.AtomicLength - atom_header_size +1) );
			
			fseek(source_file, thisAtom.AtomicStart + atom_header_size, SEEK_SET);
			fread(data_payload, 1, thisAtom.AtomicLength - atom_header_size, source_file);

			if (thisAtom.AtomicDataClass == AtomicDataClass_Text) {
				if (thisAtom.AtomicLength < (atom_header_size + 4) ) {
					//tvnn was showing up with 4 chars instead of 3; easier to null it out for now
					data_payload[thisAtom.AtomicLength - atom_header_size] = '\00';
				}
				
				APar_fprintf_UTF8_data(data_payload);
				fprintf(stdout,"\n");
				
			} else {
			
				char* primary_number_data = (char*)malloc( sizeof(char) * 4 );
				uint32_t primary_number = 0;
				uint32_t secondary_OF_number = 0;
				
				for (int i=0; i < 4; i++) {
							primary_number_data[i] = '\0';
				}
				
				if ( (strncmp(parent_atom_name, "trkn", 4) == 0) || (strncmp(parent_atom_name, "disk", 4) == 0) ) {
					char* secondary_OF_number_data = (char*)malloc( sizeof(char) * 4 );
					secondary_OF_number_data[0] = '\00';
					secondary_OF_number_data[1] = '\00';
					secondary_OF_number_data[2] = '\00';
					secondary_OF_number_data[3] = data_payload[5];
					secondary_OF_number = UInt32FromBigEndian(secondary_OF_number_data);
					free(secondary_OF_number_data);
					secondary_OF_number_data = NULL;
					
					for (int i=0; i < 4; i++) {
						primary_number_data[i] = data_payload[i]; //we want the 4 byte 'atom' in data [4,5,6,7]
					}
					primary_number = UInt32FromBigEndian(primary_number_data);
					
					if (secondary_OF_number != 0) {
						fprintf(stdout, "%u of %u\n", primary_number, secondary_OF_number);
					} else {
						fprintf(stdout, "%u\n", primary_number);
					}
					
				} else if (strncmp(parent_atom_name, "gnre", 4) == 0) {
					if ( thisAtom.AtomicLength - atom_header_size < 3 ) { //oh, a 1byte int for genre number
						for (int i=0; i < 2; i++) {
							primary_number_data[i+2] = data_payload[i];
							GenreIntToString(&genre_string, UInt32FromBigEndian(primary_number_data) );
						}
						fprintf(stdout,"%s\n", genre_string);
					}
					
				}	else if (strncmp(parent_atom_name, "tmpo", 4) == 0) {
					primary_number_data[2] = data_payload[0];
					primary_number_data[3] = data_payload[1];
					primary_number = UInt32FromBigEndian(primary_number_data);
					fprintf(stdout, "%u\n", primary_number);

				} else if ( (strncmp(parent_atom_name, "cpil", 4) == 0) || (strncmp(parent_atom_name, "pcst", 4) == 0) ) {
					primary_number_data[3] = data_payload[0];
					primary_number = UInt32FromBigEndian(primary_number_data);
					if (primary_number == 1) {
						fprintf(stdout, "true\n");
					} else {
						fprintf(stdout, "false\n");
					}
					
				} else if (strncmp(parent_atom_name, "stik", 4) == 0) { //no idea what 'stik' stands for; the State of the Union address came as 0x02
					primary_number_data[3] = data_payload[0];
					primary_number = UInt32FromBigEndian(primary_number_data);
					if (primary_number == 0) {
						fprintf(stdout, "Movie\n");
					} else if (primary_number == 1) {
						fprintf(stdout, "Normal\n");
					} else if (primary_number == 5) {
						fprintf(stdout, "Whacked Bookmarkable (only plays once per iTunes session)\n");
					} else if (primary_number == 6) {
						fprintf(stdout, "Music Video\n");
					} else if (primary_number == 9) {
						fprintf(stdout, "Short Film\n");
					} else if (primary_number == 10) {
						fprintf(stdout, "TV Show\n");
					} else {
						fprintf(stdout, "Unknown value: %u\n", primary_number);
					}

				} else if (strncmp(parent_atom_name, "rtng", 4) == 0) {
					primary_number_data[3] = data_payload[0];
					primary_number = UInt32FromBigEndian(primary_number_data);
					if (primary_number == 2) {
						fprintf(stdout, "Clean Lyrics\n");
					} else if (primary_number != 0 ) {
						fprintf(stdout, "Explicit Lyrics\n");
					} else {
						fprintf(stdout, "Inoffensive\n");
					}
					
				} else if ( (strncmp(parent_atom_name, "purl", 4) == 0) || (strncmp(parent_atom_name, "egid", 4) == 0) ) {
					fprintf(stdout,"%s\n", data_payload);
				
				} else if ( (strncmp(parent_atom_name, "tvsn", 4) == 0) || (strncmp(parent_atom_name, "tves", 4) == 0) ) {
					  for (int i=0; i < 4; i++) {
						  primary_number_data[i] = data_payload[i]; 
					  }
					  primary_number = UInt32FromBigEndian(primary_number_data);
						fprintf(stdout, "%u\n", primary_number);
						
				} else {
					  
					fprintf(stdout, "hex 0x");
					for( int hexx = 1; hexx <= (int)(thisAtom.AtomicLength - atom_header_size); ++hexx) {
						fprintf(stdout,"%02X", (uint8_t)data_payload[hexx-1]);
						if ((hexx % 4) == 0 && hexx >= 4) {
							fprintf(stdout," ");
						}
						if ((hexx % 16) == 0 && hexx > 16) {
							fprintf(stdout,"\n\t\t\t");
						}
						if (hexx == (int)(thisAtom.AtomicLength - atom_header_size) ) {
							fprintf(stdout,"\n");
						}
					}
						
				}
					
				free(primary_number_data);
				primary_number_data=NULL;
				free(data_payload);
				data_payload = NULL;
			}
		}
		free(parent_atom_name);
		parent_atom_name=NULL;
	}
	return;
}

void APar_PrintDataAtoms(const char *path, bool extract_pix, char* pic_output_path) {

#if defined (UTF8_ENABLED)
	fprintf(stdout, "\xEF\xBB\xBF"); //Default to output of a UTF-8 BOM (except under win32's WriteConsoleW where it gets transparently eliminated)
#elif defined (_MSC_VER) && defined (UTF16_ENABLED)
	APar_unicode_win32Printout(L"\xEF\xBB\xBF");
#endif

	short artwork_count=0;
	char* atom_name = (char*)malloc(sizeof(char)*10);
	char* parent_atom = (char*)malloc(sizeof(char)*10);

	for (int i=0; i < atom_number; i++) { 
		AtomicInfo thisAtom = parsedAtoms[i];
		memset(atom_name, 0, sizeof(char)*10);

		strncpy(atom_name, thisAtom.AtomicName, 4);
		if ( strncmp(atom_name, "data", 4) == 0 ) {
			memset(parent_atom, 0, sizeof(char)*10);
			
			AtomicInfo parent = parsedAtoms[ APar_FindParentAtom(i, thisAtom.AtomicLevel) ];
			strncpy(parent_atom, parent.AtomicName, 4);
			
			if ( (thisAtom.AtomicDataClass == AtomicDataClass_UInteger ||
            thisAtom.AtomicDataClass == AtomicDataClass_Text || 
            thisAtom.AtomicDataClass == AtomicDataClass_UInt8_Binary) && !extract_pix ) {
				if (strncmp(parent_atom, "----", 4) == 0) {
					if (strncmp(parsedAtoms[i-1].AtomicName, "name", 4) == 0) {
						char* iTunes_internal_tag = (char*)malloc(sizeof(char)*20); //20 seems good enough
						memset(iTunes_internal_tag, 0, sizeof(char)*20);
					
						fseek(source_file, parsedAtoms[parent.AtomicNumber + 2].AtomicStart + 12, SEEK_SET); //'name' atom is the 2nd child
						fread(iTunes_internal_tag, 1, parsedAtoms[parent.AtomicNumber + 2].AtomicLength - 12, source_file);
					
						fprintf(stdout, "Atom \"%s\" [%s] contains: ", parent_atom, iTunes_internal_tag);
						APar_ExtractDataAtom(i);
					}
				
				} else if (strncmp(parent_atom,"covr", 4) == 0) { //libmp4v2 doesn't properly set artwork with the right flags (its all 0x00)
					artwork_count++;
					
				} else {
					char* parent_duplicate = strdup(parent_atom);
					//converts iso8859 © in '©ART' to a 2byte utf8 © glyph; replaces libiconv conversion
					isolat1ToUTF8((unsigned char*)parent_atom, 10, (unsigned char*)parent_duplicate, 4);

#if defined (_MSC_VER) && defined (UTF16_ENABLED)
					fprintf(stdout, "Atom \"");
					APar_fprintf_UTF8_data(parent_atom);
					fprintf(stdout, "\" contains: ");
#else
					fprintf(stdout, "Atom \"%s\" contains: ", parent_atom);
#endif
					APar_ExtractDataAtom(i);
					free(parent_duplicate);
					parent_duplicate = NULL;
				}
								
			} else if (strncmp(parent_atom,"covr", 4) == 0) {
				artwork_count++;
				if (extract_pix) {
					APar_ExtractAAC_Artwork(thisAtom.AtomicNumber, pic_output_path, artwork_count);
				}
			}
		} else if (thisAtom.uuidAtomType) {
			char* uuid_duplicate = strdup(atom_name);
			//converts iso8859 © in '©foo' to a 2byte utf8 © glyph; replaces libiconv conversion
			isolat1ToUTF8((unsigned char*)atom_name, 10, (unsigned char*)uuid_duplicate, 4);

			if (thisAtom.AtomicDataClass == AtomicDataClass_Text && !pic_output_path) {
#if defined (_MSC_VER) && defined (UTF16_ENABLED)
				fprintf(stdout, "Atom uuid=\"");
				APar_fprintf_UTF8_data(atom_name);
				fprintf(stdout, "\" contains: ");
#else
				fprintf(stdout, "Atom uuid=\"%s\" contains: ", atom_name);
#endif
				APar_ExtractDataAtom(i);
			}
			free(uuid_duplicate);
			uuid_duplicate = NULL;
		}
	}
	free(atom_name);
	free(parent_atom);
	atom_name=NULL;
	parent_atom=NULL;
	
	if (artwork_count != 0) {
		if (artwork_count == 1) {
			fprintf(stdout, "Atom \"covr\" contains: %i piece of artwork\n", artwork_count);
		} else {
			fprintf(stdout, "Atom \"covr\" contains: %i pieces of artwork\n", artwork_count);
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//               Generic (parsing/reading, writing) Atom creation                    //
///////////////////////////////////////////////////////////////////////////////////////

bool APar_DetermineMain_mdat() {
	bool last_mdat_is_larger = false;
		
 	for (int i=0; i <= atom_number; i++) {
		AtomicInfo thisAtom = parsedAtoms[i];
		if ( (thisAtom.AtomicLevel == 1) && (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) ) {
			if (parsedAtoms[atom_number].AtomicLength >= thisAtom.AtomicLength ) {
				last_mdat_is_larger = true;
			}
		}
	}	

	return last_mdat_is_larger;
}

void APar_AtomizeFileInfo(AtomicInfo &thisAtom, uint32_t Astart, uint32_t Alength, uint64_t Aextendedlength, char* Astring,
													short Alevel, int Aclass, int NextAtomNum, bool uuid_type) {
	static bool passed_mdat = false;
	
	thisAtom.AtomicStart = Astart;
	thisAtom.AtomicLength = Alength;
	thisAtom.AtomicLengthExtended = Aextendedlength;
	
	thisAtom.AtomicName = (char*)malloc(sizeof(char)*5);
	memset(thisAtom.AtomicName, 0, sizeof(char)*5);
	
	strcpy(thisAtom.AtomicName, Astring);
	thisAtom.AtomicNumber = atom_number;
	thisAtom.AtomicLevel = Alevel;
	thisAtom.AtomicDataClass = Aclass;
	thisAtom.uuidAtomType = uuid_type;
	thisAtom.stsd_codec = 0;
	
	//set the next atom number of the PREVIOUS atom (we didn't know there would be one until now); this is our default normal mode
	if (( NextAtomNum == 0 ) && ( atom_number !=0 )) {
		parsedAtoms[atom_number-1].NextAtomNumber = atom_number;
	}
	thisAtom.NextAtomNumber=0; //this could be the end... (we just can't quite say until we find another atom)
	
	//handle our oddballs
	if (strncmp(Astring, "meta", 4) == 0) {
		thisAtom.AtomicDataClass = AtomicDataClass_UInteger;
	}
	
	if (strncmp(Astring, "mdat", 4) == 0) {
		passed_mdat = true;
	}

	if (!passed_mdat && Alevel == 1) {
		bytes_before_mdat += Alength; //this value gets used during FreeFree (for removed_bytes_tally) & chunk offset calculations
	}
			
	atom_number++; //increment to the next AtomicInfo array
	
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                                 Atom Tree                                         //
///////////////////////////////////////////////////////////////////////////////////////

//this function reflects the atom tree as it stands in memory accurately (so I hope).
void APar_PrintAtomicTree() {

#if defined (UTF8_ENABLED)
	fprintf(stdout, "\xEF\xBB\xBF"); //Default to output of a UTF-8 BOM (except under win32's WriteConsoleW where it gets transparently eliminated)
#elif defined (_MSC_VER) && defined (UTF16_ENABLED)
	APar_unicode_win32Printout(L"\xEF\xBB\xBF");
#endif

	char* tree_padding = (char*)malloc(sizeof(char)*126); //for a 25-deep atom tree (4 spaces per atom)+single space+term.
	uint32_t freeSpace = 0;
	uint32_t mdatData = 0;
	short thisAtomNumber = 0;
	char* atom_name = (char*)malloc(sizeof(char)*10);
	bool foobar_noncompliant_tags_present = false; //with any luck, it should stay that way
		
	//loop through each atom in the struct array (which holds the offset info/data)
 	while (true) {
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber];
		memset(tree_padding, 0, sizeof(char)*126);
		memset(atom_name, 0, sizeof(char)*10);
		
		strncpy(atom_name, thisAtom.AtomicName, 4);
		char* atom_duplicate = strdup(atom_name);
		isolat1ToUTF8((unsigned char*)atom_name, 10, (unsigned char*)atom_duplicate, 4); //converts iso8859 © in '©ART' to a 2byte utf8 © glyph
		free(atom_duplicate);
		atom_duplicate = NULL;
		
		strcpy(tree_padding, "");
		if ( thisAtom.AtomicLevel != 1 ) {
			for (int pad=1; pad < thisAtom.AtomicLevel; pad++) {
				strcat(tree_padding, "    "); // if the atom depth is over 1, then add spaces before text starts to form the tree
			}
			strcat(tree_padding, " "); // add a single space
		}
		
		if (thisAtom.AtomicLength == 0) {
			fprintf(stdout, "%sAtom %s @ %u of size: %u (%u*), ends @ %u\n", tree_padding, atom_name, thisAtom.AtomicStart, ( (uint32_t)file_size - thisAtom.AtomicStart), thisAtom.AtomicLength, (uint32_t)file_size );
			fprintf(stdout, "\t\t\t (*)denotes length of atom goes to End-of-File\n");
		
		} else if (thisAtom.AtomicLength == 1) {
			fprintf(stdout, "%sAtom %s @ %u of size: %llu (^), ends @ %llu\n", tree_padding, atom_name, thisAtom.AtomicStart, thisAtom.AtomicLengthExtended, (thisAtom.AtomicStart + thisAtom.AtomicLengthExtended) );
			fprintf(stdout, "\t\t\t (^)denotes a 64-bit atom length\n");
			
		} else if (thisAtom.uuidAtomType) {
			fprintf(stdout, "%sAtom uuid=%s @ %u of size: %u, ends @ %u\n", tree_padding, atom_name, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
		} else {
#if defined (_MSC_VER) && defined (UTF16_ENABLED)
			fprintf(stdout, "%sAtom ", tree_padding);
			APar_fprintf_UTF8_data(atom_name);
			fprintf(stdout, " @ %u of size: %u, ends @ %u\n", thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
#else
			fprintf(stdout, "%sAtom %s @ %u of size: %u, ends @ %u\n", tree_padding, atom_name, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
#endif
		}
		
		//simple tally & percentage of free space info
		if (strncmp(thisAtom.AtomicName, "free", 4) == 0) {
			freeSpace=freeSpace+thisAtom.AtomicLength;
		}
		//this is where the *raw* audio/video file is, the rest is container-related fluff.
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLength > 100) ) {
			mdatData = thisAtom.AtomicLength;
		} else if ( strncmp(thisAtom.AtomicName, "mdat", 4) == 0 && thisAtom.AtomicLength == 0 ) { //mdat.length = 0 = ends at EOF
			mdatData = (uint32_t)file_size - thisAtom.AtomicStart;
		} else if (strncmp(thisAtom.AtomicName, "mdat", 4) == 0 && thisAtom.AtomicLengthExtended != 0 ) {
			mdatData = thisAtom.AtomicLengthExtended; //this is still adding a (limited) uint64_t into a uint32_t
			
		} else if (strncmp(thisAtom.AtomicName, "tags", 4) == 0) { // hmmm, I must have missed the 'tags' atom in the ISO spec
			AtomicInfo FUBAR_tag_atom = APar_FindAtom("moov.udta.tags", false, false, false, true);
			if (FUBAR_tag_atom.AtomicNumber != 0) {
				foobar_noncompliant_tags_present = true;
			}
		}
		
		if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) {
			break;
		} else {
			thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
		}
	}
	
	if (foobar_noncompliant_tags_present) {
		fprintf(stdout, "\n"); 
		fprintf(stdout, "AtomicParsley warning: this file was tagged by foobar2000 with non-compliant tags\n");
		fprintf(stdout, "\tmore atoms may be present, but foobar2000 writes invalid atom structures.\n");
		fprintf(stdout, "\n"); 
	}
		
	fprintf(stdout, "------------------------------------------------------\n");
	fprintf(stdout, "Total size: %i bytes; %i atoms total. ", (int)file_size, atom_number-1);
	ShowVersionInfo();
	fprintf(stdout, "Media data: %u bytes; %u bytes all other atoms (%2.3f%% atom overhead).\n", 
												mdatData, (uint32_t)(file_size - mdatData), (float)(file_size - mdatData)/(float)file_size * 100 );
	fprintf(stdout, "Total free atoms: %u bytes; %2.3f%% waste.\n", freeSpace, (float)freeSpace/(float)file_size * 100 );
	fprintf(stdout, "------------------------------------------------------\n");
	
	free(tree_padding);
	tree_padding = NULL;
	free(atom_name);
	atom_name=NULL;
		
	return;
}

void APar_SimpleAtomPrintout() { //loop through each atom in the struct array (which holds the offset info/data)

#if defined (UTF8_ENABLED)
	fprintf(stdout, "\xEF\xBB\xBF"); //Default to output of a UTF-8 BOM (except under win32's WriteConsoleW where it gets transparently eliminated)
#elif defined (_MSC_VER) && defined (UTF16_ENABLED)
	APar_unicode_win32Printout(L"\xEF\xBB\xBF");
#endif

 	for (int i=0; i < atom_number; i++) { 
		AtomicInfo thisAtom = parsedAtoms[i]; 
		
		fprintf(stdout, "%i  -  Atom \"%s\" (level %i) has next atom at #%i\n", i, thisAtom.AtomicName, thisAtom.AtomicLevel, thisAtom.NextAtomNumber);
	}
	fprintf(stdout, "Total of %i atoms.\n", atom_number-1);
}

///////////////////////////////////////////////////////////////////////////////////////
//                      File scanning & atom parsing                                 //
///////////////////////////////////////////////////////////////////////////////////////

short APar_GetCurrentAtomDepth(uint32_t atom_start, uint32_t atom_length) {
	short level = 1;
	for (int i = 0; i < atom_number; i++) {
		AtomicInfo thisAtom = parsedAtoms[i];
		if (atom_start == (thisAtom.AtomicStart + thisAtom.AtomicLength) ) {
			return thisAtom.AtomicLevel;
		} else {
			if ( (atom_start < thisAtom.AtomicStart + thisAtom.AtomicLength) && (atom_start > thisAtom.AtomicStart) ) {
				level++;
			}
		}
	}
	return level;
}

bool APar_TestforChildAtom(char *fileData, uint32_t sizeofParentAtom, char* atom) {
	if ( strncmp(atom, "data", 4) == 0  || strncmp(atom, "mdat", 4) == 0 || strncmp(atom, "tfhd", 4) == 0) {
		return false;
	}
	
	char *childAtomLength = (char *)malloc(sizeof(char)*5);
	memset(childAtomLength, 0, sizeof(char)*5);

	for (int i=0; i < 4; i++) {
		childAtomLength[i] = fileData[i+8]; //we want the 4 byte 'atom' in data [4,5,6,7]
	}
	
	uint32_t sizeofChild = UInt32FromBigEndian(childAtomLength);
	
	if ( sizeofChild > 8 && sizeofChild < sizeofParentAtom ) {
		free(childAtomLength);
		childAtomLength=NULL;
		return true;
	} else {
		free(childAtomLength);
		childAtomLength=NULL;
	  return false;
	}
}

short APar_DetermineDataType(char* atom, bool uuid_type) {
	
	char* data_type = (char*)malloc(sizeof(char)*5);
	memset(data_type, 0, sizeof(char)*5);
	
	int data_offset = 8;
	if (uuid_type) {
		data_offset = 12;
	}
	for (int i=0; i < 4; i++) {
		data_type[i] = atom[i+data_offset]; //we want the 1 byte code (01, 15) following the data atom to determine type (text, binary)
	}
	uint32_t type_of_data = UInt32FromBigEndian(data_type);
	
	free(data_type);
	data_type=NULL;
	return (int)type_of_data;
}

void APar_IdentifyBrand(char* file_brand ) {
	uint32_t brand = UInt32FromBigEndian(file_brand);
	switch (brand) {
		//what ISN'T supported
		case 0x71742020 : //'qt  '
			fprintf(stdout, "AtomicParsley error: Quicktime movie files are not supported.\n");
			exit(2);
			break;
			
		//3gp-style metadata; what about 'kddi' EZMovie crap?
		
		case 0x33673261 : //'3g2a' 3GPP2 release 0
		case 0x33673262 : //'3g2b' 3GPP2 release A
			metadata_style = THIRD_GEN_PARTNER_VER2;
			break;
			
		case 0x33677034 : //'3gp4'
		case 0x33677035 : //'3gp5' //'albm' album tag was added in Release6, so it shouldn't be added to a 3gp5 or 3gp4 branded file.
			metadata_style = THIRD_GEN_PARTNER;
			break;
		
		case 0x33677036 : //'3gp6'
		
		case 0x33677236 : //'3gr6' progressive
		case 0x33677336 : //'3gs6' streaming
		case 0x33676536 : //'3ge6' extended presentations (jpeg images)
		case 0x33676736 : //'3gg6' general (not yet suitable; superset)
		
		case 0x6D6D7034 : //'mmp4' - not sure if it supports album or not

			metadata_style = THIRD_GEN_PARTNER_VER1_REL6;
			break;
			
		//what IS supported for iTunes-style metadata
		case 0x4D534E56 : //'MSNV'  (PSP)
		case 0x4D344120 : //'M4A '
		case 0x4D344220 : //'M4B '
		case 0x4D345620 : //'M4V '
		case 0x6D703432 : //'mp42'
		case 0x6D703431 : //'mp41'
		case 0x69736F6D : //'isom'
		case 0x69736F32 : //'iso2'
		case 0x61766331 : //'avc1'
			metadata_style = ITUNES_STYLE;
			break;
		
		//other lesser unsupported brands; http://www.mp4ra.org/filetype.html
		default :
			fprintf(stdout, "AtomicParsley error: unsupported MPEG-4 file brand found '%s'\n", file_brand);
			exit(2);
			break;
	
	}

	return;
}

void APar_Extract_stsd_codec(FILE* file, uint32_t midJump) {
	char *codec_data = (char *) malloc(12 +1);
	memset(codec_data, 0, 12 + 1);
	fseek(file, midJump, SEEK_SET);
	fread(codec_data, 1, 12, file);
	parsedAtoms[atom_number-1].stsd_codec = UInt32FromBigEndian( extractAtomName(codec_data, 1) );
	
	free(codec_data);
  return;
}

void APar_Parse_stsd_Atoms(FILE* file, uint32_t midJump, uint32_t drmLength) {
	//fprintf(stdout,"---> drms atom %s begins #: %u \t to %u\n", parsedAtoms[atom_number-1].AtomicName, midJump, drmLength);
	//stsd atom carrys data (8bytes )
	short stsd_entry_atom_number = atom_number-1;
	uint32_t stsd_entry_pos = midJump;
	char *data = (char *) malloc(12 + 1);
	memset(data, 0, 12 + 1);
	
	uint32_t interDataSize = 0;
	uint32_t stsd_progress = 16;
	short atomLevel = generalAtomicLevel+1;
	bool uuid_atom = false;
	fseek(file, midJump, SEEK_SET);
	
	//this now works anywhere the stsd atom is situated
	while ( stsd_progress < drmLength) {
		fread(data, 1, 12, file);
		char *atom = extractAtomName(data, 1);
		
		if (memcmp(atom, "uuid", 4) == 0) {
			atom = extractAtomName(data, 2);
			uuid_atom = true;
		}

		interDataSize = UInt32FromBigEndian(data);
		
		if ( interDataSize > drmLength || interDataSize < 8) {
			break; //we only get here if there is some oddball atom here under stsd that has a dataclass
		}
				
		APar_AtomizeFileInfo(parsedAtoms[atom_number], midJump, interDataSize, 0, atom, atomLevel, -1, 0, uuid_atom);
		
		if (strncmp(atom, "drms", 4) == 0) {
			//this needs to be done in order to maintain integrity of modified files
			parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28 + 1);
			memset(parsedAtoms[atom_number-1].AtomicData, 0, sizeof(char)*28 + 1);
			
			fseek (file, midJump+8, SEEK_SET);
			fread(parsedAtoms[atom_number-1].AtomicData, 1, 28, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_UInteger;
			
			midJump += 36; //drms is so odd.... it contains data so it should *NOT* have any child atoms, and yet...
										 // 983bytes (and the next atom 36 bytes away) says that it *IS* a parent atom.... very odd indeed.
			stsd_progress += 36;		
			atomLevel++;
			
		} else if (strncmp(atom, "drmi", 4) == 0) {
		
			parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*78 + 1); //74
			memset(parsedAtoms[atom_number-1].AtomicData, 0, sizeof(char)*78 + 1);
			
			fseek (file, midJump+8, SEEK_SET); //12
			fread(parsedAtoms[atom_number-1].AtomicData, 1, 78, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
			midJump += 86;
			stsd_progress += 86;	
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_UInteger;
			atomLevel++;
			
		} else if ( (strncmp(atom, "mp4a", 4) == 0) || ( (strncmp(atom, "alac", 4) == 0) && (atomLevel == 7) ) ) {
				parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28 + 1);
				memset(parsedAtoms[atom_number-1].AtomicData, 0, sizeof(char)*28 + 1);
				
				fseek (file, midJump+8, SEEK_SET); //fseek to just after the atom name
				fread(parsedAtoms[atom_number-1].AtomicData, 1, 28, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
				parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_UInteger;
				atomLevel++;
				midJump += 36;
				stsd_progress += 36;
		
		} else if (strncmp(atom, "skcr", 4) == 0) { //it has a "0x00 00 00 10, so it will appear to have a child
			midJump += interDataSize;
			stsd_progress += interDataSize;	
			
		//If I were on a mission, this is the place I would also make an exception for "stsd.drms.sinf.schi.righ" which contains "evID","plat","aver","tran" & "medi"
		//but we aren't, and so we just copy it whole. Unlike "skcr" where accomidations DON'T have to be made to write it, "righ" children atoms would need... coddling
		
		} else if ( APar_TestforChildAtom(data, interDataSize, atom) ) { 
			midJump += 8; //skip a head a grand total of... 8 *WHOLE* bytes - what progress!
			stsd_progress += 8;	
			atomLevel++; 
			
		} else {
			midJump += interDataSize;
			stsd_progress += interDataSize;	
			atomLevel = APar_GetCurrentAtomDepth(midJump, interDataSize);
		} 
		
		if (stsd_progress >=  drmLength) {
			break; //we have completed stsd parsing
		}

		free(atom);
		atom=NULL;
		fseek(file, midJump, SEEK_SET);
	}

	parsedAtoms[stsd_entry_atom_number].AtomicData = (char *)malloc(sizeof(char)*4 + 1);
	memset(parsedAtoms[stsd_entry_atom_number].AtomicData, 0, sizeof(char)*4 + 1);
	
	fseek(file, stsd_entry_pos+12, SEEK_SET);
	fread(parsedAtoms[stsd_entry_atom_number].AtomicData, 4, 1, file);
	parsedAtoms[stsd_entry_atom_number].AtomicDataClass = AtomicDataClass_UInteger;

	free(data);
	data=NULL;
	return;
}

uint64_t APar_64bitAtomRead(FILE *file, uint32_t jump_point) {
	uint64_t extended_dataSize = 0;
	char *sixtyfour_bit_data = (char *) malloc(8 + 1);
	memset(sixtyfour_bit_data, 0, 8 + 1);
	
	fseek(file, jump_point+8, SEEK_SET); //this will seek back to the last jump point and...
	fread(sixtyfour_bit_data, 1, 8, file); //read in 16 bytes so we can do a valid extraction and do a nice atom data printout
	
	extended_dataSize = UInt64FromBigEndian(sixtyfour_bit_data);
	
	//here's the problem: there can be some HUGE MPEG-4 files. They get specified by a 64-bit value. The specification says only use them when necessary - I've seen them on files about 700MB. So this will artificially place a limit on the maximum file size that would be supported under a 32-bit only AtomicParsley (but could still see & use smaller 64-bit values). For my 700MB file, moov was (rounded up) 4MB. So say 4MB x 6 +1MB give or take, and the biggest moov atom I would support is.... a heart stopping 30MB (rounded up). GADZOOKS!!! So, since I have no need to go greater than that EVER, I'm going to stik with uin32_t for filesizes and offsets & that sort. But a smaller 64-bit mdat (essentially a pseudo 32-bit traditional mdat) can be supported as long as its less than UINT32_T_MAX (minus our big fat moov allowance).
	
	if ( extended_dataSize > 4294967295UL - 30000000) {
		contains_unsupported_64_bit_atom = true;
		fprintf(stdout, "You must be off your block thinking I'm going to tag a file that is at LEAST %llu bytes long.\n", extended_dataSize);
		exit (2);
	}
	return extended_dataSize;
}

void APar_ScanAtoms(const char *path, bool scan_for_tree_ONLY) {
	if (!parsedfile) {
		file_size = findFileSize(path);
		
		FILE *file = APar_OpenFile(path, "rb");
		if (file != NULL) {
			char *data = (char *) malloc(12 + 1);
			memset(data, 0, 12 + 1);
			
			if (data == NULL) return;
			uint32_t dataSize = 0;
			uint32_t jump = 0;
			
			fread(data, 1, 12, file);
			char *atom = extractAtomName(data, 1);

			if ( memcmp(atom, "ftyp", 4) == 0) { //jpeg2000 files will have a beginning 'jp  ' atom that won't get through here
			
				APar_IdentifyBrand( extractAtomName(data, 2) );
			
				dataSize = UInt32FromBigEndian(data);
				jump = dataSize;
				
				APar_AtomizeFileInfo(parsedAtoms[atom_number], 0, jump, 0, atom, generalAtomicLevel, -1, 0, false);
				
				fseek(file, jump, SEEK_SET);
				
				while (jump < (uint32_t)file_size) {
					
					fread(data, 1, 12, file);
					char *atom = extractAtomName(data , 1);
					int atom_class = -1;
					bool uuid_atom = false;
					
					if (memcmp(atom, "uuid", 4) == 0) {
						atom = extractAtomName(data, 2);
						uuid_atom = true;
					}

					//fprintf(stdout, "atom: %s @ offset: %u \n", atom, jump);
					
					if (strncmp(atom, "data", 4) == 0) {
						atom_class=APar_DetermineDataType(data, false);
						
					} else if (uuid_atom) {						
						//because only 12 bytes per jump are read, the bytes for a uuid's data class (bytes 13-16) aren't normally fetched
						char *uuid_data = (char *) malloc(sizeof(char)*16 + 1);
						memset(uuid_data, 0, sizeof(char)*16 + 1);
						
						fseek(file, jump, SEEK_SET); //this will seek back to the last jump point and...
						fread(uuid_data, 1, 16, file); //read in 16 bytes so we can do a valid extraction and do a nice atom data printout
						
						atom_class=APar_DetermineDataType(uuid_data, uuid_atom);
						if ( (atom_class < 0) || (atom_class > 24) ) {
							atom_class = -1;
						}
						free(uuid_data);
						uuid_data=NULL;
						
					} else {
						atom_class = -1;
					}
					
					dataSize = UInt32FromBigEndian(data);
										
					//mdat.length=1; and ONLY supported for mdat atoms - no idea if the spec says "only mdat", but that's what I'm doing for now
					if ( (strncmp(atom, "mdat", 4) == 0) && (generalAtomicLevel == 1) && (dataSize == 1) ) {
						uint64_t extended_dataSize = APar_64bitAtomRead(file, jump);
						APar_AtomizeFileInfo(parsedAtoms[atom_number], jump, 1, extended_dataSize, atom, generalAtomicLevel, atom_class, 0, uuid_atom);
						
					} else {
						APar_AtomizeFileInfo(parsedAtoms[atom_number], jump, dataSize, 0, atom, generalAtomicLevel, atom_class, 0, uuid_atom);
					}
					
					
					if (dataSize == 0) { // length = 0 means it reaches to EOF
						break;
					}
					
					if (strncmp(atom, "stsd", 4) == 0) {
						//For internal use, stsd is no longer parsed; only when printing an atom hierarchical tree will it be parsed
						if (scan_for_tree_ONLY) {
							APar_Parse_stsd_Atoms(file, jump+16, dataSize);
						} else {
							APar_Extract_stsd_codec(file, jump+16);
						}
					}
					
					if (strncmp(atom, "meta", 4) == 0) {
						jump += 12;
					} else if ( strncmp(atom, "tkhd", 4) == 0 ) {
            jump += dataSize; //tkhd atoms are always 92 bytes uint32_t; don't even bother to test for any children
						
					} else if ( strncmp(atom, "tags", 4) == 0 ) { //oh dear, we have foobar2000's prison-rape tags; thick white gelatinous goo oozes out of this atom

						if (!scan_for_tree_ONLY) { //latch onto scan_for_tree_ONLY as it signals whether we are *really* writing or just getting the tree.
							
							jump += dataSize;
						} else { //we'll just be *showing* the tree, no harm in showing what foobar does to the files...
							if ( APar_TestforChildAtom(data, dataSize, atom) ) {
								jump += 8;
							} else {
								jump += dataSize;
							}
						}
						
					} else if ( APar_TestforChildAtom(data, dataSize, atom) && strncmp(atom, "free", 4) != 0 ) { // if bytes 9-12 are less than bytes 1-4 (and not 0) we have a child; if its a data atom, all bets are off
						jump += 8; //skip a head a grand total of... 8 *WHOLE* bytes - what progress!
					} else if ( generalAtomicLevel > 1 ) { // apparently, we didn't have a child
						jump += dataSize;
					} else if ((generalAtomicLevel == 1) && (dataSize == 1)) {
						jump += parsedAtoms[atom_number-1].AtomicLengthExtended; //mdat.length =1 64-bit length that is more of a cludge.
					} else {
						jump += dataSize;
					}
					generalAtomicLevel = APar_GetCurrentAtomDepth(jump, dataSize);
					
					if (jump + 8 >= (uint32_t)file_size) { //prevents jumping past EOF for the smallest of atoms
						break;
					}
					
					fseek(file, jump, SEEK_SET); 
					free(atom);
					atom = NULL;
				}
				
			} else {
				fprintf(stderr, "\nAtomicParsley error: bad mpeg4 file (ftyp atom missing or alignment error).\n\n");
				data = NULL;
				exit(1); //return;
			}
			free(data);
			data=NULL;
			fclose(file);
		}
		parsedfile = true;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Removal Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_EliminateAtom(short this_atom_number, int resume_atom_number) {
	if ( this_atom_number > 0 && this_atom_number < atom_number && resume_atom_number > 0 && resume_atom_number < atom_number) {
		short preceding_atom_pos = APar_FindPrecedingAtom(this_atom_number);

		removed_bytes_tally+=parsedAtoms[this_atom_number].AtomicLength; //used in validation routine
		parsedAtoms[preceding_atom_pos].NextAtomNumber = resume_atom_number;
		
		memset(parsedAtoms[this_atom_number].AtomicName, 0, 4); //blank out the name of the parent atom name
		parsedAtoms[this_atom_number].AtomicNumber = -1;
		parsedAtoms[this_atom_number].NextAtomNumber = -1;
	}
	return;
}

void APar_RemoveAtom(const char* atom_path, bool direct_find, bool uuid_atom_type) {
	modified_atoms = true;
	AtomicInfo desiredAtom = APar_FindAtom(atom_path, false, uuid_atom_type, false, direct_find);
	
	if (desiredAtom.AtomicNumber == 0) { //we got the default atom, ftyp - and since that can't be removed, it must not exist (or it was missed)
		//modified_atoms = false; //ah, we can't do this because other atoms may have changed with a previous option
		return;
	}
	
	if (direct_find && !uuid_atom_type) {
		AtomicInfo tailAtom = APar_LocateAtomInsertionPoint(atom_path, true);
		APar_EliminateAtom(desiredAtom.AtomicNumber, tailAtom.NextAtomNumber);
		return;
	//this will only work for AtomicParsley created uuid atoms that don't have children
	} else if (direct_find && uuid_atom_type) {
		APar_EliminateAtom(desiredAtom.AtomicNumber, desiredAtom.NextAtomNumber);
		return;
	}
	
	char* atom_hierarchy = strdup(atom_path);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	char *parent_name = strdup(search_atom_name);
	while (atom_hierarchy != NULL) {
		//free(parent_name); //<-----NEWNEWNEW makes no difference
		//parent_name=NULL; //<-----NEWNEWNEW makes no difference
		parent_name= strdup(search_atom_name);
		search_atom_name = strsep(&atom_hierarchy,".");
	}
	
  if ( (desiredAtom.AtomicName != NULL) && (search_atom_name != NULL) ) {
    char* uuid_name = (char *)malloc(sizeof(char)*5);
		memset(uuid_name, 0, sizeof(char)*5);
		
    uuid_name = strstr(search_atom_name, "uuid=");
    if (uuid_name != NULL) {
      search_atom_name = strsep(&uuid_name,"=");
    }

    if ( (strncmp(search_atom_name, desiredAtom.AtomicName, 4) == 0) ||
				 (strncmp(search_atom_name, "data", 4) == 0 && strncmp(parent_name, desiredAtom.AtomicName, 4) == 0) ) { //only remove an atom we have a matching name for	
      short parent_atom_pos;
			AtomicInfo endingAtom;
			
			if (strncmp(search_atom_name, "covr", 4) == 0) {
				parent_atom_pos = desiredAtom.AtomicNumber;
				short covr_last_child_atom = desiredAtom.NextAtomNumber;
				while (true) {
					if (parsedAtoms[parsedAtoms[covr_last_child_atom].NextAtomNumber].AtomicLevel == desiredAtom.AtomicLevel +1) {
						covr_last_child_atom = parsedAtoms[covr_last_child_atom].NextAtomNumber;
					} else {
						endingAtom = parsedAtoms[covr_last_child_atom];
						break;
					}
					if (covr_last_child_atom == 0) {
						endingAtom = parsedAtoms[atom_number-1];
						break; //this shouldn't happen
					}
				}
				
			} else {
				parent_atom_pos = APar_FindPrecedingAtom(desiredAtom.AtomicNumber);
				endingAtom = APar_LocateAtomInsertionPoint(atom_path, true);

			}
			APar_EliminateAtom(parent_atom_pos, endingAtom.NextAtomNumber);
    }
    free(atom_hierarchy);
		atom_hierarchy=NULL;
  }
	return;
}

void APar_freefree(uint8_t purge_level) {
	modified_atoms = true;
	short eval_atom = 0;
	short prev_atom = 0;
	
	while (true) {
		prev_atom = eval_atom;
		eval_atom = parsedAtoms[eval_atom].NextAtomNumber;
		if (eval_atom == 0) { //we've hit the last atom
			break;
		}

		if ( strncmp(parsedAtoms[eval_atom].AtomicName, "free", 4) == 0 ) {
			if (purge_level == 0 || purge_level >= (uint8_t)parsedAtoms[eval_atom].AtomicLevel) {
				if (parsedAtoms[eval_atom].AtomicLevel == 1 && APar_Eval_ChunkOffsetImpact(eval_atom) ) {
					removed_bytes_tally += parsedAtoms[eval_atom].AtomicLength;
				}
				short prev_atom = APar_FindPrecedingAtom(eval_atom);
				parsedAtoms[prev_atom].NextAtomNumber = parsedAtoms[eval_atom].NextAtomNumber;
				
				eval_atom = prev_atom; //go back to the previous atom and continue the search
			}
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Moving Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_MoveAtom(short this_atom_number, short new_position) {
	short precedingAtom = 0;
	short lastStationaryAtom = 0;
	short iter = 0;
	
	//look for the preceding atom (either directly before of the same level, or moov's last nth level child
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		if (parsedAtoms[iter].NextAtomNumber == this_atom_number) {
			precedingAtom = iter;
			break;
		} else {
			if (parsedAtoms[iter].NextAtomNumber == 0) { //we found the last atom (which we end our search on)
				break;
			}
		}
		iter=parsedAtoms[iter].NextAtomNumber;
	}
	
	iter = 0;
	
	//search where to insert our new atom
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		//fprintf(stdout, "At %s\n", parsedAtoms[iter].AtomicName);
		if (parsedAtoms[iter].NextAtomNumber == new_position) {
			lastStationaryAtom = iter;
			break;
		}
		iter=parsedAtoms[iter].NextAtomNumber;
		if (parsedAtoms[iter].NextAtomNumber == 0) { //we found the last atom
				lastStationaryAtom = iter;
				break;
		}

	}
	//fprintf(stdout, "%s preceded by %s, last would be %s\n", parsedAtoms[this_atom_number].AtomicName, parsedAtoms[precedingAtom].AtomicName, parsedAtoms[lastStationaryAtom].AtomicName);
	
	//APar_PrintAtomicTree();
	parsedAtoms[lastStationaryAtom].NextAtomNumber = this_atom_number;
	parsedAtoms[precedingAtom].NextAtomNumber = parsedAtoms[this_atom_number].NextAtomNumber;
	parsedAtoms[this_atom_number].NextAtomNumber = new_position;

	return;
}

bool APar_Move_mdat_Determination() {
	bool rearrange_mdat_atoms = false;
	bool found_moov_atom = false;
	int eval_atom_num = 0; 
	
	while (true) {
		if ( (strncmp(parsedAtoms[eval_atom_num].AtomicName, "moov", 4) == 0)  && (parsedAtoms[eval_atom_num].AtomicLevel == 1) ){
			found_moov_atom = true;
		}
		if ( (strncmp(parsedAtoms[eval_atom_num].AtomicName, "mdat", 4) == 0)  && (parsedAtoms[eval_atom_num].AtomicLevel == 1) ){
			if (!found_moov_atom) {
				rearrange_mdat_atoms = true;
				//break; //commented out to progress past the first mdat to... ANY of the dozens, or hundreds of 'moof's
			}
		}
		if ( (strncmp(parsedAtoms[eval_atom_num].AtomicName, "moof", 4) == 0)  && (parsedAtoms[eval_atom_num].AtomicLevel == 1) ){
			rearrange_mdat_atoms = false; //prevent rearranging atoms on a fragmented file
			move_mdat_atoms = false;
			break;
		}
		eval_atom_num = parsedAtoms[eval_atom_num].NextAtomNumber;
		
		if (eval_atom_num == 0) {
			break;
		}
	}
	return rearrange_mdat_atoms;
}

void APar_Move_mdat_Atoms() {
	int num_moved_atoms = 0;
	
	for (int i = 0; i < atom_number; i++) {
		if ( (strncmp(parsedAtoms[i].AtomicName, "mdat", 4) == 0) && (parsedAtoms[i].AtomicLevel == 1) ) {
			APar_MoveAtom(i, 0);
			num_moved_atoms ++;
		}
		if (i == (atom_number - 1 - num_moved_atoms) ) {
			break;
		}
 	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Creation Functions                                  //
///////////////////////////////////////////////////////////////////////////////////////

AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name, 
                                 char* remaining_hierarchy, short atom_level, bool asLastChild) {
	//the end boolean value below tells the function to locate where that atom (and its children) end
	AtomicInfo KeyInsertionAtom = APar_LocateAtomInsertionPoint(present_hierarchy, asLastChild);
	bool atom_shunted = false; //only shunt the NextAtomNumber once (for the first atom that is missing.
	int continuation_atom_number = 0;
	char* uuid_name = (char *)malloc(sizeof(char)*5);
	memset(uuid_name, 0, sizeof(char)*5);
	
	AtomicInfo new_atom = { 0 };
	//fprintf(stdout, "Our KEY insertion atom is \"%s\" for: %s(%s)\n", KeyInsertionAtom.AtomicName, present_hierarchy, remaining_hierarchy);
	continuation_atom_number = KeyInsertionAtom.NextAtomNumber;
	
	while (new_atom_name != NULL) {
		//the atom should be created first, so that we can determine these size issues first
		//new_atom_name = strsep(&missing_hierarchy,".");
		//fprintf(stdout, "At %s, adding new atom \"%s\" at level %i, after %s\n", present_hierarchy, new_atom_name, atom_level, parsedAtoms[APar_FindPrecedingAtom(KeyInsertionAtom.AtomicNumber)].AtomicName);

		new_atom = parsedAtoms[atom_number];
		
		uuid_name = strstr(new_atom_name, "uuid=");
		if (uuid_name != NULL) {
			uuid_name = strsep(&new_atom_name,"=");
			//fprintf(stdout, "%s\n", new_atom_name);
			new_atom.uuidAtomType = true;
		} else {
			new_atom.uuidAtomType = false;
		}
		
		new_atom.AtomicStart = 0;
		
		new_atom.AtomicName = (char*)malloc(sizeof(char)*6);
		memset(new_atom.AtomicName, 0, sizeof(char)*6);
		
		strcpy(new_atom.AtomicName, new_atom_name);
		new_atom.AtomicNumber = atom_number;
		
		if ( strncmp("meta", new_atom_name, 4) == 0 ) {
			//Though not explicitly tested here, the proper "meta" should have a definitive hierarchy of "moov.udta.meta"
			//"meta" is some oddball that is a container AND carries 4 bytes of null data; maybe its version or something
			//make sure to remember that if AtomicDataClass is >=0, to include 4 bytes in the AtomicLength
			new_atom.AtomicDataClass = AtomicDataClass_UInteger;
		} else {
			new_atom.AtomicDataClass = -1;
		}
		
		new_atom.AtomicLevel = atom_level;
		
		if (!atom_shunted) {
			
			//follow the hierarchy by NextAtomNumber (and not by AtomicNumber because that still includes the atoms removed during this run) & match KeyInsertionAtom
			int test_atom_num = 0;
			while ( test_atom_num <= atom_number ) {
			
				if (parsedAtoms[test_atom_num].NextAtomNumber == KeyInsertionAtom.NextAtomNumber) {
					parsedAtoms[test_atom_num].NextAtomNumber = atom_number;
					break;
				}
				test_atom_num = parsedAtoms[test_atom_num].NextAtomNumber;
				if (test_atom_num == 0) {
					parsedAtoms[atom_number].NextAtomNumber = atom_number;
					break;
				}
			}
			new_atom.NextAtomNumber=atom_number;
		} else {
			parsedAtoms[atom_number-1].NextAtomNumber=atom_number;
		}
		new_atom.NextAtomNumber = continuation_atom_number;
		
		new_atom_name = strsep(&remaining_hierarchy,".");
		atom_level++;
		parsedAtoms[atom_number] = new_atom;
		
		atom_number++;
		atom_shunted = true;
		
		if (new_atom_name != NULL && remaining_hierarchy != NULL) {
			if ( (strncmp(new_atom_name, "meta", 4) == 0) && (strncmp(remaining_hierarchy, "ilst", 4) == 0) ) {
				if (!Create__udta_meta_hdlr__atom) {
					Create__udta_meta_hdlr__atom = true;
				}
			}
		} //ends create hdlr section
		
	}
	
	return new_atom;
}

/*----------------------
APar_Unified_atom_Put
  atom_num - the index into the parsedAtoms array for the atom we are setting (aka AtomicNumber)
  unicode_data - a pointer to a string (possibly utf8 already); may go onto conversion to utf16 prior to the put
  text_tag_style - flag to denote that unicode_data is to become utf-16, or stay the flavors of utf8 (iTunes style, 3gp style...)
  ancillary_data - a (possibly cast) 32-bit number of any type of supplemental data to be set
	anc_bit_width - controls the number of bytes to set for ancillary data [0 to skip, 8 (1byte) - 32 (4 bytes)]

    take any variety of data & tack it onto the malloced AtomicData at the next available spot (determined by its length)
    priority is given to the numerical ancillary_data so that language can be set prior to setting whatever unicode data. Finally, advance
	  the length of the atom so that we can tack onto the end repeated times (up to the max malloced amount - which isn't checked [blush])
		if unicode_data is NULL itself, then only ancillary_data will be set - which is endian safe cuz o' bitshifting (or set 1 byte at a time)
		
		works on iTunes-style & 3GP asset style but NOT binary safe (use APar_atom_Binary_Put)
		TODO: work past the max malloced amount onto a new larger array
----------------------*/
void APar_Unified_atom_Put(short atom_num, const char* unicode_data, uint8_t text_tag_style, uint32_t ancillary_data, uint8_t anc_bit_width) {
	if (atom_num == 0) {
		return; //although it should error out, because we aren't setting anything on ftyp; APar_MetaData_atom_Init on a 3gp file (for iTunes-style metadata) will give an atom_num=0, thus preventing the setting of non-compliant metadata onto 3gp files but will allow setting of data onto a non-0 atom
	}
	uint32_t atom_data_pos = parsedAtoms[atom_num].AtomicLength - 12;
	switch (anc_bit_width) {
		case 0 : { //aye, 'twas a false alarm; arg (I'm a pirate), we just wanted to set a text string
			break;
		}
		
		case 8 : { //tracknum
			parsedAtoms[atom_num].AtomicData[atom_data_pos] = (uint8_t)ancillary_data;
			parsedAtoms[atom_num].AtomicLength++;
			atom_data_pos++;
			break;
		}
		
		case 16 : { //lang & its ilk
			parsedAtoms[atom_num].AtomicData[atom_data_pos] = (ancillary_data & 0xff00) >> 8;
			parsedAtoms[atom_num].AtomicData[atom_data_pos + 1] = (ancillary_data & 0xff) << 0;
			parsedAtoms[atom_num].AtomicLength+= 2;
			atom_data_pos+=2;
			break;
		}
		
		case 32 : { //things like coordinates and.... stuff (ah, the prose)
			parsedAtoms[atom_num].AtomicData[atom_data_pos] = (ancillary_data & 0xff000000) >> 24;
			parsedAtoms[atom_num].AtomicData[atom_data_pos + 1] = (ancillary_data & 0xff0000) >> 16;
			parsedAtoms[atom_num].AtomicData[atom_data_pos + 2] = (ancillary_data & 0xff00) >> 8;
			parsedAtoms[atom_num].AtomicData[atom_data_pos + 3] = (ancillary_data & 0xff) << 0;
			parsedAtoms[atom_num].AtomicLength+= 4;
			atom_data_pos+=4;
			break;
		}
			
		default : {
			break;
		}		
	}

	if (unicode_data != NULL) {
		if (text_tag_style == UTF16_3GP_Style) {
			uint32_t string_length = strlen(unicode_data) + 1;
			uint32_t glyphs_req_bytes = mbstowcs(NULL, unicode_data, string_length) * 2; //passing NULL pre-calculates the size of wchar_t needed;
			
			unsigned char* utf16_conversion = (unsigned char*)malloc( sizeof(unsigned char)* string_length * 2 );
			memset(utf16_conversion, 0, string_length * 2 );
			
			UTF8ToUTF16BE(utf16_conversion, glyphs_req_bytes, (unsigned char*)unicode_data, string_length);
			
			parsedAtoms[atom_num].AtomicData[atom_data_pos] = 0xFE; //BOM
			parsedAtoms[atom_num].AtomicData[atom_data_pos+1] = 0xFF; //BOM
			atom_data_pos +=2; //BOM
			
			/* copy the string directly onto AtomicData at the address of the start of AtomicData + the current length in atom_data_pos */
			/* in marked contrast to iTunes-style metadata where a string is a single string, 3gp tags like keyword & classification are more complex */
			/* directly putting the text into memory and being able to tack on more becomes a necessary accommodation */
			memcpy(parsedAtoms[atom_num].AtomicData + atom_data_pos, utf16_conversion, glyphs_req_bytes );
			parsedAtoms[atom_num].AtomicLength += glyphs_req_bytes;
			
			//double check terminating NULL (don't want to double add them - blush.... or have them missing - blushing on the.... other side)
			if (parsedAtoms[atom_num].AtomicData[atom_data_pos + (glyphs_req_bytes -1)] + parsedAtoms[atom_num].AtomicData[atom_data_pos + glyphs_req_bytes] != 0) {
				parsedAtoms[atom_num].AtomicLength += 4; //+4 because add 2 bytes for the character we just found + 2bytes for the req. NULL
			}
		
		} else if (text_tag_style == UTF8_iTunesStyle_Binary) { //because this will be 'binary' data (a misnomer for purl & egid), memcpy 4 bytes into AtomicData, not at the start of it
			uint32_t binary_bytes = strlen(unicode_data);
			memcpy(parsedAtoms[atom_num].AtomicData + atom_data_pos, unicode_data, binary_bytes + 1 );
			parsedAtoms[atom_num].AtomicLength += binary_bytes;
		
		} else {
			uint32_t total_bytes = strlen(unicode_data);
			if (text_tag_style == UTF8_3GP_Style) {
				total_bytes++;  //include the terminating NULL
				
			} else if (text_tag_style == UTF8_iTunesStyle_256byteLimited) {
				
				if (total_bytes > 255) {
					total_bytes = 255;
					
					fprintf(stdout, "AtomicParsley warning: %s was trimmed to 255 bytes (%u bytes over)\n", 
					        parsedAtoms[ APar_FindParentAtom(atom_num, parsedAtoms[atom_num].AtomicLevel) ].AtomicName, (unsigned int)total_bytes-255);
				}
			
						
			} else if (text_tag_style == UTF8_iTunesStyle_Unlimited) {
				if (total_bytes > MAXDATA_PAYLOAD) {
					free(parsedAtoms[atom_num].AtomicData);
					parsedAtoms[atom_num].AtomicData = NULL;
					
					parsedAtoms[atom_num].AtomicData = (char*)malloc( sizeof(char)* (total_bytes +1) );
					memset(parsedAtoms[atom_num].AtomicData ,0, total_bytes +1);
					
				}
			}
			
			//if we are setting iTunes-style metadata, add 0 to the pointer; for 3gp user data atoms - add in the (length-default bare atom lenth): account for language uint16_t (plus any other crap we will set); unicodeWin32 with wchar_t was converted right after program started, so do a direct copy

			memcpy(parsedAtoms[atom_num].AtomicData + (text_tag_style >= UTF8_3GP_Style ? atom_data_pos :  0), unicode_data, total_bytes + 1 );
			parsedAtoms[atom_num].AtomicLength += total_bytes;
		}	
	}	
	return;
}

/*----------------------
APar_atom_Binary_Put
  atom_num - the index into the parsedAtoms array for the atom we are setting (aka AtomicNumber)
  binary_data - a pointer to a string of binary data
  bytecount - number of bytes to copy
  atomic_data_offset - place binary data some bytes offset from the start of AtomicData

    Simple placement of binary data (perhaps containing NULLs) onto AtomicData.
		TODO: if over MAXDATA_PAYLOAD malloc a new char string
----------------------*/
void APar_atom_Binary_Put(short atom_num, const char* binary_data, uint32_t bytecount, uint32_t atomic_data_offset) {
	if (atomic_data_offset + bytecount + parsedAtoms[atom_num].AtomicLength <= MAXDATA_PAYLOAD) {
		memcpy(parsedAtoms[atom_num].AtomicData + atomic_data_offset, binary_data, bytecount );
		parsedAtoms[atom_num].AtomicLength += bytecount;
	} else {
		fprintf(stdout, "AtomicParsley warning: some data was longer than the allotted space and was skipped\n");
	}
	return;
}

/*----------------------
APar_Verify__udta_meta_hdlr__atom

    only test if the atom is present for now, it will be created just before writeout time - to insure it only happens once.
----------------------*/
void APar_Verify__udta_meta_hdlr__atom() {
	if (metadata_style == ITUNES_STYLE) {
		const char* udta_meta_hdlr__atom = "moov.udta.meta.hdlr";
		AtomicInfo hdlrAtom = APar_FindAtom(udta_meta_hdlr__atom, false, false, false, true);
		if ( hdlrAtom.AtomicNumber > 0 && hdlrAtom.AtomicNumber < atom_number ) {
			if ( strncmp(hdlrAtom.AtomicName, "hdlr", 4) != 0 ) {
				Create__udta_meta_hdlr__atom = true;
			}
		} else {
			Create__udta_meta_hdlr__atom = true; //we got a null value - which means there wan't a moov.udta.meta.hdlr atom
		}
	}
	return;
}

/*----------------------
APar_MetaData_atomGenre_Set
	atomPayload - the desired string value of the genre

    genre is special in that it gets carried on 2 atoms. A standard genre (as listed in ID3v1GenreList) is represented as a number on a 'gnre' atom
		any value other than those, and the genre is placed as a string onto a '©gen' atom. Only one or the other can be present. So if atomPayload is a
		non-NULL value, first try and match the genre into the ID3v1GenreList standard genres. Try to remove the other type of genre atom, then find or
		create the new genre atom and put the data manually onto the atom.
----------------------*/
void APar_MetaData_atomGenre_Set(const char* atomPayload) {
	if (metadata_style == ITUNES_STYLE) {
		const char* standard_genre_atom = "moov.udta.meta.ilst.gnre";
		const char* std_genre_data_atom = "moov.udta.meta.ilst.gnre.data";
		const char* custom_genre_atom = "moov.udta.meta.ilst.©gen";
		const char* cstm_genre_data_atom = "moov.udta.meta.ilst.©gen.data";
		modified_atoms = true;
		
		if ( strlen(atomPayload) == 0) {
			APar_RemoveAtom(std_genre_data_atom, false, false); //find the atom; don't create if it's "" to remove
			APar_RemoveAtom(cstm_genre_data_atom, false, false); //find the atom; don't create if it's "" to remove
		} else {
		
			uint8_t genre_number = StringGenreToInt(atomPayload);
			AtomicInfo genreAtom;
			skip_meta_hdlr_creation = false;
			
			if (genre_number != 0) {
				//first find if a custom genre atom ("©gen") exists; erase the custom-string genre atom in favor of the standard genre atom
				
				AtomicInfo verboten_genre_atom = APar_FindAtom(custom_genre_atom, false, false, true, false);
				
				if (verboten_genre_atom.AtomicNumber != 0) {
					if (strlen(verboten_genre_atom.AtomicName) > 0) {
						if (strncmp(verboten_genre_atom.AtomicName, "©gen", 4) == 0) {
							APar_RemoveAtom(cstm_genre_data_atom, false, false);
						}
					}
				}
				
				genreAtom = APar_FindAtom(std_genre_data_atom, true, false, true, false);
				APar_MetaData_atom_QuickInit(genreAtom.AtomicNumber, AtomicDataClass_UInteger, 0);
				APar_Unified_atom_Put(genreAtom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
				APar_Unified_atom_Put(genreAtom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, (uint32_t)genre_number, 8);

			} else {
				
				AtomicInfo verboten_genre_atom = APar_FindAtom(standard_genre_atom, false, false, true, false);

				if (verboten_genre_atom.AtomicNumber > 5 && verboten_genre_atom.AtomicNumber < atom_number) {
					if (strncmp(verboten_genre_atom.AtomicName, "gnre", 4) == 0) {
						APar_RemoveAtom(std_genre_data_atom, false, false);
					}		
				}
				genreAtom = APar_FindAtom(cstm_genre_data_atom, true, false, true, false);
				APar_MetaData_atom_QuickInit(genreAtom.AtomicNumber, AtomicDataClass_Text, 0);
				APar_Unified_atom_Put(genreAtom.AtomicNumber, atomPayload, UTF8_iTunesStyle_256byteLimited, 0, 0);
			}
		}
	} //end if (metadata_style == ITUNES_STYLE)
	return;
}

/*----------------------
APar_MetaData_atomArtwork_Init
	atom_num - the AtomicNumber of the atom in the parsedAtoms array (probably newly created)
	artworkPath - the path that was provided on a (hopefully) existant jpg/png file

    artwork will be inited differently because we need to test a) that the file exists and b) get its size in bytes. This info will be used at the size
		of the 'data' atom under 'covr' - and the path will be carried on AtomicData until write-out time, when the binary contents of the original will be
		copied onto the atom.
----------------------*/
void APar_MetaData_atomArtwork_Init(short atom_num, const char* artworkPath) {
	TestFileExistence(artworkPath, false);
	off_t picture_size = findFileSize(artworkPath);
	
	if (picture_size > 0) {
		APar_MetaData_atom_QuickInit(atom_num, APar_TestArtworkBinaryData(artworkPath), 0 );
		parsedAtoms[atom_num].AtomicLength += (uint32_t)picture_size;
		if (IsUnicodeWinOS() ) {
			memcpy(parsedAtoms[atom_num].AtomicData, artworkPath, wcslen( (wchar_t*)artworkPath ) * 2);
		} else {
			parsedAtoms[atom_num].AtomicData = strdup(artworkPath);
		}
	}	
	return;
}

/*----------------------
APar_MetaData_atomArtwork_Set
	artworkPath - the path that was provided on a (hopefully) existant jpg/png file
	env_PicOptions - picture embedding preferences from a 'export PIC_OPTIONS=foo' setting

    artwork gets stored under a single 'covr' atom, but with many 'data' atoms - each 'data' atom contains the binary data for each picture.
		When the 'covr' atom is found, we create a sparse atom at the end of the existing 'data' atoms, and then perform any of the image manipulation
		features on the image. The path of the file (either original, modified artwork, or both) are returned to use for possible atom creation
----------------------*/
void APar_MetaData_atomArtwork_Set(const char* artworkPath, char* env_PicOptions) {
	if (metadata_style == ITUNES_STYLE) {
		modified_atoms = true;
		const char* artwork_atom = "moov.udta.meta.ilst.covr";
		if (memcmp(artworkPath, "REMOVE_ALL", 10) == 0) {
			APar_RemoveAtom(artwork_atom, false, false);
			
		} else {
			skip_meta_hdlr_creation = false;
			AtomicInfo desiredAtom = APar_FindAtom(artwork_atom, true, false, true, false);
			desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
			
			//determine if any picture preferences will impact the picture file in any way
			myPicturePrefs = ExtractPicPrefs(env_PicOptions);

	#if defined (DARWIN_PLATFORM)
			char* resized_filepath = ResizeGivenImage(artworkPath , myPicturePrefs);
			if ( strncmp(resized_filepath, "/", 1) == 0 ) {
				APar_MetaData_atomArtwork_Init(desiredAtom.AtomicNumber, resized_filepath);
				parsedAtoms[desiredAtom.AtomicNumber].tempFile = true; //THIS desiredAtom holds the temp pic file path
			
				if (myPicturePrefs.addBOTHpix) {
					//create another sparse atom to hold the new file path (otherwise the 2nd will just overwrite the 1st in EncapsulateData
					desiredAtom = APar_FindAtom(artwork_atom, true, false, true, false);
					desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
					APar_MetaData_atomArtwork_Init(desiredAtom.AtomicNumber, artworkPath);
				}
			} else {
				APar_MetaData_atomArtwork_Init(desiredAtom.AtomicNumber, artworkPath);
			}
	#else
			//perhaps some libjpeg based resizing/modification for non-Mac OS X based platforms
			APar_MetaData_atomArtwork_Init(desiredAtom.AtomicNumber, artworkPath);
	#endif
		}
	} ////end if (metadata_style == ITUNES_STYLE)
	return;
}

/*----------------------
APar_3GP_Keyword_atom_Format
	keywords_globbed - the globbed string of keywords ('foo1,foo2,foo_you')
	keyword_count - count of keywords in the above globbed string
	set_UTF16_text - whether to encode as utf16
	formed_keyword_struct - the char string that will hold the converted keyword struct (manually formatted)

    3gp keywords are a little more complicated. Since they will be entered separated by some form of punctuation, they need to be separated out
		They also will possibly be converted to utf16 - and they NEED to start with the BOM then. Prefacing each keyword is the 8bit length of the string
		And each keyword needs to be NULL terminated. Technically it would be possible to even have mixed encodings (not supported here).
----------------------*/
uint32_t APar_3GP_Keyword_atom_Format(char* keywords_globbed, uint8_t keyword_count, bool set_UTF16_text, char* &formed_keyword_struct) {
	uint32_t formed_string_offset = 0;
	
	char* a_keyword = strsep(&keywords_globbed,",");
	uint32_t string_len = strlen(a_keyword);
	
	for (uint8_t i=1; i <= keyword_count; i++) {
		if (set_UTF16_text) {

			uint32_t glyphs_req_bytes = mbstowcs(NULL, a_keyword, string_len+1) * 2; //passing NULL pre-calculates the size of wchar_t needed;
			
			formed_keyword_struct[formed_string_offset+1] = 0xFE; //BOM
			formed_keyword_struct[formed_string_offset+2] = 0xFF; //BOM
			formed_string_offset+=3; //BOM + keyword length that has yet to be set
			
			int bytes_converted = UTF8ToUTF16BE((unsigned char*)(formed_keyword_struct + formed_string_offset), glyphs_req_bytes, (unsigned char*)a_keyword, string_len);
			
			if (bytes_converted > 1) {
				formed_keyword_struct[formed_string_offset-3] = (uint8_t)bytes_converted + 4; //keyword length is NOW set
				formed_string_offset += bytes_converted + 2; //NULL terminator
			}						
		} else {

			uint32_t string_len = strlen(a_keyword);
			formed_keyword_struct[formed_string_offset] = (uint8_t)string_len + 1; //add the terminating NULL
			formed_string_offset++;
			memcpy(formed_keyword_struct + formed_string_offset, a_keyword, string_len );
			formed_string_offset+= (string_len +1);
		}
		a_keyword = strsep(&keywords_globbed,",");
	}
	return formed_string_offset;
}

/*----------------------
APar_uuid_atom_Init
	atom_path - the parent hierarchy of the desired atom (with the location of the specific uuid atom supplied as '=%s')
	uuidName - the name of the atom (possibly provided in a forbidden utf8 - only latin1 aka iso8859 is acceptable)
	dataType - for now text is only supported
	uuidValue - the string that will get embedded onto the atom
	shellAtom - flag to denote whether the atom may possibly come as utf8 encoded

    uuid atoms are user-supplied/user-defined atoms that allow for extended tagging support. Because a uuid atom is malleable, and defined by the utility
		that created it, any information carried by a uuid is arbitrary, and cannot be guaranteed by a non-originating utility. In AtomicParsley uuid atoms,
		the data is presented much like an iTunes-style atom - except that the information gets carried directly on the uuid atom - no 'data' child atom
		exists. A uuid atom is a special longer type of traditional atom. As a traditional atom, it name is 'uuid' - and the 4 bytes after that represent
		its uuid name. Because the data will be directly sitting on the atom, a different means of finding these atoms exists, as well as creating the
		acutal uuidpath itself. Once created however, placing information on it is very much like any other atom - done via APar_Unified_atom_Put
----------------------*/
short APar_uuid_atom_Init(const char* atom_path, char* uuidName, const int dataType, const char* uuidValue, bool shellAtom) {
	modified_atoms = true;
	char uuid_path[256];
	memset(uuid_path, 0, sizeof(uuid_path));
	AtomicInfo desiredAtom = { 0 };
	char *uuid_4char_name;
	bool free_uuid_char = false;

	if (shellAtom) {
		uuid_4char_name = (char*)malloc(sizeof(char)*10);
		memset(uuid_4char_name, 0, 10);
		int conver = UTF8Toisolat1((unsigned char*)uuid_4char_name, 10, (unsigned char*)uuidName, strlen(uuidName) );
		free_uuid_char = true;
	
	} else {
		uuid_4char_name = uuidName;
	}
	
#if defined (_MSC_VER) /* sprintf must behave differently in win32 VisualC as a DEBUG release */
	strncpy(uuid_path, atom_path, strlen(atom_path) -2 );
	strncat(uuid_path, uuid_4char_name, 4);
#else
	sprintf(uuid_path, atom_path, uuid_4char_name); //gives "moov.udta.meta.uuid=©url"
#endif
	if (free_uuid_char) {
		free(uuid_4char_name);
		uuid_4char_name = NULL;
	}
	
	if ( strlen(uuidValue) == 0) {
		APar_RemoveAtom(uuid_path, true, true); //find the atom; don't create if it's "" to remove
		//APar_PrintAtomicTree();
	} else {
		//uuid atoms won't have 'data' child atoms - they will carry the data directly as opposed to traditional iTunes-style metadata that does store the information on 'data' atoms. But user-defined is user-defined, so that is how it will be defined here.
		desiredAtom = APar_FindAtom(uuid_path, true, true, false, false);
		skip_meta_hdlr_creation = false;
		
		if (dataType == AtomicDataClass_Text) {
			APar_MetaData_atom_QuickInit(desiredAtom.AtomicNumber, dataType, 4); //+4 because 'uuid' takes 4 bytes
		}
	}

	return desiredAtom.AtomicNumber;
}

/*----------------------
APar_MetaData_atom_QuickInit
	atom_num - the position in the parsedAtoms array (either found in the file or a newly created sparse atom) so AtomicData can be initialized
	atomFlags - the AtomicDataClass for the iTunes-style metadata atom
	supplemental_length - iTunes-style metadata for 'data' atoms is >= 16bytes long; AtomicParsley created uuid atoms will be +4bytes directly on that atom

    Metadata_QuickInit will initialize a pre-found atom to MAXDATA_PAYLOAD so that it can carry info on AtomicData
----------------------*/
void APar_MetaData_atom_QuickInit(short atom_num, const int atomFlags, uint32_t supplemental_length) {
	//this will skip the finding of atoms and just malloc the AtomicData; used by genre & artwork

	parsedAtoms[atom_num].AtomicData = (char*)malloc(sizeof(char)* MAXDATA_PAYLOAD + 1 ); //puts a hard limit on the length of strings (the spec doesn't)
	memset(parsedAtoms[atom_num].AtomicData, 0, sizeof(char)* MAXDATA_PAYLOAD + 1 );
	
	parsedAtoms[atom_num].AtomicLength = 16 + supplemental_length; // 4bytes atom length, 4 bytes atom length, 4 bytes version/flags, 4 bytes NULL
	parsedAtoms[atom_num].AtomicDataClass = atomFlags;
	
	return;
}

/*----------------------
APar_MetaData_atom_Init
	atom_path - the hierarchical path to the specific atom carrying iTunes-style metadata that will be found (and created if necessary)
	MD_Payload - the information to be carried (also used as a test if NULL to remove the atom)
	atomFlags - the AtomicDataClass for the atom (text, integer or unsigned integer)

    Metadata_Init will search for and create the necessary hierarchy so that the atom can be initialized to carry the payload data on AtomicData
----------------------*/
short APar_MetaData_atom_Init(const char* atom_path, const char* MD_Payload, const int atomFlags) {
	//this will handle the vanilla iTunes-style metadata atoms; genre will be handled elsewehere because it gets carried on 2 different atoms, and artwork gets special treatment because it can have multiple child data atoms
	if (metadata_style != ITUNES_STYLE) {
		return 0;
	}
	modified_atoms = true;
	bool retain_atom = true;
		
	if ( strlen(MD_Payload) == 0 ) {
		retain_atom = false;
	}
		
	AtomicInfo desiredAtom = APar_FindAtom(atom_path, true, false, retain_atom, false); //finds the atom; if not present, creates the atom
		
	AtomicInfo parent_atom = parsedAtoms[ APar_FindParentAtom(desiredAtom.AtomicNumber, desiredAtom.AtomicLevel) ];
		
	if (! retain_atom) {
		if (desiredAtom.AtomicNumber > 0 && parent_atom.AtomicNumber > 0) {
			APar_EliminateAtom(parent_atom.AtomicNumber, desiredAtom.NextAtomNumber);
		}
			
	} else {
		skip_meta_hdlr_creation = false;
		parsedAtoms[desiredAtom.AtomicNumber].AtomicData = (char*)malloc(sizeof(char)* MAXDATA_PAYLOAD + 1 ); //puts a hard limit on the length of strings (the spec doesn't)
		memset(parsedAtoms[desiredAtom.AtomicNumber].AtomicData, 0, sizeof(char)* MAXDATA_PAYLOAD + 1 );
		
		parsedAtoms[desiredAtom.AtomicNumber].AtomicLength = 16; // 4bytes atom length, 4 bytes atom length, 4 bytes version/flags, 4 bytes NULL
		parsedAtoms[desiredAtom.AtomicNumber].AtomicDataClass = atomFlags;
	}
	return desiredAtom.AtomicNumber;
}

/*----------------------
APar_UserData_atom_Init
	atom_path - the hierarchical path to the specific 3gp-style(Quicktime-style) asset atom that will be found (and created if necessary)
	UD_Payload - the information to be carried (also used as a test if NULL to remove the atom)

    UserData_Init will search for and create the necessary hierarchy so that the atom can be initialized to carry the payload data on AtomicData
		3GP-style assets are different from iTunes-style metadata. 3GP assets are carried directly on the atom (iTunes md is on a child 'data' atom),
		has a packed language code, and can be in utf8 or utf16 which are NULL terminated (not so in iTunes md). The two implementations are separate
----------------------*/
short APar_UserData_atom_Init(const char* atom_path, const char* UD_Payload) {
	//Perhaps there is something wrong with Apple's implementation of 3gp metadata, or I'm loosing my mind.
	//the exact same utf8 string that shows up in a 3gp file as ??? - ??? shows up *perfect* in an mp4 or mov container. encoded as utf16 same problem
	//a sample string using Polish glyphs in utf8 has some gylphs missing with lang=eng. The same string with 'lang=pol', and different glyphs are missing.
	//the problem occurs using unicode.org's ConvertUTF8toUTF16 or using libxmls's UTF8ToUTF16BE (when converting to utf16) in the same places - except for the copyright symbol which unicode.org's ConvertUTF16toUTF8 didn't properly convert - which was the reason why libxml's functions are now used. And at no point can I get the audio protected P-in-a-circle glyph to show up in utf8 or utf16.
	//to summarize, either I am completely overlooking some interplay (of lang somehow changing the utf8 or utf16 standard), the unicode translations are off (which in the case of utf8 is embedded directly on Mac OS X, so that can't be it), or Apple's 3gp implementation is off.
	
	//TODO: most of these tags allow for more than 1 of each if the uint16_t language is different
	modified_atoms = true;
	bool retain_atom = true;
	AtomicInfo desiredAtom = { 0 };
	
	if ( strlen(UD_Payload) == 0) {
		retain_atom = false;
	}
	
	if ( !retain_atom ) {
		APar_RemoveAtom(atom_path, true, false); //find the atom; don't create if it's "" to remove
	} else {
		desiredAtom = APar_FindAtom(atom_path, true, true, false, false);
		parsedAtoms[desiredAtom.AtomicNumber].AtomicData = (char*)malloc(sizeof(char)* MAXDATA_PAYLOAD ); //puts a hard limit on the length of strings (the spec doesn't)
		memset(parsedAtoms[desiredAtom.AtomicNumber].AtomicData, 0, sizeof(char)* MAXDATA_PAYLOAD );
		
		parsedAtoms[desiredAtom.AtomicNumber].AtomicLength = 12; // 4bytes atom length, 4 bytes atom length, 4 bytes version/flags (NULLs)
		parsedAtoms[desiredAtom.AtomicNumber].AtomicDataClass = AtomicDataClass_UInteger;
	}
	return desiredAtom.AtomicNumber;
}

void APar_StandardTime(char* &formed_time) {
	//ISO 8601 Coordinated Universal Time (UTC)
  time_t rawtime;
  struct tm *timeinfo;

  time (&rawtime);
  timeinfo = gmtime (&rawtime);
	strftime(formed_time ,100 , "%Y-%m-%dT%H:%M:%SZ", timeinfo); //that hanging Z is there; denotes the UTC
	
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                         'stco' Atom Calculations                                  //
///////////////////////////////////////////////////////////////////////////////////////

//determine if our mdat atom has moved at all...
uint32_t APar_DetermineMediaData_AtomPosition() {
	uint32_t mdat_position = 0;
	short thisAtomNumber = 0;
	
	//loop through each atom in the struct array (which holds the offset info/data)
 	while (parsedAtoms[thisAtomNumber].NextAtomNumber != 0) {
		
		if ( (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "mdat", 4) == 0) && 
				 (parsedAtoms[thisAtomNumber].AtomicLevel == 1) && 
				(parsedAtoms[thisAtomNumber].AtomicLength <= 1 || parsedAtoms[thisAtomNumber].AtomicLength > 75) ) {
			break;
		}
		if (parsedAtoms[thisAtomNumber].AtomicLevel == 1 && parsedAtoms[thisAtomNumber].AtomicLengthExtended == 0) {
			mdat_position +=parsedAtoms[thisAtomNumber].AtomicLength;
		} else {
			//part of the pseudo 64-bit support
			mdat_position +=parsedAtoms[thisAtomNumber].AtomicLengthExtended;
		}
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}
	return mdat_position;
}

uint32_t APar_SimpleSumAtoms(short stop_atom) {
	uint32_t byte_sum = 0;
	//first, find the first mdat after this initial 'tfhd' atom to get the sum relative to that atom
	while (true) {
		if ( strncmp(parsedAtoms[stop_atom].AtomicName, "mdat", 4) == 0) {
			stop_atom--; //don't include the fragment's mdat, just the atoms prior to it
			break;
		} else {
			if (parsedAtoms[stop_atom].NextAtomNumber != 0) {
				stop_atom = parsedAtoms[stop_atom].NextAtomNumber;
			} else {
				break;
			}
		}
	}
	byte_sum += 8; //the 'tfhd' points to the byte in mdat where the fragment data is - NOT the atom itself (should always be +8bytes with a fragment)
	while (true) {
		if (parsedAtoms[stop_atom].AtomicLevel == 1) {
			byte_sum+= (parsedAtoms[stop_atom].AtomicLength == 1 ? (uint32_t )parsedAtoms[stop_atom].AtomicLengthExtended : parsedAtoms[stop_atom].AtomicLength);
			//fprintf(stdout, "%i %s (%u)\n", stop_atom, parsedAtoms[stop_atom].AtomicName, parsedAtoms[stop_atom].AtomicLength);
		}
		if (stop_atom == 0) {
			break;
		} else {
			stop_atom = APar_FindPrecedingAtom(stop_atom);
		}
	}
	return byte_sum;
}

bool APar_Readjust_CO64_atom(uint32_t mdat_position, short co64_number) {
	bool co64_changed = false;
	APar_AtomicRead(co64_number);
	parsedAtoms[co64_number].AtomicDataClass = AtomicDataClass_UInteger;
	//readjust
	
	char* co64_entries = (char *)malloc(sizeof(char)*4 + 1);
	memset(co64_entries, 0, sizeof(char)*4 + 1);
	
	memcpy(co64_entries, parsedAtoms[co64_number].AtomicData, 4);
	uint32_t entries = UInt32FromBigEndian(co64_entries);
	
	char* a_64bit_entry = (char *)malloc(sizeof(char)*8 + 1);
	memset(a_64bit_entry, 0, sizeof(char)*8 + 1);
	
	for(uint32_t i=1; i<=entries; i++) {
		//read 8 bytes of the atom into a 8 char uint64_t a_64bit_entry to eval it
		for (int c = 0; c <=7; c++ ) {
			//first co64 entry (32-bit uint32_t) is the number of entries; every other one is an actual offset value
			a_64bit_entry[c] = parsedAtoms[co64_number].AtomicData[4 + (i-1)*8 + c];
		}
		uint64_t this_entry = UInt64FromBigEndian(a_64bit_entry);
		
		if (i == 1 && mdat_supplemental_offset == 0) { //for the first chunk, and only for the first *ever* entry, make the global mdat supplemental offset
			
			mdat_supplemental_offset = mdat_position - (this_entry - removed_bytes_tally);
			bytes_into_mdat = this_entry - bytes_before_mdat - removed_bytes_tally;
			
			if (mdat_supplemental_offset == 0) {
				break;
			}
		}
		
		if (mdat_supplemental_offset != 0) {
			co64_changed = true;
		}
		
		this_entry += mdat_supplemental_offset + bytes_into_mdat; //this is where we add our new mdat offset difference
		char8TOuint64(this_entry, a_64bit_entry);
		//and put the data back into AtomicData...
		for (int d = 0; d <=7; d++ ) {
			//first stco entry is the number of entries; every other one is an actual offset value
			parsedAtoms[co64_number].AtomicData[4 + (i-1)*8 + d] = a_64bit_entry[d];
		}
	}
	
	free(a_64bit_entry);
	free(co64_entries);
	a_64bit_entry=NULL;
	co64_entries=NULL;
	//end readjustment
	return co64_changed;
}

bool APar_Readjust_TFHD_fragment_atom(uint32_t mdat_position, short tfhd_number) {
	static bool tfhd_changed = false;
	static bool determined_offset = false;
	static uint64_t base_offset = 0;
	
	APar_AtomicRead(tfhd_number);
	char* tfhd_atomFlags_scrap = (char *)malloc(sizeof(char)*10);
	memset(tfhd_atomFlags_scrap, 0, 10);
	parsedAtoms[tfhd_number].AtomicDataClass = APar_read32(tfhd_atomFlags_scrap, source_file, parsedAtoms[tfhd_number].AtomicStart+8);
 
	if (parsedAtoms[tfhd_number].AtomicDataClass & 0x01) { //seems the atomflags suggest bitpacking, but the spec doesn't specify it; if the 1st bit is set...
		memset(tfhd_atomFlags_scrap, 0, 10);	
		memcpy(tfhd_atomFlags_scrap, parsedAtoms[tfhd_number].AtomicData, 4);
		
		uint32_t track_ID = UInt32FromBigEndian(tfhd_atomFlags_scrap); //unused
		uint64_t tfhd_offset = UInt64FromBigEndian(parsedAtoms[tfhd_number].AtomicData +4);
		
		if (!determined_offset) {
			determined_offset = true;
			base_offset = APar_SimpleSumAtoms(tfhd_number) - tfhd_offset;
			if (base_offset != 0) {
				tfhd_changed = true;
			}
		}
		
		tfhd_offset += base_offset;
		char8TOuint64(tfhd_offset, parsedAtoms[tfhd_number].AtomicData +4);
	}
	return tfhd_changed;
}

bool APar_Readjust_STCO_atom(uint32_t mdat_position, short stco_number) {
	bool stco_changed = false;
	APar_AtomicRead(stco_number);
	parsedAtoms[stco_number].AtomicDataClass = AtomicDataClass_UInteger;
	//readjust
	
	char* stco_entries = (char *)malloc(sizeof(char)*4 + 1);
	memset(stco_entries, 0, sizeof(char)*4 + 1);
	
	memcpy(stco_entries, parsedAtoms[stco_number].AtomicData, 4);
	uint32_t entries = UInt32FromBigEndian(stco_entries);
	
	char* an_entry = (char *)malloc(sizeof(char)*4 + 1);
	memset(an_entry, 0, sizeof(char)*4 + 1);
	
	for(uint32_t i=1; i<=entries; i++) {
		//read 4 bytes of the atom into a 4 char uint32_t an_entry to eval it
		for (int c = 0; c <=3; c++ ) {
			//first stco entry is the number of entries; every other one is an actual offset value
			an_entry[c] = parsedAtoms[stco_number].AtomicData[i*4 + c];
		}
		
		uint32_t this_entry = UInt32FromBigEndian(an_entry);
						
		if (i == 1 && mdat_supplemental_offset == 0) { //for the first chunk, and only for the first *ever* entry, make the global mdat supplemental offset
		
			mdat_supplemental_offset = (uint64_t)(mdat_position - (this_entry - removed_bytes_tally) );
			bytes_into_mdat = this_entry - bytes_before_mdat - removed_bytes_tally;
			
			if (mdat_supplemental_offset == 0) {
				break;
			}
		}
		
		if (mdat_supplemental_offset != 0) {
			stco_changed = true;
		}

		this_entry += mdat_supplemental_offset + bytes_into_mdat;
		char4TOuint32(this_entry, an_entry);
		//and put the data back into AtomicData...
		for (int d = 0; d <=3; d++ ) {
			//first stco entry is the number of entries; every other one is an actual offset value
			parsedAtoms[stco_number].AtomicData[i*4 + d] = an_entry[d];
		}
	}
	
	free(an_entry);
	free(stco_entries);
	an_entry=NULL;
	stco_entries=NULL;
	//end readjustment
	return stco_changed;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Determine Atom Length                                    //
///////////////////////////////////////////////////////////////////////////////////////

void APar_DetermineNewFileLength() {
	short thisAtomNumber = 0;
	while (true) {		
		if (parsedAtoms[thisAtomNumber].AtomicLevel == 1) {
			if (parsedAtoms[thisAtomNumber].AtomicLengthExtended == 0) {
				//normal 32-bit number when AtomicLengthExtended == 0 (for run-o-the-mill mdat & mdat.length=0)
				new_file_size += parsedAtoms[thisAtomNumber].AtomicLength; //used in progressbar
			} else {
				//pseudo 64-bit mdat length
				new_file_size += parsedAtoms[thisAtomNumber].AtomicLengthExtended; //used in progressbar
			}
		}
		if (parsedAtoms[thisAtomNumber].AtomicLength == 0) {				
	    new_file_size += (uint32_t)file_size - parsedAtoms[thisAtomNumber].AtomicStart; //used in progressbar; mdat.length = 1
		}
		if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) {
			break;
		}
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}
	return;
}

void APar_DetermineAtomLengths() {
	
	if (!skip_meta_hdlr_creation) {
		APar_Verify__udta_meta_hdlr__atom();
	}
	
	if (metadata_style >= THIRD_GEN_PARTNER) { //with 3gp files, ***FORCED*** removal of Nero's dain-bread tagging scheme gets the GoLytely
		AtomicInfo FUBAR_tag_atom = APar_FindAtom("moov.udta.tags", false, false, false, true);
		if (FUBAR_tag_atom.AtomicNumber != 0) {
			APar_RemoveAtom("moov.udta.tags", true, false);
		}
	}

	if (move_mdat_atoms && APar_Move_mdat_Determination() ) { //from cli flag
		APar_Move_mdat_Atoms();
	}
	
	//Create__udta_meta_hdlr__atom gets set in APar_Verify__udta_meta_hdlr__atom; a (filled) hdlr atom is required by iTunes to enable tagging
	if (Create__udta_meta_hdlr__atom && !skip_meta_hdlr_creation) {
		
		//if Quicktime (Player at the least) is used to create any type of mp4 file, the entire udta hierarchy is missing. If iTunes doesn't find
		//this "moov.udta.meta.hdlr" atom (and its data), it refuses to let any information be changed & the dreaded "Album Artwork Not Modifiable"
		//shows up. It's because this atom is missing. Oddly, QT Player can see the info, but this only works for mp4/m4a files.
		
		AtomicInfo hdlr_atom = APar_FindAtom("moov.udta.meta.hdlr", false, false, false, true);
		hdlr_atom = APar_CreateSparseAtom("moov.udta.meta", "hdlr", NULL, 4, false);
		
		APar_MetaData_atom_QuickInit(hdlr_atom.AtomicNumber, AtomicDataClass_UInteger, 0);
		APar_Unified_atom_Put(hdlr_atom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0x6D646972, 32); //'mdir'
		APar_Unified_atom_Put(hdlr_atom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0x6170706C, 32); //'appl'
		APar_Unified_atom_Put(hdlr_atom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0, 32);
		APar_Unified_atom_Put(hdlr_atom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0, 32);
		APar_Unified_atom_Put(hdlr_atom.AtomicNumber, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
	}
	
	short rev_atom_loop = APar_FindLastAtom();
	//fprintf(stdout, "Last atom is named %s, num:%i\n", parsedAtoms[last_atom].AtomicName, parsedAtoms[last_atom].AtomicNumber);
	
	//To determine the lengths of the atoms, and of each parent for EVERY atom in the hierarchy (even atoms we haven't touched), we start at the end
	//
	//Progressing backward, we evaluate & look at each atom; if it is at the end of its hierarchy then its length is what it already states
	//if the atom after our eval atom is a child, sum the lengths of atoms who are 1 level below to our length; also parent atoms have a lengh of 8
	//which are taken care of in the case statement; iterate backwards through tree taking note of odball atoms as they occur.
	
	while (true) {
		short next_atom = 0;
		uint32_t atom_size = 0;
		short previous_atom = 0; //only gets used in testing for atom under stsd

		//fprintf(stdout, "current atom is named %s, num:%i\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[rev_atom_loop].AtomicNumber);
		
		if (rev_atom_loop == 0) {
			break; //we seem to have hit the first atom
		} else {
			previous_atom = APar_FindPrecedingAtom(rev_atom_loop);
		}
		
		uint32_t _atom_ = UInt32FromBigEndian(parsedAtoms[rev_atom_loop].AtomicName);
		switch (_atom_) {
			case 0x6D657461 : //'meta'
				atom_size += 12; //meta has 4 bytes length, 4 bytes name & 4 bytes NULL space (...it could be versioned atom...)
				break;
			
			case 0x73747364 : //'stsd'
				atom_size += 16;
				break;
			
			default :
				atom_size += 8; //all atoms have *at least* 4bytes length & 4 bytes name
				break;		
		}
		
		if (parsedAtoms[rev_atom_loop].NextAtomNumber != 0) {
			next_atom = parsedAtoms[rev_atom_loop].NextAtomNumber;
		}
		
		while (parsedAtoms[next_atom].AtomicLevel > parsedAtoms[rev_atom_loop].AtomicLevel) { // eval all child atoms....
			//fprintf(stdout, "\ttest child atom %s, level:%i (sum %u)\n", parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size);
			if (parsedAtoms[rev_atom_loop].AtomicLevel == ( parsedAtoms[next_atom].AtomicLevel - 1) ) { // only child atoms 1 level down
				atom_size += parsedAtoms[next_atom].AtomicLength;
				//fprintf(stdout, "\t\teval child atom %s, level:%i (sum %u)\n", parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size); 
				//fprintf(stdout, "\t\teval %s's child atom %s, level:%i (sum %u, added %u)\n", parsedAtoms[previous_atom].AtomicName, parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size, parsedAtoms[next_atom].AtomicLength);
			} else if (parsedAtoms[next_atom].AtomicLevel < parsedAtoms[rev_atom_loop].AtomicLevel) {
				break;
			}
			next_atom = parsedAtoms[next_atom].NextAtomNumber; //increment to eval next atom
			parsedAtoms[rev_atom_loop].AtomicLength = atom_size;
		}
		
		if (_atom_ == 0x75647461 && parsedAtoms[rev_atom_loop].AtomicLevel > parsedAtoms[ parsedAtoms[rev_atom_loop].NextAtomNumber ].AtomicLevel) { //udta with no child atoms; get here by erasing the last asset in a 3gp file, and it won't quite erase because udta thinks its the former AtomicLength
			parsedAtoms[rev_atom_loop].AtomicLength = 8;
		}
		
		rev_atom_loop = APar_FindPrecedingAtom(parsedAtoms[rev_atom_loop].AtomicNumber);
		
	}
	APar_DetermineNewFileLength();
	//APar_SimpleAtomPrintout();
	//APar_PrintAtomicTree();
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Writing Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_ValidateAtoms() {
	bool atom_name_with_4_characters = true;
	short iter = 0;
	short next_atom = 0;
	uint64_t simple_tally = 0;
	
	while (true) {
		// there are valid atom names that are 0x00000001 - but I haven't seen them in MPEG-4 files, but they could show up, so this isn't a hard error
		if ( strlen(parsedAtoms[iter].AtomicName) < 4) {
			atom_name_with_4_characters = false;
		}
		
		//test for atoms that are going to be greater than out current file size; problem is we could be adding a 1MB pix to a 200k 3gp file; only fail for a file > 300k file; otherwise there would have to be more checks (like artwork present, but a zealous tagger could make moov.lengt > filzesize)
		if (parsedAtoms[iter].AtomicLength > (uint32_t)file_size && file_size > 300000) {
			if (parsedAtoms[iter].AtomicData == NULL) {
				fprintf(stderr, "AtomicParsley error: an atom was detected that presents as larger than filesize. Aborting. %c\n", '\a');
				fprintf(stderr, "atom %s is %u bytes long which is greater than the filesize of %llu\n", parsedAtoms[iter].AtomicName, parsedAtoms[iter].AtomicLength, (long long unsigned int)file_size);
				exit(1); //its conceivable to repair such an off length by the surrounding atoms constrained by file_size - just not anytime soon; probly would catch a foobar2000 0.9 tagged file
			}
		}
		
		next_atom=parsedAtoms[iter].NextAtomNumber;
		if (parsedAtoms[next_atom].AtomicLevel <= parsedAtoms[iter].AtomicLevel) {
			if (parsedAtoms[iter].AtomicData == NULL) {
				(parsedAtoms[iter].AtomicLength == 1 ? simple_tally += parsedAtoms[iter].AtomicLengthExtended : simple_tally += parsedAtoms[iter].AtomicLength);
				if (parsedAtoms[iter].AtomicLength == 0 && strncmp(parsedAtoms[iter].AtomicName, "mdat", 4) == 0) {
					simple_tally = (uint64_t)file_size - parsedAtoms[iter].AtomicStart;
				}
			}
		}
		
		if (strncmp(parsedAtoms[iter].AtomicName, "mdat", 4) == 0 && parsedAtoms[iter].AtomicLevel != 1) {
			fprintf(stderr, "AtomicParsley error: mdat atom was found at an illegal (not at top level). Aborting. %c\n", '\a');
			exit(1); //the error which forced this was some bad atom length redetermination; probably won't be fixed
		}
		
		iter=parsedAtoms[iter].NextAtomNumber;
		if (iter == 0) {
			break;
		}
	}
	
	int percentage_difference = 0;
	
	if (simple_tally > (uint64_t)file_size) {
#if defined (_MSC_VER) /* seems a uint64_t can't be cast to a double under Windows */
		percentage_difference = (int)lroundf( (float)(file_size-removed_bytes_tally)/(float)((uint32_t)simple_tally) * 100 );	
	} else {
		percentage_difference = (int)lroundf( (float)((uint32_t)simple_tally)/(float)(file_size-removed_bytes_tally) * 100 );
		
#else

		percentage_difference = (int)lroundf( (float)(file_size-removed_bytes_tally)/(float)simple_tally * 100 );	
	} else {
		percentage_difference = (int)lroundf( (float)simple_tally/(float)(file_size-removed_bytes_tally) * 100 );
#endif
	}
	
	if (percentage_difference < 90 && file_size > 300000) { //only kick in when files are over 300k & 90% of the size
		fprintf(stderr, "AtomicParsley error: total existing atoms present as larger than filesize. Aborting. %c\n", '\a');
		exit(1); //a foobar2000 0.9 tagged file would also probably show up here
	}
	
	if (!atom_name_with_4_characters) {
		fprintf(stdout, "AtomicParsley warning: atom(s) were detected with atypical names containing NULLs\n");
	}
	return;
}

#if defined (DARWIN_PLATFORM)

//this bit of code is central to eliminating the need to set file extension for mpeg4 files (at least for the currently know codec types).
//we will count how many 'trak' atoms there are; for each one drill down to 'stsd' and return the the atom right after that
//the atom after 'stsd' will carry the 4cc code of the codec for the track - for example jpeg, alac, avc1 or mp4a
//if he have 'avc1', 'drmi', 'mp4v' for any of the returned atoms, then we have a VIDEO mpeg-4 file

//then using Cocoa calls, the Mac OS X specific TYPE/CREATOR code will be set to indicate to the OS/Finder/Applications/iTunes that
//this file is a audio/video file without having to change its extension.

void APar_TestTracksForKind() {
	uint8_t total_tracks = 0;
	uint8_t track_num = 0;
	short codec_atom = 0;

	//With track_num set to 0, it will return the total trak atom into total_tracks here.
	APar_TrackInfo(total_tracks, track_num, codec_atom);
	
	if (total_tracks > 0) {
		while (total_tracks > track_num) {
			track_num+= 1;
			
			//Now total_tracks != 0; this ships off which trak atom to test, and returns codec_atom set to the atom after stsd.
			APar_TrackInfo(total_tracks, track_num, codec_atom);
			
			//now test this trak's stsd codec against these 4cc codes:			
			switch(parsedAtoms[codec_atom].stsd_codec) {
				//video types
				case 0x61766331 : // "avc1"
					track_codecs.has_avc1 = true;
					break;
				case 0x6D703476 : // "mp4v"
					track_codecs.has_mp4v = true;
					break;
				case 0x64726D69 : // "drmi"
					track_codecs.has_drmi = true;
					break;
					
				//audio types
				case 0x616C6163 : // "alac"
					track_codecs.has_alac = true;
					break;
				case 0x6D703461 : // "mp4a"
					track_codecs.has_mp4a = true;
					break;
				case 0x64726D73 : // "drms"
					track_codecs.has_drms = true;
					break;
				
				//podcast types
				case 0x74657874 : // "text"
					track_codecs.has_timed_text = true;
					break;
				case 0x6A706567 : // "jpeg"
					track_codecs.has_timed_jpeg = true;
					break;
				
				//either podcast type (audio-only) or a QT-unsupported video file with subtitles
				case 0x74783367 : // "tx3g"
					track_codecs.has_timed_tx3g = true;
					break;
				
				//other
				case 0x6D703473 : // "mp4s"
					track_codecs.has_mp4s = true;
					break;
				case 0x72747020  : // "rtp "
					track_codecs.has_rtp_hint = true;
					break;
			}
		}
	}	
	return;
}
#endif

void APar_DeriveNewPath(const char *filePath, char* temp_path, int output_type, const char* file_kind) {
	char* suffix = strrchr(filePath, '.');
	
	if (strncmp(file_kind, "-dump-", 4) == 0) {
		strncpy(suffix, ".raw", 4);
	}
	
	size_t filepath_len = strlen(filePath);
	size_t base_len = filepath_len-strlen(suffix);
	strncpy(temp_path, filePath, base_len);
	memcpy(temp_path, filePath, base_len);
	
	memcpy(temp_path + base_len, file_kind, strlen(file_kind));

	char randstring[6];
	srand((int) time(NULL)); //Seeds rand()
	int randNum = rand()%100000;
	sprintf(randstring, "%i", randNum);

	memcpy(temp_path + strlen(temp_path), randstring, strlen(randstring));
	memcpy(temp_path + strlen(temp_path), suffix, strlen(suffix) );
	return;
}

void APar_MetadataFileDump(const char* m4aFile) {
	bool ilst_present = false;
	char* dump_file_name=(char*)malloc( sizeof(char)* (strlen(m4aFile) +12 +1) );
	memset(dump_file_name, 0, sizeof(char)* (strlen(m4aFile) +12 +1) );
	
	FILE* dump_file;
	AtomicInfo ilst_atom = APar_FindAtom("moov.udta.meta.ilst", false, false, false, true);
	
	//make sure that the atom really exists
	if (ilst_atom.AtomicNumber != 0) {
		if (strlen(ilst_atom.AtomicName) > 0) {
			if (strncmp(ilst_atom.AtomicName, "ilst", 4) == 0) {
				ilst_present = true;
			}
		}
	}
	
	if (ilst_present) {
		char* dump_buffer=(char*)malloc( sizeof(char)* ilst_atom.AtomicLength +1 );
		memset(dump_buffer, 0, sizeof(char)* ilst_atom.AtomicLength +1 );
	
		APar_DeriveNewPath(m4aFile, dump_file_name, 1, "-dump-");
		dump_file = APar_OpenFile(dump_file_name, "wb");
		if (dump_file != NULL) {
			//body of atom writing here
			
			fseek(source_file, ilst_atom.AtomicStart, SEEK_SET);
			fread(dump_buffer, 1, (size_t)ilst_atom.AtomicLength, source_file);
			
			fwrite(dump_buffer, (size_t)ilst_atom.AtomicLength, 1, dump_file);
			fclose(dump_file);
		
			fprintf(stdout, " Metadata dumped to %s\n", dump_file_name);
		}
		free(dump_buffer);
		dump_buffer=NULL;
		
	} else {
		fprintf(stdout, "AtomicParsley error: no ilst atom was found to dump out to file.\n");
	}
	
	return;
}

void APar_ShellProgressBar(uint32_t bytes_written) {
	strcpy(file_progress_buffer, " Progress: ");
	
	int display_progress = (int)lroundf( (float)bytes_written/(float)new_file_size * 100 *( (float)max_display_width/100) );
	int percentage_complete = (int)lroundf( (float)bytes_written/(float)new_file_size * 100 );
		
	for (int i = 0; i <= max_display_width; i++) {
		if (i < display_progress ) {
			strcat(file_progress_buffer, "=");
		} else if (i == display_progress) {
			sprintf(file_progress_buffer, "%s>%d%%", file_progress_buffer, percentage_complete);
		} else {
			strcat(file_progress_buffer, "-");
		}
	}
	strcat(file_progress_buffer, "|");
		
	fprintf(stdout, "%s\r", file_progress_buffer);
	fflush(stdout);
	return;
}

void APar_FileWrite_Buffered(FILE* dest_file, FILE *src_file, uint32_t dest_start, uint32_t src_start, uint32_t length, char* &buffer) {
	//fprintf(stdout, "I'm at %u\n", src_start);
	fseek(src_file, src_start, SEEK_SET);
	fread(buffer, 1, (size_t)length, src_file);

	fseek(dest_file, dest_start, SEEK_SET);
	fwrite(buffer, (size_t)length, 1, dest_file);
	return;
}

uint32_t APar_DRMS_WriteAtomically(FILE* temp_file, char* &buffer, char* &conv_buffer, uint32_t bytes_written_tally, short this_number) {
	uint32_t bytes_written = 0;

	char4TOuint32(parsedAtoms[this_number].AtomicLength, conv_buffer); //write the atom length
	fseek(temp_file, bytes_written_tally, SEEK_SET);
	fwrite(conv_buffer, 4, 1, temp_file);
	bytes_written += 4;
	
	fwrite(parsedAtoms[this_number].AtomicName, 4, 1, temp_file); //write the atom name
	bytes_written += 4;
	
	if (strncmp(parsedAtoms[this_number].AtomicName, "drmi", 4) == 0) {
		fseek(temp_file, bytes_written_tally + bytes_written, SEEK_SET);
	  fwrite(parsedAtoms[this_number].AtomicData, 78, 1, temp_file); //74
	  bytes_written += 78;
	} else {
		//if (strncmp(parsedAtoms[this_number].AtomicName, "drms", 4) == 0) {
	  fseek(temp_file, bytes_written_tally + bytes_written, SEEK_SET);
	  fwrite(parsedAtoms[this_number].AtomicData, 28, 1, temp_file);
	  bytes_written += 28;
	}
	return bytes_written;
}

uint32_t APar_WriteAtomically(FILE* source_file, FILE* temp_file, bool from_file, char* &buffer, char* &conv_buffer, uint32_t bytes_written_tally, short this_atom) {
	uint32_t bytes_written = 0;
	
	if (parsedAtoms[this_atom].AtomicLength > 1 && parsedAtoms[this_atom].AtomicLength < 8) { //prevents any spurious atoms from appearing
		return bytes_written;
	}
	
	//write the length of the atom first... taken from our tree in memory
	char4TOuint32(parsedAtoms[this_atom].AtomicLength, conv_buffer);
	fseek(temp_file, bytes_written_tally, SEEK_SET);
	fwrite(conv_buffer, 4, 1, temp_file);
	bytes_written += 4;
	
	//since we have already writen the length out to the file, it can be changed now with impunity
	if (parsedAtoms[this_atom].AtomicLength == 0) { //the spec says if an atom has a length of 0, it extends to EOF
		parsedAtoms[this_atom].AtomicLength = (uint32_t)file_size - parsedAtoms[this_atom].AtomicLength;
	} else if (parsedAtoms[this_atom].AtomicLength == 1) {
		//part of the pseudo 64-bit support
		parsedAtoms[this_atom].AtomicLength = (uint32_t)parsedAtoms[this_atom].AtomicLengthExtended;
	}
	
	//handle jpeg/pngs differently when we are ADDING them: they will be coming from entirely separate files
	//jpegs/png already present in the file get handled in the "if (from_file)" portion
	/*if (parsedAtoms[this_atom].AtomicData != NULL) {
		AtomicInfo parentAtom = parsedAtoms[ APar_FindParentAtom(parsedAtoms[this_atom].AtomicNumber, parsedAtoms[this_atom].AtomicLevel) ];
		fprintf(stdout, "Parent atom %s has child with data %s\n", parentAtom.AtomicName, parsedAtoms[this_atom].AtomicData);
	}*/
	if ( ((parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_JPEGBinary) || 
				(parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_PNGBinary)) && (parsedAtoms[this_atom].AtomicData != NULL) ) {
			
		fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
		
		if (parsedAtoms[this_atom].uuidAtomType) {
			fwrite("uuid", 4, 1, temp_file);
			bytes_written += 4;
		}
		
		fwrite(parsedAtoms[this_atom].AtomicName, 4, 1, temp_file); //write the atom name
		bytes_written += 4;
		
		char4TOuint32( (uint32_t)parsedAtoms[this_atom].AtomicDataClass, conv_buffer); //write the atom data class
		fwrite(conv_buffer, 4, 1, temp_file);
		bytes_written += 4;
		
		char4TOuint32( 0, conv_buffer); //write a 4 bytes of null space out
		fwrite(conv_buffer, 4, 1, temp_file);
		bytes_written += 4;
		
		//open the originating file...
		FILE *pic_file = NULL;
		pic_file = APar_OpenFile(parsedAtoms[this_atom].AtomicData, "rb");
		if (pic_file != NULL) {
			//...and the actual transfer of the picture
			while (bytes_written <= parsedAtoms[this_atom].AtomicLength) {
				if (bytes_written + max_buffer <= parsedAtoms[this_atom].AtomicLength ) {
					fseek(pic_file, bytes_written - 16, SEEK_SET);
					fread(buffer, 1, (size_t)max_buffer, pic_file);
				
					fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
					fwrite(buffer, (size_t)max_buffer, 1, temp_file);
					bytes_written += max_buffer;
					
					APar_ShellProgressBar(bytes_written_tally + bytes_written);
				
				} else { //we either came up on a short atom (most are), or the last bit of a really long atom
					//fprintf(stdout, "Writing atom %s from file directly into buffer\n", parsedAtoms[this_atom].AtomicName);
					fseek(pic_file, bytes_written - 16, SEEK_SET);
					fread(buffer, 1, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), pic_file);
				
					fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
					fwrite(buffer, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), 1, temp_file);
					bytes_written += parsedAtoms[this_atom].AtomicLength - bytes_written;
					
					APar_ShellProgressBar(bytes_written_tally + bytes_written);
				
					break;
				} //endif
			}//end while
			
		}//ends if(pic_file)
		fclose(pic_file);
		
		if (myPicturePrefs.removeTempPix && parsedAtoms[this_atom].tempFile ) {
			//reopen the picture file to delete if this IS a temp file (and the env pref was given)
			pic_file = APar_OpenFile(parsedAtoms[this_atom].AtomicData, "wb");
			
			if ( IsUnicodeWinOS() ) {
#if defined (_MSC_VER)
				wchar_t* utf16_pic_path = Convert_multibyteUTF8_to_wchar(parsedAtoms[this_atom].AtomicData);
		
				_wremove(utf16_pic_path);
		
				free(utf16_pic_path);
				utf16_pic_path = NULL;
#endif
			} else {
				remove(parsedAtoms[this_atom].AtomicData);
			}
			
			fclose(pic_file);
		}
				
	} else if (from_file) {
		// here we read in the original atom into the buffer. If the length is greater than our buffer length,
		// we loop, reading in chunks of the atom's data into the buffer, and immediately write it out, reusing the buffer.
		
		while (bytes_written <= parsedAtoms[this_atom].AtomicLength) {
			if (bytes_written + max_buffer <= parsedAtoms[this_atom].AtomicLength ) {
				//fprintf(stdout, "Writing atom %s from file looping into buffer\n", parsedAtoms[this_atom].AtomicName);
//fprintf(stdout, "Writing atom %s from file looping into buffer %u - %u | %u\n", parsedAtoms[this_atom].AtomicName, parsedAtoms[this_atom].AtomicLength, bytes_written_tally, bytes_written);
				//read&write occurs from & including atom name through end of atom
				fseek(source_file, (bytes_written + parsedAtoms[this_atom].AtomicStart), SEEK_SET);
				fread(buffer, 1, (size_t)max_buffer, source_file);
				
				fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
				fwrite(buffer, (size_t)max_buffer, 1, temp_file);
				bytes_written += max_buffer;
				
				APar_ShellProgressBar(bytes_written_tally + bytes_written);
				
			} else { //we either came up on a short atom (most are), or the last bit of a really long atom
				//fprintf(stdout, "Writing atom %s from file directly into buffer\n", parsedAtoms[this_atom].AtomicName);
				fseek(source_file, (bytes_written + parsedAtoms[this_atom].AtomicStart), SEEK_SET);
				fread(buffer, 1, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), source_file);
				
				fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
				fwrite(buffer, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), 1, temp_file);
				bytes_written += parsedAtoms[this_atom].AtomicLength - bytes_written;
				
				APar_ShellProgressBar(bytes_written_tally + bytes_written);
				
				break;
			}
		}
		
	} else { // we are going to be writing not from the file, but directly from the tree (in memory).

		//fprintf(stdout, "Writing atom %s from memory\n", parsedAtoms[this_atom].AtomicName);
		fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
		
		if (parsedAtoms[this_atom].uuidAtomType) {
			fwrite("uuid", 4, 1, temp_file);
			bytes_written += 4;
			//fprintf(stdout, "%u\n", parsedAtoms[this_atom].AtomicLength);
		}
		
		fwrite(parsedAtoms[this_atom].AtomicName, 4, 1, temp_file);
		bytes_written += 4;
		if (parsedAtoms[this_atom].AtomicDataClass >= 0) {
			char4TOuint32( (uint32_t)parsedAtoms[this_atom].AtomicDataClass, conv_buffer);
			fwrite(conv_buffer, 4, 1, temp_file);
			bytes_written += 4;
			
			//TODO: reimplement how AtomicData is stored so that any NULLs aren't depedant on the AtomicDataClass, but get put there at the time of creation
			//because tfhd had to be accommodated here
			if (parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_Text && strncmp(parsedAtoms[this_atom].AtomicName, "tfhd", 4) != 0) {
				char4TOuint32( 0, conv_buffer);
				fwrite(conv_buffer, 4, 1, temp_file);
				bytes_written += 4;
			}
		}
		
		if (parsedAtoms[this_atom].AtomicData != NULL) {
			uint32_t atom_data_size = 0;
			//fwrite(conv_buffer, 4, 1, temp_file);
			if (strncmp(parsedAtoms[this_atom].AtomicName, "stsd", 4) == 0) {
					atom_data_size = 4;
					
			} else {
				if (parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_Text && strncmp(parsedAtoms[this_atom].AtomicName, "tfhd", 4) != 0) {
					atom_data_size = parsedAtoms[this_atom].AtomicLength - 16;
				} else {
					atom_data_size = parsedAtoms[this_atom].AtomicLength - 12;
				}
				if (parsedAtoms[this_atom].uuidAtomType) {
					atom_data_size -= 4;
				}
			}
			//can't strlen on data that has nulls (and most non-udta atoms like the important stco have nulls)
			//fwrite(parsedAtoms[this_atom].AtomicData, strlen(parsedAtoms[this_atom].AtomicData)+1, 1, temp_file);
			//bytes_written += (uint32_t)(strlen(parsedAtoms[this_atom].AtomicData)+1);
			fwrite(parsedAtoms[this_atom].AtomicData, atom_data_size, 1, temp_file);
			bytes_written += atom_data_size;
			
			APar_ShellProgressBar(bytes_written_tally + bytes_written);
		}
	}
	
	return bytes_written;
}

void APar_WriteFile(const char* m4aFile, const char* outfile, bool rewrite_original) {
	char* temp_file_name=(char*)malloc( sizeof(char)* 3500 );
	char* file_buffer=(char*)malloc( sizeof(char)* max_buffer + 1 );
	char* data = (char*)malloc(sizeof(char)*4 + 1);
	FILE* temp_file;
	uint32_t temp_file_bytes_written = 0;
	short thisAtomNumber = 0;
	memset(temp_file_name, 0, sizeof(char)* 3500 );
	memset(file_buffer, 0, sizeof(char)* max_buffer + 1 );
	memset(data, 0, sizeof(char)*4 + 1);
	
	uint32_t mdat_position = APar_DetermineMediaData_AtomPosition();
	
	APar_ValidateAtoms();
	
	if (!outfile) {  //if (outfile == NULL) {  //if (strlen(outfile) == 0) { 
		APar_DeriveNewPath(m4aFile, temp_file_name, 0, "-temp-");
		temp_file = APar_OpenFile(temp_file_name, "wb");
		
#if defined (DARWIN_PLATFORM)
		APar_SupplySelectiveTypeCreatorCodes(m4aFile, temp_file_name); //provide type/creator codes for ".mp4" for randomly named temp files
#endif
		
	} else {
		//case-sensitive compare means "The.m4a" is different from "THe.m4a"; on certiain Mac OS X filesystems a case-preservative but case-insensitive FS exists &
		//AP probably will have a problem there. Output to a uniquely named file as I'm not going to poll the OS for the type of FS employed on the target drive.
		if (strncmp(m4aFile,outfile,strlen(outfile)) == 0 && (strlen(outfile) == strlen(m4aFile)) ) {
			//er, nice try but you were trying to ouput to the exactly named file of the original. Y'all ain't so slick
			APar_DeriveNewPath(m4aFile, temp_file_name, 0, "-temp-");
			temp_file = APar_OpenFile(temp_file_name, "wb");
			
#if defined (DARWIN_PLATFORM)
			APar_SupplySelectiveTypeCreatorCodes(m4aFile, temp_file_name); //provide type/creator codes for ".mp4" for a fall-through randomly named temp files
#endif
		
		} else {
			temp_file = APar_OpenFile(outfile, "wb");
			
#if defined (DARWIN_PLATFORM)
			APar_SupplySelectiveTypeCreatorCodes(m4aFile, outfile); //provide type/creator codes for ".mp4" for a user-defined output file
#endif
			
			}
	}
	
	if (temp_file != NULL) {
		//body of atom writing here
		
		fprintf(stdout, "\n Started writing to temp file.\n");
		
		while (true) {
			AtomicInfo thisAtom = parsedAtoms[thisAtomNumber];
			//the loop where the critical determination is made
			if (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "stco", 4) == 0) {
				bool readjusted_stco = APar_Readjust_STCO_atom(mdat_position, thisAtomNumber);
				
				temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, !readjusted_stco, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
					
			} else if (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "co64", 4) == 0) {
				bool readjusted_co64 = APar_Readjust_CO64_atom(mdat_position, thisAtomNumber);
				
				temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, !readjusted_co64, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				
			} else if (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "tfhd", 4) == 0) {
				bool readjusted_tfhd = APar_Readjust_TFHD_fragment_atom(mdat_position, thisAtomNumber);
				
				temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, !readjusted_tfhd, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				

			//AtomicDataClass comes into play for a missing "moov.udta.meta" atom, his has a data type class, but no actual data (or conversely, no type with 4 bytes of data
			} else if ( (thisAtom.AtomicData != NULL) || ( strncmp(thisAtom.AtomicName, "meta", 4)  == 0 && thisAtom.AtomicDataClass >= 0) ) {
				if ( (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "drms", 4) == 0) || 
				     (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "drmi", 4) == 0) || 
						 (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "mp4a", 4) == 0) || 
						 (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "alac", 4) == 0 && (parsedAtoms[thisAtomNumber].AtomicLevel == 7)) ) {
					temp_file_bytes_written += APar_DRMS_WriteAtomically(temp_file, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				} else {
					temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, false, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				}
			} else {
				//write out parent atoms (the standard kind that are only offset & name from the tree in memory (total: 8bytes)
				if ( APar_AtomHasChildren(thisAtomNumber) ) {
					//fprintf(stdout, "(Parent atom) ");
					temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, false, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				//or its a child (they invariably contain some sort of data.
				} else {
					temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, true, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
				}
			}
			if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) {
				//fprintf(stdout, "The loop is buh-rokin\n");
				break;
			}
			
			//prevent any looping back to atoms already written
			parsedAtoms[thisAtomNumber].AtomicNumber = -1;
			thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
		}
		fprintf(stdout, "\n Finished writing to temp file.\n");
		fclose(temp_file);
		
	} else {
		fprintf(stdout, "AtomicParsley error: an error occurred while trying to create a temp file.\n");
		exit(1);
	}
	
	if (rewrite_original && !outfile) { //disable overWrite when writing out to a specifically named file; presumably the enumerated output file was meant to be the final destination
		fclose(source_file);

#if defined (_MSC_VER) /* native windows seems to require removing the file first; rename() on Mac OS X does the removing automatically as needed */
		if ( IsUnicodeWinOS() ) {
			wchar_t* utf16_filepath = Convert_multibyteUTF8_to_wchar(m4aFile);
		
			_wremove(utf16_filepath);
		
			free(utf16_filepath);
			utf16_filepath = NULL;
		} else {
			remove(m4aFile);
		}
#endif
		int err = 0;
		
		if ( IsUnicodeWinOS() ) {
#if defined (_MSC_VER)
			wchar_t* utf16_filepath = Convert_multibyteUTF8_to_wchar(m4aFile);
			wchar_t* temp_utf16_filepath = Convert_multibyteUTF8_to_wchar(temp_file_name);
		
			err = _wrename(temp_utf16_filepath, utf16_filepath);
		
			free(utf16_filepath);
			free(temp_utf16_filepath);
			utf16_filepath = NULL;
			temp_utf16_filepath = NULL;
#endif
		} else {
			err = rename(temp_file_name, m4aFile);
		}
		
		if (err != 0) {
			switch (errno) {
				
				case ENAMETOOLONG: {
					fprintf (stdout, "Some or all of the orginal path was too long.");
					exit (-1);
				}
				case ENOENT: {
					fprintf (stdout, "Some part of the original path was missing.");
					exit (-1);
				}
				case EACCES: {
					fprintf (stdout, "Unable to write to a directory lacking write permission.");
					exit (-1);
				}
				case ENOSPC: {
					fprintf (stdout, "Out of space.");
					exit (-1);
				}
			}
		}
	}
	//APar_PrintAtomicTree();
	APar_FreeMemory();
	free(temp_file_name);
	temp_file_name=NULL;
	free(file_buffer);
	file_buffer = NULL;
	free(data);
	data=NULL;
	
	return;
}
