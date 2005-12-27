//==================================================================//
/*
    AtomicParsley - AtomicParsley.cpp

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

    ----------------------
    Code Contributions by:
		
    * Mike Brancato - Debian patches & build support
    * Lowell Stewart - null-termination bugfix for Apple compliance
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

#include "AtomicParsley.h"
#include "AtomicParsley_genres.h"
#include "AP_iconv.h"

#include "AP_NSImage.h"

///////////////////////////////////////////////////////////////////////////////////////
//                               Global Variables                                    //
///////////////////////////////////////////////////////////////////////////////////////

off_t file_size;

struct AtomicInfo parsedAtoms[250]; //max out at 250 atoms (most I've seen is 144 for an untagged mp4)
short atom_number = 0;
short generalAtomicLevel = 1;
bool file_opened = false;
FILE* source_file;
bool parsedfile = false;
bool alter_original = false;
bool flag_drms_atom = false;
bool Create__udta_meta_hdlr__atom = false;

long max_buffer = 4096*25;

long mdat_start=0;
long new_file_size = 0; //used for the progressbar

#if defined (WIN32)
short max_display_width = 45;
#else
short max_display_width = 75; //ah, figured out grub - vga=773 makes a whole new world open up
#endif
char* file_progress_buffer=(char*)malloc( sizeof(char)* (max_display_width+10) ); //+5 for any overflow in "%100", or "|"


struct PicPrefs myPicturePrefs;
bool parsed_prefs = false;

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
//                               Generic Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

#if defined (WIN32) && defined (__MINGW_H)
//Commented out because under mingw, AtomicParsley is broken
//Also, strsep is defined in glibc, and this marks the point where a ./configure & makefile combo would make this easier

/* Copyright (C) 1992, 93, 96, 97, 98, 99, 2004 Free Software Foundation, Inc.
   This strsep function is part of the GNU C Library - v2.3.5; LGPL.
*/

/* char *strsep (char **stringp, const char *delim)
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
} */
#endif

off_t findFileSize(const char *path) {
	struct stat fileStats;
	stat(path, &fileStats);
	
	return fileStats.st_size;
}

long longFromBigEndian(const char *string) {
#if defined (__ppc__) || defined (__ppc64__)
	long test;
	memcpy(&test,string,4);
	return test;
#else
	//oddly, this line worked for 16496, but failed at 5849 (which became -39 or something); moving right past bitshifting - 2 dozen cracks at it was enough
	//TODO: figure out bitshifting
	//return ((string[0] << 24) | (string[1] << 16) | (string[2] << 8) | string[3] << 0);
	long test;
	char shift_data[4];
	shift_data[0] = string[3];
	shift_data[1] = string[2];
	shift_data[2] = string[1];
	shift_data[3] = string[0];
	memcpy(&test,shift_data,4);
	return test;
#endif
}

void char4long(long lnum, char* data) {
	data[0] = (lnum >> 24) & 0xff;
	data[1] = (lnum >> 16) & 0xff;
	data[2] = (lnum >>  8) & 0xff;
	data[3] = (lnum >>  0) & 0xff;
	return;
}

void char4short(short snum, char* data) {
	data[0] = (snum >>  8) & 0xff;
	data[1] = (snum >>  0) & 0xff;
	return;
}

char* extractAtomName(char *fileData) {
	char *atom=(char *)malloc(sizeof(char)*5);

	for (int i=0; i < 4; i++) {
		atom[i] = fileData[i+4]; //we want the 4 byte 'atom' in data [4,5,6,7]
	}
	return atom;
	free(atom);
}

void openSomeFile(const char* file, bool open) {
	if ( open && !file_opened) {
		source_file = fopen(file, "r");
		if (file != NULL) {
			file_opened = true;
		}
	} else {
		fclose(source_file);
		file_opened = false;
	}
}

bool TestFileExistence(const char *filePath, bool errorOut) {
	bool file_present = false;
	FILE *a_file = NULL;
	a_file = fopen(filePath, "r");
	if( (a_file == NULL) && errorOut ){
		fprintf(stderr, "AtomicParsley error: can't open %s for reading: %s\n", filePath, strerror(errno));
		exit(1);
		} else {
			file_present = true;
			fclose(a_file);
		}
	return file_present;
}

void APar_FreeMemory() {
	for(int iter=0; iter < atom_number; iter++) {
		if (parsedAtoms[iter].AtomicData != NULL) {
			free(parsedAtoms[iter].AtomicData);
		}
	}
	return;
}

int APar_TestArtworkBinaryData(const char* artworkPath) {
	int artwork_dataType = 0;
	FILE *artfile = fopen(artworkPath, "r");
	if (artfile != NULL) {
		char *pic_data=(char *)malloc(sizeof(char)*9);
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
	}
	return artwork_dataType;
}

// enables writing out the contents of a single memory-resident atom out to a text file
void APar_AtomicWriteTest(short AtomicNumber, bool binary) {
	AtomicInfo anAtom = parsedAtoms[AtomicNumber];
	
	char* indy_atom_path = (char *)malloc(sizeof(char)*MAXPATHLEN);
	strcat(indy_atom_path, "/Users/");
	strcat(indy_atom_path, getenv("USER") );
	strcat(indy_atom_path, "/Desktop/singleton_atom.txt");

	FILE* single_atom_file;
	single_atom_file = fopen(indy_atom_path, "wr");
	if (single_atom_file != NULL) {
		
		if (binary) {
			fwrite(anAtom.AtomicData, (size_t)(anAtom.AtomicLength -12), 1, single_atom_file);
		} else {
			char* data = (char*)malloc(sizeof(char)*4);
			char4long(anAtom.AtomicLength, data);
		
			fwrite(data, 4, 1, single_atom_file);
			fwrite(anAtom.AtomicName, 4, 1, single_atom_file);
		
			char4long((long)anAtom.AtomicDataClass, data);
			fwrite(data, 4, 1, single_atom_file);

			fwrite(anAtom.AtomicData, strlen(anAtom.AtomicData)+1, 1, single_atom_file);
			free(data);
		}
	}
	fclose(single_atom_file);
	free(indy_atom_path);
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                            Locating/Finding Atoms                                 //
///////////////////////////////////////////////////////////////////////////////////////

AtomicInfo APar_FindParentAtom(int order_in_tree, short this_atom_level) {
	AtomicInfo thisAtom;
	for(int iter=order_in_tree-1; iter >= 0; iter--) {
		if (parsedAtoms[iter].AtomicLevel == this_atom_level-1) {
			thisAtom = parsedAtoms[iter];
			break;
		}
	}
	return thisAtom;
}

short APar_FindPrecedingAtom(AtomicInfo thisAtom) {
	short precedingAtom = 0;
	short iter = 0;
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		if (parsedAtoms[iter].NextAtomNumber == thisAtom.NextAtomNumber) {
			break;
		} else {
			precedingAtom = iter;
			iter=parsedAtoms[iter].NextAtomNumber;
		}
	}
	return precedingAtom;
}

short APar_FindPrecedingAtomNumber(short an_atom_num) {
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

AtomicInfo APar_FindAtom(const char* atom_name, bool createMissing, bool stringFromShell, bool findChild) {
	short present_atom_level = 1; //from where out generalAtomicLevel starts
	char* atom_hierarchy = strdup(atom_name);
	char* found_hierarchy = (char *)malloc(sizeof(char)*1000); //that should hold it
	for (int i=0; i<= 1000; i++) {
		found_hierarchy[i] = '\00';
	}
	
	AtomicInfo thisAtom;
	char* parent_name = (char *)malloc(sizeof(char)*4);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	while (search_atom_name != NULL) {
		
		if (stringFromShell) {
			StringReEncode(search_atom_name, "ISO-8859-1", "UTF-8");
		}
		
		//this could use a better technique to search through atoms.... it finds the first atom in the hierarchy with the same name, regardless of where it is
		for(int iter=0; iter < atom_number; iter++) {
			if (parsedAtoms[iter].NextAtomNumber == iter+1) {
				
				if ((strncmp(parsedAtoms[iter].AtomicName, search_atom_name, 4) == 0) && (parsedAtoms[iter].AtomicLevel == present_atom_level)) {
					
					if (atom_hierarchy == NULL) {
						//we have found the proper hierarchy we want
						AtomicInfo parent_atom = APar_FindParentAtom(parsedAtoms[iter].AtomicNumber, parsedAtoms[iter].AtomicLevel);

						if ( strncmp(parent_atom.AtomicName, parent_name, 4) == 0) {
							thisAtom = parsedAtoms[iter];
							break;
						} else {
							//we've come to the right level, & hieararchy, not strncmp through the atoms in this level
							int next_atom = parsedAtoms[iter].NextAtomNumber;
							while (parsedAtoms[next_atom].NextAtomNumber != 0) {
								
								if ( strncmp(parsedAtoms[next_atom].AtomicName, parent_name, 4) == 0) {
									if (findChild) {
										next_atom = parsedAtoms[next_atom].NextAtomNumber;
										thisAtom = parsedAtoms[next_atom]; //this is IT!!!
									} else {
										thisAtom = parsedAtoms[next_atom]; //this is IT!!!
									}
									break;
								} else {
									next_atom = parsedAtoms[next_atom].NextAtomNumber;
								}
							}
							break;
						}
					}
					if (atom_hierarchy != NULL) {
						present_atom_level++;
						break;
					}
				}	
			} else if (iter+1 == atom_number) {
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
			parent_name= strdup(search_atom_name);
			strcat(found_hierarchy, ".");
		}
		
		strcat(found_hierarchy, search_atom_name);
		search_atom_name = strsep(&atom_hierarchy,".");
	}
	atom_hierarchy = NULL;
	found_hierarchy = NULL;
	free(atom_hierarchy);
	free(found_hierarchy);
	free(parent_name);
	
  return thisAtom;
}

short APar_LocateParentHierarchy(const char* the_hierarchy) { //This only gets used when we are adding atoms at the end of the hierarchy
	short last_atom = 0;
	char* atom_hierarchy = strdup(the_hierarchy);
	char* search_atom_name = strsep(&atom_hierarchy,".");
	char* found_hierarchy = (char *)malloc(sizeof(char)*1000); //that should hold it
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
	
	parent_atom = APar_FindAtom(found_hierarchy, false, false, true);
	if (parent_atom.AtomicNumber < 0 || parent_atom.AtomicNumber > atom_number) {
		return (atom_number-1); 
	}
	
	short this_atom_num = parent_atom.NextAtomNumber;

	if ( (this_atom_num > atom_number) || (this_atom_num < 1) ) {
		last_atom = APar_FindEndingAtom();
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
	
	return last_atom;
}

AtomicInfo APar_LocateAtomInsertionPoint(const char* the_hierarchy, bool findLastChild) {
	//fprintf(stdout, "Searching for this path %s\n", the_hierarchy);
	AtomicInfo InsertionPointAtom;
	InsertionPointAtom.AtomicNumber=0;
	short present_atom_level = 1;
	int nextAtom = 0;
	int pre_parent_atom = 0;
	char* atom_hierarchy = strdup(the_hierarchy);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	
	while (search_atom_name != NULL) {
		AtomicInfo thisAtom;
		if ( (atom_hierarchy == NULL) && (present_atom_level == 1) ) {
			
			for (int i=0; i < atom_number; i++) {
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
	free(atom_hierarchy);	// A "Deallocation of a pointer not malloced" occured for a screwed up m4a file (for gnre & ©grp ONLY oddly)
	atom_hierarchy = NULL;
	
	if (InsertionPointAtom.NextAtomNumber == 0) {
		InsertionPointAtom = parsedAtoms[APar_LocateParentHierarchy(the_hierarchy)];
	}
	
	return InsertionPointAtom;
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

bool APar_AtomHasChildren(short thisAtom) {
	bool is_parent = false;
	if (parsedAtoms[parsedAtoms[thisAtom].NextAtomNumber].AtomicLevel > parsedAtoms[thisAtom].AtomicLevel) {
		is_parent = true;
	}
	return is_parent;
}

///////////////////////////////////////////////////////////////////////////////////////
//                         'data'/'stco' Atom extraction                             //
///////////////////////////////////////////////////////////////////////////////////////

void APar_AtomicRead(short this_atom_number) {
	//fprintf(stdout, "Reading %li bytes\n", parsedAtoms[this_atom_number].AtomicLength-12 );
	parsedAtoms[this_atom_number].AtomicData = (char*)malloc(sizeof(char)* (size_t)(parsedAtoms[this_atom_number].AtomicLength-12) );
	
	fseek(source_file, parsedAtoms[this_atom_number].AtomicStart+12, SEEK_SET);
	fread(parsedAtoms[this_atom_number].AtomicData, 1, parsedAtoms[this_atom_number].AtomicLength-12, source_file);
	return;
}

void APar_ExtractAAC_Artwork(AtomicInfo thisAtom, char* pic_output_path, short artwork_count) {
	char *base_outpath=(char *)malloc(sizeof(char)*MAXPATHLEN);
	strcpy(base_outpath, pic_output_path);
	strcat(base_outpath, "_artwork");
	sprintf(base_outpath, "%s_%d", base_outpath, artwork_count);
	
	char* art_payload = (char*)malloc( sizeof(char) * (thisAtom.AtomicLength-16) );
			
	fseek(source_file, thisAtom.AtomicStart+16, SEEK_SET);
	fread(art_payload, 1, thisAtom.AtomicLength-16, source_file);
	
	char* suffix = (char *)malloc(sizeof(char)*5);
	
	if (strncmp((char *)art_payload, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {//casts uchar* to char* (2)
				suffix = ".png";
	}	else if (strncmp((char *)art_payload, "\xFF\xD8\xFF\xE0", 4) == 0) {//casts uchar* to char* (2)
				suffix = ".jpg";
	}
	
	strcat(base_outpath, suffix);
	
	FILE *outfile = fopen(base_outpath, "wb");
	if (outfile != NULL) {
		fwrite(art_payload, (size_t)(thisAtom.AtomicLength-16), 1, outfile);
		fclose(outfile);
		fprintf(stdout, "Extracted artwork to file: %s\n", base_outpath);
	}
	free(base_outpath);
	free(art_payload);
	//free(suffix); 
  return;
}

void APar_ExtractDataAtom(int this_atom_number) {
	if ( source_file != NULL ) {
		AtomicInfo thisAtom = parsedAtoms[this_atom_number];
		char* genre_string;
		char* parent_atom_name=(char*)malloc( sizeof(char)*4);
		AtomicInfo parent_atom_stats = parsedAtoms[this_atom_number-1];
		parent_atom_name = parent_atom_stats.AtomicName;
		//fprintf(stdout, "\t\t\t parent atom %s\n", parent_atom_name);

		if (thisAtom.AtomicLength > 12 ) {
			char* data_payload = (char*)malloc( sizeof(char) * (thisAtom.AtomicLength-16) );
			for (int i=0; i<= thisAtom.AtomicLength-16; i++) {
				data_payload[i] = '\00';
			}
			
			fseek(source_file, thisAtom.AtomicStart+16, SEEK_SET);
			fread(data_payload, 1, thisAtom.AtomicLength-16, source_file);

			if (thisAtom.AtomicDataClass == AtomicDataClass_Text) {
				if (thisAtom.AtomicLength < 20 ) {
					//tvnn was showing up with 4 chars instead of 3; easier to null it out for now
					data_payload[thisAtom.AtomicLength-16] = '\00';
				}
				fprintf(stdout,"%s\n", data_payload);
				
			} else {
			
				char* primary_number_data = (char*)malloc( sizeof(char) * 4 );
				long primary_number = 0;
				long secondary_OF_number = 0;
				
				for (int i=0; i < 4; i++) {
							primary_number_data[i] = '\0';
				}
				
				if ( (strncmp(parent_atom_name, "trkn", 4) == 0) || (strncmp(parent_atom_name, "disk", 4) == 0) ) {
					char* secondary_OF_number_data = (char*)malloc( sizeof(char) * 4 );
					secondary_OF_number_data[0] = '\00';
					secondary_OF_number_data[1] = '\00';
					secondary_OF_number_data[2] = '\00';
					secondary_OF_number_data[3] = data_payload[5];
					secondary_OF_number = longFromBigEndian(secondary_OF_number_data);
					free(secondary_OF_number_data);
					
					for (int i=0; i < 4; i++) {
						primary_number_data[i] = data_payload[i]; //we want the 4 byte 'atom' in data [4,5,6,7]
					}
					primary_number = longFromBigEndian(primary_number_data);
					
					if (secondary_OF_number != 0) {
						fprintf(stdout, "%li of %li\n", primary_number, secondary_OF_number);
					} else {
						fprintf(stdout, "%li\n", primary_number);
					}
					
				} else if (strncmp(parent_atom_name, "gnre", 4) == 0) {
					if ( thisAtom.AtomicLength-16 < 3 ) { //oh, a 1byte int for genre number
						for (int i=0; i < 2; i++) {
							primary_number_data[i+2] = data_payload[i];
							GenreIntToString(&genre_string, longFromBigEndian(primary_number_data) );
						}
						fprintf(stdout,"%s\n", genre_string);
					}
					
				}	else if (strncmp(parent_atom_name, "tmpo", 4) == 0) {
					primary_number_data[2] = data_payload[0];
					primary_number_data[3] = data_payload[1];
					primary_number = longFromBigEndian(primary_number_data);
					fprintf(stdout, "%li\n", primary_number);

				} else if ( (strncmp(parent_atom_name, "cpil", 4) == 0) || (strncmp(parent_atom_name, "pcst", 4) == 0) ) {
					primary_number_data[3] = data_payload[0];
					primary_number = longFromBigEndian(primary_number_data);
					if (primary_number == 1) {
						fprintf(stdout, "true\n");
					} else {
						fprintf(stdout, "false\n");
					}
					
				} else if (strncmp(parent_atom_name, "stik", 4) == 0) { //no idea what this atom is; resembles cpil
					primary_number_data[3] = data_payload[0];
					primary_number = longFromBigEndian(primary_number_data);
					if (primary_number == 5) {
						fprintf(stdout, "Whacked Bookmarkable (only plays once per iTunes session)\n");
					} else if (primary_number == 6) {
						fprintf(stdout, "Music Video\n");
					} else if (primary_number == 10) {
						fprintf(stdout, "TV Show\n");
					} else {
						fprintf(stdout, "Movie\n");
					}

				} else if (strncmp(parent_atom_name, "rtng", 4) == 0) {
					primary_number_data[3] = data_payload[0];
					primary_number = longFromBigEndian(primary_number_data);
					if (primary_number == 2) {
						fprintf(stdout, "Clean Lyrics\n");
					} else if (primary_number != 0 ) {
						fprintf(stdout, "Explicit Lyrics\n");
					} else {
						fprintf(stdout, "Inoffensive\n");
					}
					
				} else if ( (strncmp(parent_atom_name, "purl", 4) == 0) || (strncmp(parent_atom_name, "egid", 4) == 0) ) {
					data_payload[thisAtom.AtomicLength-16] = '\00'; //purl & egid aren't null terminated
					fprintf(stdout,"%s\n", data_payload);
				
				} else {
					if (thisAtom.AtomicLength >= 20 ) {
					  for (int i=0; i < 4; i++) {
						  primary_number_data[i] = data_payload[i]; 
					  }
					  primary_number = longFromBigEndian(primary_number_data);
						fprintf(stdout, "%li\n", primary_number);
					} else {
					  fprintf(stdout, "%s\n", data_payload);
					}
				}
					
				free(primary_number_data);
				primary_number_data=NULL;
				free(data_payload);
				data_payload = NULL;
			}
		}
		free(parent_atom_name);
	}
	return;
}

void APar_PrintDataAtoms(const char *path, bool extract_pix, char* pic_output_path) {

#if defined (USE_ICONV_CONVERSION)
	fprintf(stdout, "\xEF\xBB\xBF"); //Default to a UTF-8 BOM; maybe UTF-16 one day... but not yet
#endif

	short artwork_count=0;
	for (int i=0; i < atom_number; i++) { 
		AtomicInfo thisAtom = parsedAtoms[i];
		char* atom_name = (char*)malloc(sizeof(char)*4);
		strncpy(atom_name, thisAtom.AtomicName, 4);
		if ( strncmp(atom_name, "data", 4) == 0 ) {
		
			//we want to print out the parent atom's name, not "data", that would be retarded (also, I tried that way first).
			char* parent_atom = (char*)malloc(sizeof(char)*4);
			for (int j=0; j<= 4; j++) {
				parent_atom[j] = '\00';
			}
			
			AtomicInfo parent = APar_FindParentAtom(i, thisAtom.AtomicLevel);
			strncpy(parent_atom, parent.AtomicName, 4);
			
#if defined (USE_ICONV_CONVERSION)
			StringReEncode(parent_atom, "UTF-8", "ISO-8859-1");
#endif
			
			if ( (thisAtom.AtomicDataClass == AtomicDataClass_Integer ||
            thisAtom.AtomicDataClass == AtomicDataClass_Text || 
            thisAtom.AtomicDataClass == AtomicDataClass_CPIL_TMPO) && !extract_pix ) {
				fprintf(stdout, "Atom \"%s\" contains: ", parent_atom);
				APar_ExtractDataAtom(i);
			} else if (strncmp(parent_atom,"covr", 4) == 0) {
				artwork_count++;
				if (extract_pix) {
					APar_ExtractAAC_Artwork(thisAtom, pic_output_path, artwork_count);
				}
			} else {
				//fprintf(stdout, "parent atom %s had data atom (of class:) %i\n ", parent_atom, thisAtom.AtomicDataClass);
			}
			free(parent_atom);
		}
		free(atom_name);
	}
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

void APar_AtomizeFileInfo(AtomicInfo &thisAtom, long Astart, long Alength, char* Astring, short Alevel, int Aclass, int NextAtomNum) {
	thisAtom.AtomicStart = Astart;
	thisAtom.AtomicLength = Alength;
	
	thisAtom.AtomicName = (char*)malloc(sizeof(char)*4);
	strcpy(thisAtom.AtomicName, Astring);
	thisAtom.AtomicNumber = atom_number;
	thisAtom.AtomicLevel = Alevel;
	thisAtom.AtomicDataClass = Aclass;
	
	//set the next atom number of the PREVIOUS atom (we didn't know there would be one until now); this is our default normal mode
	if (( NextAtomNum == 0 ) && ( atom_number !=0 )) {
		parsedAtoms[atom_number-1].NextAtomNumber = atom_number;
	}
	thisAtom.NextAtomNumber=0; //this could be the end... (we just can't quite say until we find another atom)
	
	//handle our oddballs
	if (strncmp(Astring, "meta", 4) == 0) {
		thisAtom.AtomicDataClass = AtomicDataClass_Integer;
	}
	
	atom_number++; //increment to the next AtomicInfo array
	
	if ( (strncmp(Astring, "mdat", 4) == 0) && (Alevel == 1) && (Alength > 16) ) {
		mdat_start = Astart;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                                 Atom Tree                                         //
///////////////////////////////////////////////////////////////////////////////////////

//this function reflects the atom tree as it stands in memory accurately (so I hope).
void APar_PrintAtomicTree() {
#if defined (USE_ICONV_CONVERSION)
	fprintf(stdout, "\xEF\xBB\xBF"); //UTF-8 BOM
#endif
	char* tree_padding = (char*)malloc(sizeof(char)*126); //for a 25-deep atom tree (4 spaces per atom)+single space+term.
	long freeSpace = 0;
	long aacData = 0;
	short thisAtomNumber = 0;
	
	//loop through each atom in the struct array (which holds the offset info/data)
 	while (parsedAtoms[thisAtomNumber].NextAtomNumber != 0) { 
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber]; 
		
		strcpy(tree_padding, "");
		if ( thisAtom.AtomicLevel != 1 ) {
			for (int pad=1; pad < thisAtom.AtomicLevel; pad++) {
				strcat(tree_padding, "    "); // if the atom depth is over 1, then add spaces before text starts to form the tree
			}
			strcat(tree_padding, " "); // add a single space
		}

#if defined (USE_ICONV_CONVERSION)
		StringReEncode(thisAtom.AtomicName, "UTF-8", "ISO-8859-1");
#endif
		
		fprintf(stdout, "%sAtom %s @ %li of size: %li, ends @ %li\n", tree_padding, thisAtom.AtomicName, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
		
		//simple tally & percentage of free space info
		if (strncmp(thisAtom.AtomicName, "free", 4) == 0) {
			freeSpace=freeSpace+thisAtom.AtomicLength;
		}
		//this is where the *raw* audio file is, the rest if fluff.
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLength > 100) ) {
			aacData = thisAtom.AtomicLength;
		}
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}
	if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) { 
		
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber]; 
		
		strcpy(tree_padding, "");
		if ( thisAtom.AtomicLevel != 1 ) {
			for (int pad=1; pad < thisAtom.AtomicLevel; pad++) {
				strcat(tree_padding, "    "); // if the atom depth is over 1, then add spaces before text starts to form the tree
			}
			strcat(tree_padding, " "); // add a single space
		}
		fprintf(stdout, "%sAtom %s @ %li of size: %li, ends @ %li\n", tree_padding, thisAtom.AtomicName, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
		
		//simple tally & percentage of free space info
		if (strncmp(thisAtom.AtomicName, "free", 4) == 0) {
			freeSpace=freeSpace+thisAtom.AtomicLength;
		}
		//this is where the *raw* audio file is, the rest if fluff.
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLength > 100) ) {
			aacData = thisAtom.AtomicLength;
		}
	}
		
	fprintf(stdout, "------------------------------------------------------\n");
	fprintf(stdout, "Total size: %i bytes; %i atoms total. AtomicParsley v%s\n", (int)file_size, atom_number-1, AtomicParsley_version);
	fprintf(stdout, "Media data: %li bytes; %li bytes all other atoms (%2.3f%% atom overhead).\n", 
												aacData, (long)(file_size - aacData), (float)(file_size - aacData)/(float)file_size * 100 );
	fprintf(stdout, "Total free atoms: %li bytes; %2.3f%% waste.\n", freeSpace, (float)freeSpace/(float)file_size * 100 );
	fprintf(stdout, "------------------------------------------------------\n");
	
	free(tree_padding);
	tree_padding = NULL;
		
	return;
}

void APar_SimpleAtomPrintout() {
	//loop through each atom in the struct array (which holds the offset info/data)
	fprintf(stdout, "\xEF\xBB\xBF"); //UTF-8 BOM
 	for (int i=0; i < atom_number; i++) { 
		AtomicInfo thisAtom = parsedAtoms[i]; 
		
		fprintf(stdout, "%i  -  Atom \"%s\" (level %i) has next atom at #%i\n", i, thisAtom.AtomicName, thisAtom.AtomicLevel, thisAtom.NextAtomNumber);
	}
	fprintf(stdout, "Total of %i atoms.\n", atom_number-1);
}

void APar_PrintAtomicDataTree() {
	char* tree_padding = (char*)malloc(sizeof(char)*126); //for a 25-deep atom tree (4 spaces per atom)+single space+term.
	short thisAtomNumber = 0;
	
	//loop through the number of atoms, starting at 1 and following the trail of NextDataAtom in the struct
	while (parsedAtoms[thisAtomNumber].NextAtomNumber != 0) { 
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber]; 
		
		strcpy(tree_padding, "");
		if ( thisAtom.AtomicLevel != 1 ) {
			for (int pad=1; pad < thisAtom.AtomicLevel; pad++) {
				strcat(tree_padding, "    "); // if the atom depth is over 1, then add spaces before text starts to form the tree
			}
			strcat(tree_padding, " "); // add a single space
		}
		fprintf(stdout, "%i%sAtom %s has data: %s\n", thisAtom.AtomicNumber, tree_padding, thisAtom.AtomicName, thisAtom.AtomicData);
		
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}
	free(tree_padding);
	tree_padding = NULL;
	
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                      File scanning & atom parsing                                 //
///////////////////////////////////////////////////////////////////////////////////////

short APar_GetCurrentAtomDepth(long atom_start, long atom_length) {
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

bool APar_TestforChildAtom(char *fileData, long sizeofParentAtom, char* atom) {
	if ( strncmp(atom, "data", 4) == 0  || strncmp(atom, "mdat", 4) == 0 ){
		return false;
	}
	
	char *childAtomLength = (char *)malloc(sizeof(char)*4);
	for (int i=0; i < 4; i++) {
		childAtomLength[i] = fileData[i+8]; //we want the 4 byte 'atom' in data [4,5,6,7]
	}
	
	long sizeofChild = longFromBigEndian(childAtomLength);
	
	if ( sizeofChild > 8 && sizeofChild < sizeofParentAtom ) {
		free(childAtomLength);
		return true;
	} else {
		free(childAtomLength);
	  return false;
	}
}

short APar_DetermineDataType(char* atom) {
	
	char*data_type = (char*)malloc(sizeof(char)*4);
	for (int i=0; i < 4; i++) {
		data_type[i] = atom[i+8]; //we want the 1 byte code (01, 15) following the data atom to determine type (text, binary)
	}
	long type_of_data = longFromBigEndian(data_type);
	
	free(data_type);
	return (int)type_of_data;
}

void APar_Parse_stsd_Atoms(FILE* file, long midJump, long drmLength) {
	//fprintf(stdout,"---> drms atom %s begins #: %li \t to %li\n", parsedAtoms[atom_number-1].AtomicName, midJump, drmLength);
	//stsd atom carrys data (8bytes )
	short stsd_entry_atom_number = atom_number-1;
	long stsd_entry_pos = midJump;
	char *data = (char *) malloc(12);
	long interDataSize = 0;
	long stsd_progress = 16;
	short atomLevel = generalAtomicLevel+1;
	fseek(file, midJump, SEEK_SET);
	
	//this now works anywhere the stsd atom is situated
	while ( stsd_progress < drmLength) {
		fread(data, 1, 12, file);
		char *atom = extractAtomName(data);
		interDataSize = longFromBigEndian(data);
		
		if ( interDataSize > drmLength || interDataSize < 8) {
			break; //we only get here if there is some oddball atom here under stsd that has a dataclass
		}
				
		APar_AtomizeFileInfo(parsedAtoms[atom_number], midJump, interDataSize, atom, atomLevel, -1, 0);
		
		if (strncmp(atom, "drms", 4) == 0) {
			//this needs to be done in order to maintain integrity of modified files
			parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28);
			fseek (file, midJump+8, SEEK_SET);
			fread(parsedAtoms[atom_number-1].AtomicData, 1, 28, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_Integer;
			//APar_AtomicWriteTest(atom_number-1, true);
			midJump += 36; //drms is so odd.... it contains data so it should *NOT* have any child atoms, and yet...
										 // 983bytes (and the next atom 36 bytes away) says that it *IS* a parent atom.... very odd indeed.
			stsd_progress += 36;		
			atomLevel++;
			flag_drms_atom = true;
			
		} else if (strncmp(atom, "drmi", 4) == 0) { //TODO TODO TODO: just as a drmi atom is 86 bytes, so is avc1 (hex length of 0x83
			//a new drm atom in a different trkn than the first - appeared (first for me) in an iTMS TV Show episode (Lost 209)
			parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*78); //74
			fseek (file, midJump+8, SEEK_SET); //12
			fread(parsedAtoms[atom_number-1].AtomicData, 1, 78, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
			midJump += 86;
			stsd_progress += 86;	
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_Integer;
			atomLevel++;
			flag_drms_atom = true; //although almost assuredly, we already set this to true by finding a "drms" atom before this atom
			
		} else if ( (strncmp(atom, "mp4a", 4) == 0) || ( (strncmp(atom, "alac", 4) == 0) && (atomLevel == 7) ) ) {
				parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28);
				fseek (file, midJump+8, SEEK_SET); //fseek to just after the atom name
				fread(parsedAtoms[atom_number-1].AtomicData, 1, 28, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
				parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_Integer;
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
		fseek(file, midJump, SEEK_SET);
	}

	parsedAtoms[stsd_entry_atom_number].AtomicData = (char *)malloc(sizeof(char)*4 );
	fseek(file, stsd_entry_pos+12, SEEK_SET);
	fread(parsedAtoms[stsd_entry_atom_number].AtomicData, 4, 1, file);
	parsedAtoms[stsd_entry_atom_number].AtomicDataClass = AtomicDataClass_Integer;

	free(data);
	return;
}

void APar_ScanAtoms(const char *path) {
	if (!parsedfile) {
		file_size = findFileSize(path);
		
		FILE *file = fopen(path, "r");
		if (file != NULL)
		{
			char *data = (char *) malloc(12);
			if (data == NULL) return;
			long dataSize = 0;
			long jump = 0;
			
			fread(data, 1, 12, file);
			char *atom = extractAtomName(data);
			//fprintf(stdout, "atom: %s @ offset: %li ", atom, jump);
			if ( (strncmp(atom, "ftyp", 4) == 0 ) || (strncmp(atom, "jP  ", 4) == 0) ) //jpeg2000 files will be "jP  " (2 spaces)
			{
				dataSize = longFromBigEndian(data);
				jump = dataSize;
				
				APar_AtomizeFileInfo(parsedAtoms[atom_number], 0, jump, atom, generalAtomicLevel, -1, 0);
				
				fseek(file, jump, SEEK_SET);
				
				while (jump < (long)file_size) {
					//a jpg2000 file will have a "jp2c" atom of 0000 jump length (don't know the rest).
					fread(data, 1, 12, file);
					char *atom = extractAtomName(data);
					int atom_class = -1;
					//fprintf(stdout, "atom: %s @ offset: %li \n", atom, jump);
					
					if (strncmp(atom, "data", 4) == 0) {
						atom_class=APar_DetermineDataType(data);
					} else {
						atom_class = -1;
					}
					
					dataSize = longFromBigEndian(data);
					
					if (dataSize == 0) { //terminate at quicktime's 4byte null termination
						break;
					}
					
					APar_AtomizeFileInfo(parsedAtoms[atom_number], jump, dataSize, atom, generalAtomicLevel, atom_class, 0);
					
					if (strncmp(atom, "stsd", 4) == 0) {
						//For now, this will be treated as a special scenario, and it is... odd... and mostly working
						APar_Parse_stsd_Atoms(file, jump+16, dataSize);
					}
					
					if (strncmp(atom, "meta", 4) == 0) {
						jump += 12;
					} else if ( strncmp(atom, "tkhd", 4) == 0 ) {
            jump += dataSize; //tkhd atoms are always 92 bytes long; don't even bother to test for any children
					} else if ( APar_TestforChildAtom(data, dataSize, atom) ) { // if bytes 9-12 are less than bytes 1-4 (and not 0) we have a child; if its a data atom, all bets are off
						jump += 8; //skip a head a grand total of... 8 *WHOLE* bytes - what progress!
					} else if ( generalAtomicLevel > 1 ) { // apparently, we didn't have a child
						jump += dataSize;
					} else {
						jump += dataSize;
					}
					generalAtomicLevel = APar_GetCurrentAtomDepth(jump, dataSize);
					
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
			fclose(file);
		}
		parsedfile = true;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Removal Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_RemoveAtom(const char* atom_path, bool shellAtom) {
	modified_atoms = true;
	AtomicInfo desiredAtom = APar_FindAtom(atom_path, false, shellAtom, false);
	
	char* atom_hierarchy = strdup(atom_path);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	while (atom_hierarchy != NULL) {
		search_atom_name = strsep(&atom_hierarchy,".");
	}
	
  if ( (desiredAtom.AtomicName != NULL) && (search_atom_name != NULL) ) {
    if (strncmp(search_atom_name, desiredAtom.AtomicName, 4) == 0) { //only remove an atom we have a matching name for	
      short preceding_atom_pos = APar_FindPrecedingAtom(desiredAtom);
      AtomicInfo endingAtom = APar_LocateAtomInsertionPoint(atom_path, true);
      if (endingAtom.AtomicNumber != 0) {
        //leaves the unwanted atoms in place, but NextAtomNumber skips around those atoms, and won't be used anymore.
        parsedAtoms[preceding_atom_pos].NextAtomNumber = endingAtom.NextAtomNumber;
      } else {
        //fprintf(stdout, "i have nothing to remove.\n");
      }
    }
    free(atom_hierarchy);
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

void APar_Move_mdat_Atoms() {
	short iter = 0;
	while (parsedAtoms[iter].NextAtomNumber != 0) {
		iter=parsedAtoms[iter].NextAtomNumber;
		if ( (strncmp(parsedAtoms[iter].AtomicName, "mdat", 4) == 0) && (parsedAtoms[iter].AtomicLevel == 1) ) {
			APar_MoveAtom(iter, 0);
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Creation Functions                                  //
///////////////////////////////////////////////////////////////////////////////////////

void APar_EncapulateData(AtomicInfo anAtom, const char* atomData, short binaryData[], const int atomDataClass) {
	short thisnum = anAtom.AtomicNumber;
	//fprintf(stdout, "Working on atom %s, num %i\n", anAtom.AtomicName, anAtom.AtomicNumber);
	//fprintf(stdout, "Working on atom %s, num %i\n", parsedAtoms[thisnum].AtomicName, parsedAtoms[thisnum].AtomicNumber);
	bool picture_exists = false;
	off_t picture_size = 0;
	size_t data_length;
	
	switch (atomDataClass) {
	
		case AtomicDataClass_JPEGBinary : //read picture data from file; test for existence first
			picture_exists = TestFileExistence(atomData, false);
			picture_size = findFileSize(atomData);
			if (picture_exists && picture_size > 0) {
				//"data" atoms under "covr" are: 4bytes(LONG atom length) + 4bytes(CHAR atom name = 'data') + 4bytes (INT data type) + 4 bytes (NULL space 00 00 00 00)
				parsedAtoms[thisnum].AtomicLength = (long)picture_size + 16;
				parsedAtoms[thisnum].AtomicData = strdup(atomData);
			}
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_JPEGBinary;
			break;
			
		case AtomicDataClass_PNGBinary : //read picture data from file; test for existence first
			picture_exists = TestFileExistence(atomData, false);
			picture_size = findFileSize(atomData);
			//the file won't be read until we have to write out the m4a file since it will be included unaltered
			if (picture_exists && picture_size > 0) {
				parsedAtoms[thisnum].AtomicLength = (long)picture_size + 16;
				parsedAtoms[thisnum].AtomicData = strdup(atomData);
			}
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_PNGBinary;
			break;
			
		case AtomicDataClass_Text :
			data_length = strlen(atomData);
			
			parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* data_length);
			if ( data_length >= 8 ) {
				memcpy(parsedAtoms[thisnum].AtomicData, atomData, data_length );
			} else {
				for (int i = 0; i <= (int)data_length; i++) {
					parsedAtoms[thisnum].AtomicData[i] = atomData[i];
				}
			}
			//fprintf(stdout, "(AP_E1) atom %s, size: %li\n", parsedAtoms[38].AtomicName, parsedAtoms[38].AtomicLength);
			parsedAtoms[thisnum].AtomicLength = (long)data_length + 12 + 4;
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_Text;
			
			//APar_AtomicWriteTest(thisnum, false);
			//fprintf(stdout, "(AP_E2) atom %s, size: %li\n", parsedAtoms[38].AtomicName, parsedAtoms[38].AtomicLength);
			break;
			
		case AtomicDataClass_Integer :
			//the first member of the binaryData = # of members (sizeof on arrays become pointers & won't work when passed to a function)
			if ( strncmp(anAtom.AtomicName, "hdlr",4) == 0 ) {
				
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* 21);
				//apparently, a TODO item is to revisit how AtomicData is stored/input; this is repulsive (but effective)
				memcpy(parsedAtoms[thisnum].AtomicData, "\x00\x00\x00\x00\x6D\x64\x69\x72\x61\x70\x70\x6C\x00\x00\x00\x00\x00\x00\x00\x00\x00", 21 ); 
				parsedAtoms[thisnum].AtomicLength = 33;
				
			} else {
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* ((binaryData[0]-1) * sizeof(short))  ); // for genre: 3 * 2
				
				// set up a 2byte(2 char) buffer for short to char conversions, then copy each character into the main AtomicData char.
				char *short_buf= (char*)malloc(sizeof(char)* 2);
				for (short i = 0; i < binaryData[0]-1; i++) {
					char4short(binaryData[i+1], short_buf);
					parsedAtoms[thisnum].AtomicData[2 * i] = short_buf[0];
					parsedAtoms[thisnum].AtomicData[(2 * i) + 1] = short_buf[1];
				}
				free(short_buf);
				
				parsedAtoms[thisnum].AtomicLength = (binaryData[0]-1) *2 + 12;
			}
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_Integer;
			break;
			
		case AtomicDataClass_CPIL_TMPO :
			//it's identical(for now) to the integer class - for those atoms like tmpo(bpm) that even go here - cpil/rtng doesn't
			//it will be separate from the integer class for clarity's sake
			
			//the first member of the binaryData = # of members (sizeof on arrays become pointers & won't work when passed to a function)
			parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* ((binaryData[0]-1) * sizeof(short))  ); // for genre: 3 * 2
			
			// set up a 2byte(2 char) buffer for short to char conversions, then copy each character into the main AtomicData char.
			char *small_buf= (char*)malloc(sizeof(char)* 2);
			for (short i = 0; i < binaryData[0]-1; i++) {
				char4short(binaryData[i+1], small_buf);
				parsedAtoms[thisnum].AtomicData[2 * i] = small_buf[0];
				parsedAtoms[thisnum].AtomicData[(2 * i) + 1] = small_buf[1];
			}
			free(small_buf);
			
			parsedAtoms[thisnum].AtomicLength = (binaryData[0]-1) *2 + 12;
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_Integer;

			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_CPIL_TMPO;
			break;
	}

	return;
}

AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name, 
                                 char* remaining_hierarchy, short atom_level, bool asLastChild) {
	//the end boolean value below tells the function to locate where that atom (and its children) end
	AtomicInfo KeyInsertionAtom = APar_LocateAtomInsertionPoint(present_hierarchy, asLastChild);
	bool atom_shunted = false; //only shunt the NextAtomNumber once (for the first atom that is missing.
	int continuation_atom_number = 0;
	AtomicInfo new_atom;
	//fprintf(stdout, "Our KEY insertion atom is \"%s\" for: %s(%s)\n", KeyInsertionAtom.AtomicName, present_hierarchy, remaining_hierarchy);
	continuation_atom_number = KeyInsertionAtom.NextAtomNumber;
	
	while (new_atom_name != NULL) {
		//the atom should be created first, so that we can determine these size issues first
		//new_atom_name = strsep(&missing_hierarchy,".");
		//fprintf(stdout, "At %s, adding new atom \"%s\" at level %i, after %s\n", present_hierarchy, new_atom_name, atom_level, parsedAtoms[APar_FindPrecedingAtomNumber(KeyInsertionAtom.AtomicNumber)].AtomicName);

		new_atom = parsedAtoms[atom_number];
		
		new_atom.AtomicStart = 0;
		
		new_atom.AtomicName = (char*)malloc(sizeof(char)*6);
		strcpy(new_atom.AtomicName, new_atom_name);
		new_atom.AtomicNumber = atom_number;
		
		if ( strncmp("meta", new_atom_name, 4) == 0 ) {
			//Though not explicitly tested here, the proper "meta" should have a definitive hierarchy of "moov.udta.meta"
			//"meta" is some oddball that is a container AND carries 4 bytes of null data; maybe its version or something
			//make sure to remember that if AtomicDataClass is >=0, to include 4 bytes in the AtomicLength
			new_atom.AtomicDataClass = AtomicDataClass_Integer;
		} else {
			new_atom.AtomicDataClass = -1;
		}
		
		new_atom.AtomicLevel = atom_level;
		
		if (!atom_shunted) {
			for (int i=0; i <= atom_number; i++) { //loop through our atoms looking for the KeyInsertionAtom (its NextAtomNumber)
				//fprintf(stdout, "We're looking for next number of \"%i\"", continuation_atom_number);
				//we want to insert right AFTER the insertion point
				if (parsedAtoms[i].NextAtomNumber == KeyInsertionAtom.NextAtomNumber) {
					parsedAtoms[i].NextAtomNumber = atom_number;
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
			if ( (strncmp(new_atom_name, "meta", 4) == 0) && (strncmp(remaining_hierarchy, "ilst", 4) == 0) ) { //causes a bus error
				//fprintf(stdout, "a hdlr atom needs to be created\n");
				if (!Create__udta_meta_hdlr__atom) {
					Create__udta_meta_hdlr__atom = true;
				}
			}
		} //ends create hdlr section
		
	}
	
	return new_atom;
}

void APar_Verify__udta_meta_hdlr__atom() {
	const char* udta_meta_hdlr__atom = "moov.udta.meta.hdlr";
	AtomicInfo hdlrAtom = APar_FindAtom(udta_meta_hdlr__atom, false, false, false);
	//argh, for whatever freason, that FindAtom finds "meta" when it exits, not "hdlr"; TODO: fix that
	hdlrAtom = parsedAtoms[hdlrAtom.NextAtomNumber];
	
	if (hdlrAtom.AtomicName != NULL) {
		if ( strncmp(hdlrAtom.AtomicName, "hdlr", 4) != 0 ) {
			Create__udta_meta_hdlr__atom = true;
		}
	} else {
		Create__udta_meta_hdlr__atom = true;
	}
	return;
}

void APar_AddMetadataInfo(const char* m4aFile, const char* atom_path, const int dataType, const char* atomPayload, bool shellAtom) {
	modified_atoms = true;
	//shellAtom is only for iconv use - © can be \302\251, \251 or who even knows in other encodings
	if ( strlen(atomPayload) == 0) {
		APar_RemoveAtom(atom_path, shellAtom); //find the atom; don't create if it's "" to remove
		//APar_PrintAtomicTree();
	} else {
		AtomicInfo desiredAtom = APar_FindAtom(atom_path, true, shellAtom, true); //finds the atom; if not present, creates the atom
		
		AtomicInfo parent_atom = APar_FindParentAtom(desiredAtom.AtomicNumber, desiredAtom.AtomicLevel);
		
		if (dataType == AtomicDataClass_Text) {
			APar_EncapulateData(desiredAtom, atomPayload, 0, dataType);
			
		} else if (dataType == AtomicDataClass_Integer) {
			//determine what kinds of numbers we have before we go onto working at the atom level
			short pos_in_total = 0;
			short the_total = 0;
			if (strrchr(atomPayload, '/') != NULL) {
				
				char* duplicate_info = strdup(atomPayload);
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hd", &pos_in_total); //sscanf into a short int
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hd", &the_total);
				
			} else {
				sscanf(atomPayload, "%hd", &pos_in_total);
			}
			
			//tracknumber (trkn.data)
			if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "trkn", 4) == 0 ) {
				short track_data[7] = {0, 0, 0, 0, pos_in_total, the_total, 0}; // number of elements + 6 shorts used in atom
				track_data[0] = (short)sizeof(track_data)/sizeof(short);
				APar_EncapulateData(desiredAtom, NULL, track_data, dataType);
				
			//disknumber (disk.data)
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "disk", 4) == 0 ) {
				short disk_data[6] = {0, 0, 0, 0, pos_in_total, the_total}; // number of elements + 5 shorts used in atom
				disk_data[0] = (short)sizeof(disk_data)/sizeof(short);
				APar_EncapulateData(desiredAtom, NULL, disk_data, dataType);

			}
			
		} else if (dataType == AtomicDataClass_CPIL_TMPO) {
			if ( (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "cpil", 4) == 0) ||
					 (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "pcst", 4) == 0) ) {
				//compilations is 5 bytes of data after the data class.... no great way to handle that....
				parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x01");
				parsedAtoms[desiredAtom.AtomicNumber].AtomicDataClass = dataType;
				parsedAtoms[desiredAtom.AtomicNumber].AtomicLength = 12 + 4 +1;  //offset + name + class + 4bytes null + \01
				//APar_AtomicWriteTest(desiredAtom.AtomicNumber, true); //only the first byte will be valid
				
			} else if ( ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "stik", 4) == 0 ) || 
									( strncmp(parent_atom.AtomicName, "stik", 4) == 0) ) {
				//compilations is 5 bytes of data after the data class.... no great way to handle that....  TV Show				
				if (strncmp(atomPayload, "TV Show", 6) == 0) {
				  parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x0A");
				} else if (strncmp(atomPayload, "Whacked Bookmark", 16) == 0) {
					parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x05");
				} else if (strncmp(atomPayload, "Music Video", 11) == 0) {
					parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x06");
				} else if (strncmp(atomPayload, "Movie", 11) == 0) {
					parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x01"); //it could be anything, but some nums make iTunes slower it seems
				}
				parsedAtoms[desiredAtom.AtomicNumber].AtomicDataClass = dataType;
				parsedAtoms[desiredAtom.AtomicNumber].AtomicLength = 12 + 4 +1;  //offset + name + class + 4bytes null + \01
				//APar_AtomicWriteTest(desiredAtom.AtomicNumber, true); //only the first byte will be valid
				
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "rtng", 4) == 0 ) {
				//'rtng'(advisory rating) is 5 bytes of data after the data class.... no great way to handle that either.
				if (strncmp(atomPayload, "clean", 5) == 0) {
					parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x02"); //only \02 is clean
				} else if (strncmp(atomPayload, "explicit", 8) == 0) {
					parsedAtoms[desiredAtom.AtomicNumber].AtomicData = strdup("\x04"); //most non \00, \02 numbers are allowed
				}
				parsedAtoms[desiredAtom.AtomicNumber].AtomicDataClass = dataType;
				parsedAtoms[desiredAtom.AtomicNumber].AtomicLength = 12 + 4 +1;  //offset + name + class + 4bytes null + \01
			
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "tmpo", 4) == 0 ) {
				//this sets a 6byte data value (the whole atom length is 18; 4bytes offset, 4bytes name, 4bytes datatype, 6 bytes data) hex 0x12
				short bpm_value = 0;
				sscanf(atomPayload, "%hd", &bpm_value); //sscanf into a short int
				short bpm_data[4] = {0, 0, 0, bpm_value}; // number of elements + 3 shorts used in atom
				bpm_data[0] = (short)sizeof(bpm_data)/sizeof(short);
				APar_EncapulateData(desiredAtom, NULL, bpm_data, dataType);
			
			} else if ( (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "tvsn", 4) == 0) ||
			            (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom)].AtomicName, "tves", 4) == 0) ) {
				//this sets a 8byte data value (the whole atom length is 20; 4bytes offset, 4bytes name, 4bytes datatype, 6 bytes data) hex 0x14
				short data_value = 0;
				sscanf(atomPayload, "%hd", &data_value); //sscanf into a short int
				short given_data[5] = {0, 0, 0, 0, data_value}; // number of elements (doesn't go into atom value) + 4 shorts used in atom (does go into atom)
				given_data[0] = (short)sizeof(given_data)/sizeof(short);
				APar_EncapulateData(desiredAtom, NULL, given_data, dataType);
				
			}
			
		}
	}
	
	return;
}

void APar_AddGenreInfo(const char* m4aFile, const char* atomPayload) {
	//the 1 slight difference between AtomicParsley's custom genre strings on "gnre.data" is that here they are NULL (\00) terminated.
	//in iTunes, there is no null termination; AtomicParsley's atom TextDataClass is all NULL terminated, so that explains it.
	//iTunes doesn't have a problem with it however, so I'll just keep it like so until it breaks.
	const char* standard_genre_atom = "moov.udta.meta.ilst.gnre";
	const char* std_genre_data_atom = "moov.udta.meta.ilst.gnre.data";
	const char* custom_genre_atom = "moov.udta.meta.ilst.©gen";
	const char* cstm_genre_data_atom = "moov.udta.meta.ilst.©gen.data";
	modified_atoms = true;
	
	short genre_number = StringGenreToInt(atomPayload);
	AtomicInfo genreAtom;
	
	if (genre_number != 0) {
		//first find if a custom genre atom ("©gen") exists; erase the custom-string genre atom in favor of the standard genre atom
		//
		//because a sizeof(array) can't be done in a function (a pointer to the array is passed), the first element will be set to the number of elements, and won't be used as part of atom data.
		short genre_data[4] = {0, 0, 0, genre_number}; // number of elements + 3 shorts used in atom
		genre_data[0] = (short)sizeof(genre_data)/sizeof(short);
		
		AtomicInfo verboten_genre_atom = APar_FindAtom(custom_genre_atom, false, false, true);
		
		if (strlen(verboten_genre_atom.AtomicName) > 0) {
			if (strncmp(verboten_genre_atom.AtomicName, "©gen", 4) == 0) {
				APar_RemoveAtom(custom_genre_atom, false);
			}		
		}
		
		genreAtom = APar_FindAtom(std_genre_data_atom, true, false, true);
		APar_EncapulateData(genreAtom, atomPayload, genre_data, AtomicDataClass_Integer);

	} else {
		
		AtomicInfo verboten_genre_atom = APar_FindAtom(standard_genre_atom, false, false, true);

		if (verboten_genre_atom.AtomicNumber > 5 && verboten_genre_atom.AtomicNumber < atom_number) {
			if (strncmp(verboten_genre_atom.AtomicName, "gnre", 4) == 0) {
				APar_RemoveAtom(standard_genre_atom, false);
			}		
		}
		genreAtom = APar_FindAtom(cstm_genre_data_atom, true, false, true);
		APar_EncapulateData(genreAtom, atomPayload, 0, AtomicDataClass_Text);
	}
	return;
}

void APar_AddMetadataArtwork(const char* m4aFile, const char* artworkPath, char* env_PicOptions) {
	modified_atoms = true;
	const char* artwork_atom = "moov.udta.meta.ilst.covr";
	if (strncasecmp(artworkPath, "REMOVE_ALL", 10) == 0) {
		APar_RemoveAtom(artwork_atom, false);
	
	} else {
		AtomicInfo desiredAtom = APar_FindAtom(artwork_atom, true, false, true);
		desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
	
		//determine if any picture preferences will impact the picture file in any way
		myPicturePrefs = ExtractPicPrefs(env_PicOptions);

#if defined(__ppc__)
		char* resized_filepath = ResizeGivenImage(artworkPath , myPicturePrefs);
		if ( strncmp(resized_filepath, "/", 1) == 0 ) {
			APar_EncapulateData(desiredAtom, resized_filepath, 0, APar_TestArtworkBinaryData(resized_filepath) );
			parsedAtoms[desiredAtom.AtomicNumber].tempFile = true; //THIS desiredAtom holds the temp pic file path
		
			if (myPicturePrefs.addBOTHpix) {
				//create another sparse atom to hold the new file path (otherwise the 2nd will just overwrite the 1st in EncapsulateData
				desiredAtom = APar_FindAtom(artwork_atom, true, false, true);
				desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
				APar_EncapulateData(desiredAtom, artworkPath, 0, APar_TestArtworkBinaryData(artworkPath) );
			}
		} else {
			APar_EncapulateData(desiredAtom, artworkPath, 0, APar_TestArtworkBinaryData(artworkPath) );
		}
#else
		//perhaps some libjpeg based resizing/modification for non-Mac OS X based platforms
		APar_EncapulateData(desiredAtom, artworkPath, 0, APar_TestArtworkBinaryData(artworkPath) );
#endif
	}
	return;
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
long APar_DetermineMediaData_AtomPosition() {
	long mdat_position = 0;
	short thisAtomNumber = 0;
	
	//loop through each atom in the struct array (which holds the offset info/data)
 	while (parsedAtoms[thisAtomNumber].NextAtomNumber != 0) {
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber];
		//fprintf(stdout, "our atom is %s\n", thisAtom.AtomicName);
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLevel == 1) && (thisAtom.AtomicLength > 16) ) {
			break;
		}
		if (thisAtom.AtomicLevel == 1) {
			mdat_position +=thisAtom.AtomicLength;
		}
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}	
	return mdat_position - mdat_start;
}

void APar_Readjust_STCO_atom(long supplemental_offset, short stco_number) {
	AtomicInfo thisAtom = parsedAtoms[stco_number];
	//fprintf(stdout, "Just checking stco = %s\n", thisAtom.AtomicName);
	APar_AtomicRead(stco_number);
	parsedAtoms[stco_number].AtomicDataClass = AtomicDataClass_Integer;
	//readjust
	
	char* stco_entries = (char *)malloc(sizeof(char)*4);
	memcpy(stco_entries, parsedAtoms[stco_number].AtomicData, 4);
	long entries = longFromBigEndian(stco_entries);
	
	char* an_entry = (char *)malloc(sizeof(char)*4);
	
	for(long i=1; i<=entries; i++) {
		//read 4 bytes of the atom into a 4 char long an_entry to eval it
		for (int c = 0; c <=3; c++ ) {
			//first stco entry is the number of entries; every other one is an actual offset value
			an_entry[c] = parsedAtoms[stco_number].AtomicData[i*4 + c];
		}
		long this_entry = longFromBigEndian(an_entry);
		this_entry += supplemental_offset; //this is where we add our new mdat offset difference
		char4long(this_entry, an_entry);
		//and put the data back into AtomicData...
		for (int c = 0; c <=3; c++ ) {
			//first stco entry is the number of entries; every other one is an actual offset value
			parsedAtoms[stco_number].AtomicData[i*4 + c] = an_entry[c];
		}
	}
	
	free(an_entry);
	free(stco_entries);
	//end readjustment
	//APar_AtomicWriteTest(parsedAtoms[thisAtomNumber].AtomicNumber, true);
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Determine Atom Length                                    //
///////////////////////////////////////////////////////////////////////////////////////

void APar_DetermineNewFileLength() {
	short thisAtomNumber = 0;
	while (true) {		
		if (parsedAtoms[thisAtomNumber].AtomicLevel == 1) {				
	    new_file_size += parsedAtoms[thisAtomNumber].AtomicLength; //used in progressbar
		}
		if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) {
			break;
		}
		thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
	}
	return;
}

void APar_DetermineAtomLengths() {

	APar_Verify__udta_meta_hdlr__atom();
	//APar_PrintAtomicTree();

	APar_Move_mdat_Atoms();
	
	if (Create__udta_meta_hdlr__atom) { //this boolean gets set in APar_Verify__udta_meta_hdlr__atom; the hdlr atom (with data) is required by iTunes to enable tagging
		
		//if Quicktime (Player at the least) is used to create any type of mp4 file, the entire udta hierarchy is missing
		//if iTunes doesn't find this "moov.udta.meta.hdlr" atom (and its data), it refuses to let any information be changed
		//the dreaded "Album Artwork Not Modifiable" shows up. It's because this atom is missing. Oddly, QT Player can see the info
		//this only works for mp4/m4a files - it doesn't work for 3gp (it writes perfectly fine, iTunes plays it (not modifiable)
		
		AtomicInfo hdlr_atom = APar_FindAtom("moov.udta.meta.hdlr", true, false, false);
		hdlr_atom = APar_CreateSparseAtom("moov.udta.meta", "hdlr", NULL, 4, false);
		APar_EncapulateData(hdlr_atom, NULL, NULL, AtomicDataClass_Integer);
	}
	
	short last_atom = APar_FindLastAtom();
	//fprintf(stdout, "Last atom is named %s, num:%i\n", parsedAtoms[last_atom].AtomicName, parsedAtoms[last_atom].AtomicNumber);
	short rev_atom_loop = APar_FindPrecedingAtom(parsedAtoms[last_atom]);
	
	while (rev_atom_loop !=0) {
		short next_atom = 0;
		long atom_size = 0;
		//fprintf(stdout, "preceding atom is named %s, num:%i\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[rev_atom_loop].AtomicNumber);
		
		//if we were to eval our last atom, we would do it here, but it's either "free", "mdat", or a metadata "data" child atom
		//which means, that it's either a top level atom, or a child (and not a container atom), so length has already been determined
		//had it not (or we were on a mission to cover every base), we would eval the last atom before APar_FindPrecedingAtom.
		next_atom = rev_atom_loop;
		rev_atom_loop = APar_FindPrecedingAtom(parsedAtoms[rev_atom_loop]);
		//fprintf(stdout, "current atom is named %s, num:%i\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[rev_atom_loop].AtomicNumber);
		short previous_atom = APar_FindPrecedingAtomNumber(rev_atom_loop);

		if (parsedAtoms[rev_atom_loop].AtomicLevel == ( parsedAtoms[next_atom].AtomicLevel - 1) ) {
			//apparently, a newly created atom of some sort.... we'll need to discern if what kind of parent/container atom 
			if ( strncmp(parsedAtoms[rev_atom_loop].AtomicName, "meta", 4) == 0 ) {
				atom_size += 12;
				
			} else if ( strncmp(parsedAtoms[rev_atom_loop].AtomicName, "stsd", 4) == 0 ) {
				atom_size += 16;

			//video drm
			} else if (strncmp(parsedAtoms[previous_atom].AtomicName, "stsd", 4) == 0) {
				if (strncmp(parsedAtoms[rev_atom_loop].AtomicName, "drmi", 4) == 0) {
					atom_size += 86;
				
				//chapter atoms from Apple's chapter tool make 3 trkn hierarchies with these types; if any new ones show up, they default to the next "else": +36
				} else if ( (strncmp(parsedAtoms[rev_atom_loop].AtomicName, "text", 4) == 0) || 
										(strncmp(parsedAtoms[rev_atom_loop].AtomicName, "jpeg", 4) == 0) ||
										(strncmp(parsedAtoms[rev_atom_loop].AtomicName, "tx3g", 4) == 0) ) {
					atom_size += 16;
					
				} else {
					//all the other nonstandard atoms directly after stsd: "drms", "mp4a", "alac" (and any new ones that may come up will default to +36 - watch for it)
					atom_size += 36;
				}
			
			} else {
				atom_size += 8;
			}
		}
		
		while (parsedAtoms[next_atom].AtomicLevel > parsedAtoms[rev_atom_loop].AtomicLevel) { // eval all child atoms....
			//fprintf(stdout, "\ttest child atom %s, level:%i (sum %li)\n", parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size);
			if (parsedAtoms[rev_atom_loop].AtomicLevel == ( parsedAtoms[next_atom].AtomicLevel - 1) ) { // only child atoms 1 level down
				atom_size += parsedAtoms[next_atom].AtomicLength;
				//fprintf(stdout, "\t\teval child atom %s, level:%i (sum %li)\n", parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size); 
				//fprintf(stdout, "\t\teval %s's child atom %s, level:%i (sum %li, added %li)\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[next_atom].AtomicName, parsedAtoms[next_atom].AtomicLevel, atom_size, parsedAtoms[next_atom].AtomicLength);
			}
			next_atom = parsedAtoms[next_atom].NextAtomNumber; //increment to eval next atom
			parsedAtoms[rev_atom_loop].AtomicLength = atom_size;
		}
		
	}
	APar_DetermineNewFileLength();
	//APar_SimpleAtomPrintout();
	//APar_PrintAtomicTree();
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                          Atom Writing Functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_ShellProgressBar(long bytes_written) {
	strcpy(file_progress_buffer, "Progress: ");
	
	if (bytes_written == 0) {
		for (int i = 0; i <= max_display_width; i++) {
			strcat(file_progress_buffer, "=");
			if (i == max_display_width) {
				sprintf(file_progress_buffer, "%s>%d%%", file_progress_buffer, 100);
			}
		}
	} else {
	
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
	}
	strcat(file_progress_buffer, "|");
	
	//char* file_progress=(char*)malloc( sizeof(char)* (strlen(file_progress_buffer) -1) );
	//strncpy(file_progress, file_progress_buffer, strlen(file_progress_buffer) -1);
	
	fprintf(stdout, "%s\r", file_progress_buffer);
	fflush(stdout);
	return;
}

void APar_DeriveNewPath(const char *filePath, char* &temp_path) {
	char* suffix = strrchr(filePath, '.');
	
	size_t filepath_len = strlen(filePath);
	size_t base_len = filepath_len-strlen(suffix);
	strncpy(temp_path, filePath, base_len);
	
	char* appendage = "-temp-";
	for (size_t i=0; i <= strlen(appendage); i++) {
		temp_path[base_len+i] = appendage[i];
	}
	
	char randstring[6];
	srand((int) time(NULL)); //Seeds rand()
	int randNum = rand()%100000;
	sprintf(randstring, "%i", randNum);
	strcat(temp_path, randstring);
	
	strcat(temp_path, suffix);
	return;
}

void APar_FileWrite_Buffered(FILE* dest_file, FILE *src_file, long dest_start, long src_start, long length, char* &buffer) {
	//fprintf(stdout, "I'm at %li\n", src_start);
	fseek(src_file, src_start, SEEK_SET);
	fread(buffer, 1, (size_t)length, src_file);

	fseek(dest_file, dest_start, SEEK_SET);
	fwrite(buffer, (size_t)length, 1, dest_file);
	return;
}

void APar_CompleteCopyFile(FILE* dest_file, FILE *src_file, long new_file_size, char* &buffer) {
	//this function is used to duplicate the temp file back into the original file.
	//hopefully, this reduces any memory strain due to modding an Apple Lossless m4a file (toting in at a pork choppy 50Megs)
	//allocating 50MB would be.... quite the allocation, so we'll show restraint: 102400 bytes (goes through the while loop 512 times)
	long file_pos = 0;
	while (file_pos <= new_file_size) {
		if (file_pos + max_buffer <= new_file_size ) {
			fseek(src_file, file_pos, SEEK_SET);
			fread(buffer, 1, (size_t)max_buffer, src_file);
			
			fseek(dest_file, file_pos, SEEK_SET);
			fwrite(buffer, (size_t)max_buffer, 1, dest_file);
			file_pos += max_buffer;
			
		} else {
			fseek(src_file, file_pos, SEEK_SET);
			fread(buffer, 1, (size_t)(new_file_size - file_pos), src_file);
			
			fseek(dest_file, file_pos, SEEK_SET);
			fwrite(buffer, (size_t)(new_file_size - file_pos), 1, dest_file);
			file_pos += new_file_size - file_pos;
			break;
		}		
	}
	return;
}

long APar_DRMS_WriteAtomically(FILE* temp_file, char* &buffer, char* &conv_buffer, long bytes_written_tally, short this_number) {
	long bytes_written = 0;

	char4long(parsedAtoms[this_number].AtomicLength, conv_buffer); //write the atom length
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

long APar_WriteAtomically(FILE* source_file, FILE* temp_file, bool from_file, char* &buffer, char* &conv_buffer, long bytes_written_tally, short this_atom) {
	long bytes_written = 0;
	
	//fprintf(stdout, "Writing atom \"%s\" at position %li\n", parsedAtoms[this_atom].AtomicName, bytes_written_tally);
	
	//write the length of the atom first... taken from our tree in memory
	char4long(parsedAtoms[this_atom].AtomicLength, conv_buffer);
	fseek(temp_file, bytes_written_tally, SEEK_SET);
	fwrite(conv_buffer, 4, 1, temp_file);
	bytes_written += 4;
	
	//handle jpeg/pngs differently when we are ADDING them: they will be coming from entirely separate files
	//jpegs/png already present in the file get handled in the "if (from_file)" portion
	/*if (parsedAtoms[this_atom].AtomicData != NULL) {
		AtomicInfo parentAtom = APar_FindParentAtom(parsedAtoms[this_atom].AtomicNumber, parsedAtoms[this_atom].AtomicLevel);
		fprintf(stdout, "Parent atom %s has child with data %s\n", parentAtom.AtomicName, parsedAtoms[this_atom].AtomicData);
	}*/
	if ( ((parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_JPEGBinary) || 
				(parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_PNGBinary)) && (parsedAtoms[this_atom].AtomicData != NULL) ) {
			
		//write the atom name
		fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
		fwrite(parsedAtoms[this_atom].AtomicName, 4, 1, temp_file);
		bytes_written += 4;
		
		//write the atom data class
		char4long( (long)parsedAtoms[this_atom].AtomicDataClass, conv_buffer);
		fwrite(conv_buffer, 4, 1, temp_file);
		bytes_written += 4;
		
		//write a 4 bytes of null space out
		char4long( 0, conv_buffer);
		fwrite(conv_buffer, 4, 1, temp_file);
		bytes_written += 4;
		
		//open the originating file...
		FILE *pic_file = NULL;
		pic_file = fopen(parsedAtoms[this_atom].AtomicData, "r");
		if (pic_file != NULL) {
			//...and the actual transfer of the picture
			while (bytes_written <= parsedAtoms[this_atom].AtomicLength) {
				if (bytes_written + max_buffer <= (long)parsedAtoms[this_atom].AtomicLength ) {
					//fprintf(stdout, "Writing atom %s from file looping into buffer\n", parsedAtoms[this_atom].AtomicName);
					fseek(pic_file, bytes_written - 16, SEEK_SET);
					fread(buffer, 1, (size_t)max_buffer, pic_file);
				
					fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
					fwrite(buffer, (size_t)max_buffer, 1, temp_file);
					bytes_written += max_buffer;
					
					APar_ShellProgressBar(bytes_written);
				
				} else { //we either came up on a short atom (most are), or the last bit of a really long atom
					//fprintf(stdout, "Writing atom %s from file directly into buffer\n", parsedAtoms[this_atom].AtomicName);
					fseek(pic_file, bytes_written - 16, SEEK_SET);
					fread(buffer, 1, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), pic_file);
				
					fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
					fwrite(buffer, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), 1, temp_file);
					bytes_written += parsedAtoms[this_atom].AtomicLength - bytes_written;
					
					APar_ShellProgressBar(bytes_written);
				
					break;
				} //endif
			}//end while
			
		}//ends if(pic_file)
		fclose(pic_file);
		
		if (myPicturePrefs.removeTempPix && parsedAtoms[this_atom].tempFile ) {
			//reopen the picture file to delete if this IS a temp file (and the env pref was given)
			pic_file = fopen(parsedAtoms[this_atom].AtomicData, "w");
			remove(parsedAtoms[this_atom].AtomicData);
			fclose(pic_file);
		}		
				
	} else if (from_file) {
		// here we read in the original atom into the buffer. If the length is greater than our buffer length,
		// we loop, reading in chunks of the atom's data into the buffer, and immediately write it out, reusing the buffer.
		
		while (bytes_written <= parsedAtoms[this_atom].AtomicLength) {
			if (bytes_written + max_buffer <= (long)parsedAtoms[this_atom].AtomicLength ) {
				//fprintf(stdout, "Writing atom %s from file looping into buffer\n", parsedAtoms[this_atom].AtomicName);
				//read&write occurs from & including atom name through end of atom
				fseek(source_file, (bytes_written + parsedAtoms[this_atom].AtomicStart), SEEK_SET);
				fread(buffer, 1, (size_t)max_buffer, source_file);
				
				fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
				fwrite(buffer, (size_t)max_buffer, 1, temp_file);
				bytes_written += max_buffer;
				
				APar_ShellProgressBar(bytes_written);
				
			} else { //we either came up on a short atom (most are), or the last bit of a really long atom
				//fprintf(stdout, "Writing atom %s from file directly into buffer\n", parsedAtoms[this_atom].AtomicName);
				fseek(source_file, (bytes_written + parsedAtoms[this_atom].AtomicStart), SEEK_SET);
				fread(buffer, 1, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), source_file);
				
				fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
				fwrite(buffer, (size_t)(parsedAtoms[this_atom].AtomicLength - bytes_written), 1, temp_file);
				bytes_written += parsedAtoms[this_atom].AtomicLength - bytes_written;
				
				APar_ShellProgressBar(bytes_written);
				
				break;
			}
		}
		
	} else { // we are going to be writing not from the file, but directly from the tree (in memory).

		//fprintf(stdout, "Writing atom %s from memory\n", parsedAtoms[this_atom].AtomicName);
		fseek(temp_file, (bytes_written_tally + bytes_written), SEEK_SET);
		fwrite(parsedAtoms[this_atom].AtomicName, 4, 1, temp_file);
		bytes_written += 4;
		if (parsedAtoms[this_atom].AtomicDataClass >= 0) {
			char4long( (long)parsedAtoms[this_atom].AtomicDataClass, conv_buffer);
			fwrite(conv_buffer, 4, 1, temp_file);
			bytes_written += 4;
			
			if (parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_Text) {
				char4long( 0, conv_buffer);
				fwrite(conv_buffer, 4, 1, temp_file);
				bytes_written += 4;
			}
		}
		//parent_atom is only used for cpil/rtng
		AtomicInfo parent_atom = APar_FindParentAtom(parsedAtoms[this_atom].AtomicNumber, parsedAtoms[this_atom].AtomicLevel);
		
		if (parsedAtoms[this_atom].AtomicData != NULL) {
			long atom_data_size = 0;
			//fwrite(conv_buffer, 4, 1, temp_file);
			if (strncmp(parsedAtoms[this_atom].AtomicName, "stsd", 4) == 0) {
					atom_data_size = 4;
					
			} else if ( (strncmp(parent_atom.AtomicName, "cpil", 4) == 0) || 
									(strncmp(parent_atom.AtomicName, "rtng", 4) == 0) ||
									(strncmp(parent_atom.AtomicName, "stik", 4) == 0) || 
									(strncmp(parent_atom.AtomicName, "pcst", 4) == 0) ) {
				//cpil/rtng is difficult to handle: its 5 bytes; AtomicParsley works in 1byte chunks for text/art; 2 bytes for others
				char4long( 0, conv_buffer);
				fwrite(conv_buffer, 4, 1, temp_file);
				bytes_written += 4;
				
				fwrite(parsedAtoms[this_atom].AtomicData, 1, 1, temp_file);
				bytes_written += 1;	
			
			} else {
				if (parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_Text) {
					atom_data_size = parsedAtoms[this_atom].AtomicLength - 16;
				} else {
					atom_data_size = parsedAtoms[this_atom].AtomicLength - 12;
				}
			}
			//can't strlen on data that has nulls (and most non-udta atoms like the important stco have nulls)
			//fwrite(parsedAtoms[this_atom].AtomicData, strlen(parsedAtoms[this_atom].AtomicData)+1, 1, temp_file);
			//bytes_written += (long)(strlen(parsedAtoms[this_atom].AtomicData)+1);
			fwrite(parsedAtoms[this_atom].AtomicData, atom_data_size, 1, temp_file);
			bytes_written += atom_data_size;
			
			APar_ShellProgressBar(bytes_written);
		}
	}
	
	return bytes_written;
}

void APar_WriteFile(const char* m4aFile, bool rewrite_original) {
	char* file_buffer=(char*)malloc( sizeof(char)* max_buffer );
	char* temp_file_name=(char*)malloc( sizeof(char)* (strlen(m4aFile) +12) );
	char* data = (char*)malloc(sizeof(char)*4);
	FILE* temp_file;
	long temp_file_bytes_written = 0;
	short thisAtomNumber = 0;
	
	long mdat_offset = APar_DetermineMediaData_AtomPosition();

	APar_DeriveNewPath(m4aFile, temp_file_name);
	temp_file = fopen(temp_file_name, "wr");
	if (temp_file != NULL) {
		//body of atom writing here
		
		fprintf(stdout, "\nStarted writing to temp file.\n");
		
		while (true) {
			AtomicInfo thisAtom = parsedAtoms[thisAtomNumber];
			//the loop where the critical determination is made
			if (strncmp(parsedAtoms[thisAtomNumber].AtomicName, "stco", 4) == 0) {
					if (mdat_offset != 0 ) {
						//stco atom will need to be readjusted
						APar_Readjust_STCO_atom(mdat_offset, thisAtomNumber);
						temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, false, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
					} else {
						temp_file_bytes_written += APar_WriteAtomically(source_file, temp_file, true, file_buffer, data, temp_file_bytes_written, thisAtomNumber);
					}

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
			thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
		}
		APar_ShellProgressBar(0);
		fprintf(stdout, "\nFinished writing to temp file.\n");
		fclose(temp_file);
		
	} else {
		fprintf(stdout, "An error occurred while trying to create a temp file.\n");
		exit(1);
	}
	
	if (rewrite_original) {
		fclose(source_file);
		FILE *originating_file = NULL;
		originating_file = fopen(m4aFile, "wr");
		temp_file = NULL;
		temp_file = fopen(temp_file_name, "r"); //reopens as read-only
		if (originating_file != NULL) {
			APar_CompleteCopyFile(originating_file, temp_file, temp_file_bytes_written, file_buffer);
			fclose(temp_file);
			remove(temp_file_name); //this deletes the temp file so we don't have to suffer seeing it anymore. (I could pre-pend a '.' to it....)
			fclose(originating_file);
		} else {
			fprintf(stdout, "AtomicParsley error: unable to open original file for writing; writeback failed - file is unaltered.\n");
		}
	}
	//APar_PrintAtomicTree();
	APar_FreeMemory();
	free(temp_file_name);
	file_buffer = NULL;
	free(file_buffer);
	free(data);
	
	return;
}
