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

long max_buffer = 4096*125; // increased to 512KB

long mdat_start=0;
long largest_mdat=0;
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

char* extractAtomName(char *fileData, int name_position) {
//name_position = 1 for normal atoms and needs to be done first; 2 for uuid atoms (which can only occur after we first find the atomName == "uuid")
	char *atom=(char *)malloc(sizeof(char)*5);

	for (int i=0; i < 4; i++) {
		atom[i] = fileData[i + (name_position * 4) ]; //we want the 4 byte 'atom' in data [4,5,6,7] (or the uuid atom name in 8,9,10,11)
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

AtomicInfo APar_FindAtom(const char* atom_name, bool createMissing, bool uuid_atom_type, bool findChild, bool directFind) {
	short present_atom_level = 1; //from where our generalAtomicLevel starts
	char* atom_hierarchy = strdup(atom_name);
	char* found_hierarchy = (char *)malloc(sizeof(char)*1000); //that should hold it
	for (int i=0; i<= 1000; i++) {
		found_hierarchy[i] = '\00';
	}
	bool is_uuid_atom = false;
	char* uuid_name = (char *)malloc(sizeof(char)*4);
	AtomicInfo thisAtom = { 0 };
	char* parent_name = (char *)malloc(sizeof(char)*4);
	
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
				break;
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
	free(uuid_name);

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
	
	parent_atom = APar_FindAtom(found_hierarchy, false, false, true, false);
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
	free(found_hierarchy);
	free(atom_hierarchy);
	
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
	char* uuid_name = (char *)malloc(sizeof(char)*4);
	
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

void APar_ExtractAAC_Artwork(short this_atom_num, char* pic_output_path, short artwork_count) {
	char *base_outpath=(char *)malloc(sizeof(char)*MAXPATHLEN);
	strcpy(base_outpath, pic_output_path);
	strcat(base_outpath, "_artwork");
	sprintf(base_outpath, "%s_%d", base_outpath, artwork_count);
	
	char* art_payload = (char*)malloc( sizeof(char) * (parsedAtoms[this_atom_num].AtomicLength-16) );
			
	fseek(source_file, parsedAtoms[this_atom_num].AtomicStart+16, SEEK_SET);
	fread(art_payload, 1, parsedAtoms[this_atom_num].AtomicLength-16, source_file);
	
	char* suffix = (char *)malloc(sizeof(char)*5);
	
	if (strncmp((char *)art_payload, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {//casts uchar* to char* (2)
				suffix = ".png";
	}	else if (strncmp((char *)art_payload, "\xFF\xD8\xFF\xE0", 4) == 0) {//casts uchar* to char* (2)
				suffix = ".jpg";
	}
	
	strcat(base_outpath, suffix);
	
	FILE *outfile = fopen(base_outpath, "wb");
	if (outfile != NULL) {
		fwrite(art_payload, (size_t)(parsedAtoms[this_atom_num].AtomicLength-16), 1, outfile);
		fclose(outfile);
		fprintf(stdout, "Extracted artwork to file: %s\n", base_outpath);
	}
	free(base_outpath);
	free(art_payload);
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

		int min_atom_datasize = 12;
		int atom_header_size = 16;
		
		if (thisAtom.uuidAtomType) {
			min_atom_datasize += 4;
			atom_header_size += 4;
		}
 
		if (thisAtom.AtomicLength > min_atom_datasize ) {
			char* data_payload = (char*)malloc( sizeof(char) * (thisAtom.AtomicLength - atom_header_size) );
			for (int i=0; i<= thisAtom.AtomicLength - atom_header_size; i++) {
				data_payload[i] = '\00';
			}
			
			fseek(source_file, thisAtom.AtomicStart + atom_header_size, SEEK_SET);
			fread(data_payload, 1, thisAtom.AtomicLength - atom_header_size, source_file);

			if (thisAtom.AtomicDataClass == AtomicDataClass_Text) {
				if (thisAtom.AtomicLength < (atom_header_size + 4) ) {
					//tvnn was showing up with 4 chars instead of 3; easier to null it out for now
					data_payload[thisAtom.AtomicLength - atom_header_size] = '\00';
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
					if ( thisAtom.AtomicLength - atom_header_size < 3 ) { //oh, a 1byte int for genre number
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
						fprintf(stdout, "Unknown value: %li\n", primary_number);
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
	char* atom_name = (char*)malloc(sizeof(char)*4);
	char* parent_atom = (char*)malloc(sizeof(char)*4);

	for (int i=0; i < atom_number; i++) { 
		AtomicInfo thisAtom = parsedAtoms[i];
		for (int j=0; j<= 4; j++) {
			atom_name[j] = '\00';
		}
		strncpy(atom_name, thisAtom.AtomicName, 4);
		if ( strncmp(atom_name, "data", 4) == 0 ) {
			for (int j=0; j<= 4; j++) {
				parent_atom[j] = '\00';
			}
			
			AtomicInfo parent = parsedAtoms[ APar_FindParentAtom(i, thisAtom.AtomicLevel) ];
			strncpy(parent_atom, parent.AtomicName, 4);
			
#if defined (USE_ICONV_CONVERSION)
			StringReEncode(parent_atom, "UTF-8", "ISO-8859-1");
#endif
			
			if ( (thisAtom.AtomicDataClass == AtomicDataClass_UInteger ||
            thisAtom.AtomicDataClass == AtomicDataClass_Text || 
            thisAtom.AtomicDataClass == AtomicDataClass_UInt8_Binary) && !extract_pix ) {
				fprintf(stdout, "Atom \"%s\" contains: ", parent_atom);
				APar_ExtractDataAtom(i);
			} else if (strncmp(parent_atom,"covr", 4) == 0) {
				artwork_count++;
				if (extract_pix) {
					APar_ExtractAAC_Artwork(thisAtom.AtomicNumber, pic_output_path, artwork_count);
				}
			} else {
				//fprintf(stdout, "parent atom %s had data atom (of class:) %i\n ", parent_atom, thisAtom.AtomicDataClass);
			}
		} else if (thisAtom.uuidAtomType) {
#if defined (USE_ICONV_CONVERSION)
			StringReEncode(atom_name, "UTF-8", "ISO-8859-1");
#endif

			if (thisAtom.AtomicDataClass == AtomicDataClass_Text) {
				fprintf(stdout, "Atom uuid=\"%s\" contains: ", atom_name);
				APar_ExtractDataAtom(i);
			}
		}
	}
	free(atom_name);
	free(parent_atom);
	
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

void APar_AtomizeFileInfo(AtomicInfo &thisAtom, long Astart, long Alength, char* Astring, short Alevel, int Aclass, int NextAtomNum, bool uuid_type) {
	thisAtom.AtomicStart = Astart;
	thisAtom.AtomicLength = Alength;
	
	thisAtom.AtomicName = (char*)malloc(sizeof(char)*4);
	strcpy(thisAtom.AtomicName, Astring);
	thisAtom.AtomicNumber = atom_number;
	thisAtom.AtomicLevel = Alevel;
	thisAtom.AtomicDataClass = Aclass;
	thisAtom.uuidAtomType = uuid_type;
	
	//set the next atom number of the PREVIOUS atom (we didn't know there would be one until now); this is our default normal mode
	if (( NextAtomNum == 0 ) && ( atom_number !=0 )) {
		parsedAtoms[atom_number-1].NextAtomNumber = atom_number;
	}
	thisAtom.NextAtomNumber=0; //this could be the end... (we just can't quite say until we find another atom)
	
	//handle our oddballs
	if (strncmp(Astring, "meta", 4) == 0) {
		thisAtom.AtomicDataClass = AtomicDataClass_UInteger;
	}
	
	if ( (strncmp(Astring, "mdat", 4) == 0) && (Alevel == 1) && (Alength > 16) ) {
		if ( Astart >= largest_mdat ) {
			if (Alength >= largest_mdat) {
				mdat_start = Astart;
				largest_mdat = Alength;
			}
		}
	}
	
	//takes care of mdat.length=1
	if ( (strncmp(Astring, "mdat", 4) == 0) && (Alevel == 1) && (Alength == 0) ) {
		long mdat_to_eof = (long)file_size - Astart;
		if ( mdat_to_eof >= largest_mdat) {
			mdat_start = Astart;
			largest_mdat = mdat_to_eof;
		}
	}
	
	atom_number++; //increment to the next AtomicInfo array
	
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
	long mdatData = 0;
	short thisAtomNumber = 0;
	char* atom_name = (char*)malloc(sizeof(char)*4);
	
	//loop through each atom in the struct array (which holds the offset info/data)
 	while (true) { //while (parsedAtoms[thisAtomNumber].NextAtomNumber != 0) { 
		AtomicInfo thisAtom = parsedAtoms[thisAtomNumber];
		
		for (int i=0; i<= 4; i++) {
			atom_name[i] = '\00';
		}
		strncpy(atom_name, thisAtom.AtomicName, 4);
		
		strcpy(tree_padding, "");
		if ( thisAtom.AtomicLevel != 1 ) {
			for (int pad=1; pad < thisAtom.AtomicLevel; pad++) {
				strcat(tree_padding, "    "); // if the atom depth is over 1, then add spaces before text starts to form the tree
			}
			strcat(tree_padding, " "); // add a single space
		}

#if defined (USE_ICONV_CONVERSION)
		StringReEncode(atom_name, "UTF-8", "ISO-8859-1");
#endif
		
		if (thisAtom.AtomicLength == 0) {
			fprintf(stdout, "%sAtom %s @ %li of size: %li (%li*), ends @ %li\n", tree_padding, atom_name, thisAtom.AtomicStart, ( (long)file_size - thisAtom.AtomicStart), thisAtom.AtomicLength, (long)file_size );
			fprintf(stdout, "\t\t\t (*)denotes length of atom goes to End-of-File\n");
		} else if (thisAtom.uuidAtomType) {
			fprintf(stdout, "%sAtom uuid=%s @ %li of size: %li, ends @ %li\n", tree_padding, atom_name, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
		} else {
			fprintf(stdout, "%sAtom %s @ %li of size: %li, ends @ %li\n", tree_padding, atom_name, thisAtom.AtomicStart, thisAtom.AtomicLength, (thisAtom.AtomicStart + thisAtom.AtomicLength) );
		}
		
		//simple tally & percentage of free space info
		if (strncmp(thisAtom.AtomicName, "free", 4) == 0) {
			freeSpace=freeSpace+thisAtom.AtomicLength;
		}
		//this is where the *raw* audio/video file is, the rest if fluff.
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLength > 100) ) {
			mdatData = thisAtom.AtomicLength;
		}
		if (parsedAtoms[thisAtomNumber].NextAtomNumber == 0) {
			break;
		} else {
			thisAtomNumber = parsedAtoms[thisAtomNumber].NextAtomNumber;
		}
	}
		
	fprintf(stdout, "------------------------------------------------------\n");
	fprintf(stdout, "Total size: %i bytes; %i atoms total. AtomicParsley v%s\n", (int)file_size, atom_number-1, AtomicParsley_version);
	fprintf(stdout, "Media data: %li bytes; %li bytes all other atoms (%2.3f%% atom overhead).\n", 
												mdatData, (long)(file_size - mdatData), (float)(file_size - mdatData)/(float)file_size * 100 );
	fprintf(stdout, "Total free atoms: %li bytes; %2.3f%% waste.\n", freeSpace, (float)freeSpace/(float)file_size * 100 );
	fprintf(stdout, "------------------------------------------------------\n");
	
	free(tree_padding);
	tree_padding = NULL;
	free(atom_name);
		
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

short APar_DetermineDataType(char* atom, bool uuid_type) {
	
	char* data_type = (char*)malloc(sizeof(char)*4);
	int data_offset = 8;
	if (uuid_type) {
		data_offset = 12;
	}
	for (int i=0; i < 4; i++) {
		data_type[i] = atom[i+data_offset]; //we want the 1 byte code (01, 15) following the data atom to determine type (text, binary)
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
	bool uuid_atom = false;
	fseek(file, midJump, SEEK_SET);
	
	//this now works anywhere the stsd atom is situated
	while ( stsd_progress < drmLength) {
		fread(data, 1, 12, file);
		char *atom = extractAtomName(data, 1);
		
		if (strncasecmp(atom, "uuid", 4) == 0) {
			atom = extractAtomName(data, 2);
			uuid_atom = true;
		}

		interDataSize = longFromBigEndian(data);
		
		if ( interDataSize > drmLength || interDataSize < 8) {
			break; //we only get here if there is some oddball atom here under stsd that has a dataclass
		}
				
		APar_AtomizeFileInfo(parsedAtoms[atom_number], midJump, interDataSize, atom, atomLevel, -1, 0, uuid_atom);
		
		if (strncmp(atom, "drms", 4) == 0) {
			//this needs to be done in order to maintain integrity of modified files
			parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28);
			fseek (file, midJump+8, SEEK_SET);
			fread(parsedAtoms[atom_number-1].AtomicData, 1, 28, file); //store the entire atom (data class won't even be used; only the length & atom name are created)
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_UInteger;
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
			parsedAtoms[atom_number-1].AtomicDataClass = AtomicDataClass_UInteger;
			atomLevel++;
			flag_drms_atom = true; //although almost assuredly, we already set this to true by finding a "drms" atom before this atom
			
		} else if ( (strncmp(atom, "mp4a", 4) == 0) || ( (strncmp(atom, "alac", 4) == 0) && (atomLevel == 7) ) ) {
				parsedAtoms[atom_number-1].AtomicData = (char *)malloc(sizeof(char)*28);
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
		fseek(file, midJump, SEEK_SET);
	}

	parsedAtoms[stsd_entry_atom_number].AtomicData = (char *)malloc(sizeof(char)*4 );
	fseek(file, stsd_entry_pos+12, SEEK_SET);
	fread(parsedAtoms[stsd_entry_atom_number].AtomicData, 4, 1, file);
	parsedAtoms[stsd_entry_atom_number].AtomicDataClass = AtomicDataClass_UInteger;

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
			char *atom = extractAtomName(data, 1);
			//fprintf(stdout, "atom: %s @ offset: %li ", atom, jump);
			if ( (strncmp(atom, "ftyp", 4) == 0 ) || (strncmp(atom, "jP  ", 4) == 0) ) //jpeg2000 files will be "jP  " (2 spaces)
			{
				dataSize = longFromBigEndian(data);
				jump = dataSize;
				
				APar_AtomizeFileInfo(parsedAtoms[atom_number], 0, jump, atom, generalAtomicLevel, -1, 0, false);
				
				fseek(file, jump, SEEK_SET);
				
				while (jump < (long)file_size) {
					//a jpg2000 file will have a "jp2c" atom of 0000 jump length (don't know the rest).
					fread(data, 1, 12, file);
					char *atom = extractAtomName(data , 1);
					int atom_class = -1;
					bool uuid_atom = false;
					
					if (strncasecmp(atom, "uuid", 4) == 0) {
						atom = extractAtomName(data, 2);
						uuid_atom = true;
					}

					//fprintf(stdout, "atom: %s @ offset: %li \n", atom, jump);
					
					if (strncmp(atom, "data", 4) == 0) {
						atom_class=APar_DetermineDataType(data, false);
						
					} else if (uuid_atom) {						
						//because only 12 bytes per jump are read, the bytes for a uuid's data class (bytes 13-16) aren't normally fetched
						char *uuid_data = (char *) malloc(16);
						fseek(file, jump, SEEK_SET); //this will seek back to the last jump point and...
						fread(uuid_data, 1, 16, file); //read in 16 bytes so we can do a valid extraction and do a nice atom data printout
						
						atom_class=APar_DetermineDataType(uuid_data, uuid_atom);
						if ( (atom_class < 0) || (atom_class > 24) ) {
							atom_class = -1;
						}
						free(uuid_data);
						
					} else {
						atom_class = -1;
					}
					
					dataSize = longFromBigEndian(data);
					
					APar_AtomizeFileInfo(parsedAtoms[atom_number], jump, dataSize, atom, generalAtomicLevel, atom_class, 0, uuid_atom);
					
					if (dataSize == 0) { // length = 0 means it reaches to EOF
						break;
					}
					
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

void APar_EliminateAtom(short this_atom_number, int resume_atom_number) {
	if ( this_atom_number > 0 && this_atom_number < atom_number && resume_atom_number > 0 && resume_atom_number < atom_number) {
		short preceding_atom_pos = APar_FindPrecedingAtom(this_atom_number);

		parsedAtoms[preceding_atom_pos].NextAtomNumber = resume_atom_number;
	}
	return;
}

void APar_RemoveAtom(const char* atom_path, bool uuid_atom_type) {
	modified_atoms = true;
	AtomicInfo desiredAtom = APar_FindAtom(atom_path, false, uuid_atom_type, false, false);
	
	char* atom_hierarchy = strdup(atom_path);
	char *search_atom_name = strsep(&atom_hierarchy,".");
	char *parent_name= strdup(search_atom_name);
	while (atom_hierarchy != NULL) {
		parent_name= strdup(search_atom_name);
		search_atom_name = strsep(&atom_hierarchy,".");
	}
	
  if ( (desiredAtom.AtomicName != NULL) && (search_atom_name != NULL) ) {
    char* uuid_name = (char *)malloc(sizeof(char)*4);
    uuid_name = strstr(search_atom_name, "uuid=");
    if (uuid_name != NULL) {
      search_atom_name = strsep(&uuid_name,"=");
    }

    if ( (strncmp(search_atom_name, desiredAtom.AtomicName, 4) == 0) ||
				 (strncmp(search_atom_name, "data", 4) == 0 && strncmp(parent_name, desiredAtom.AtomicName, 4) == 0) ) { //only remove an atom we have a matching name for	
      short preceding_atom_pos = APar_FindPrecedingAtom(desiredAtom.AtomicNumber);
			AtomicInfo endingAtom;
			if (strncmp(parent_name, "covr", 4) == 0) {
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
				endingAtom = APar_LocateAtomInsertionPoint(atom_path, true);

			}
			APar_EliminateAtom(preceding_atom_pos, endingAtom.NextAtomNumber);

    } else if (desiredAtom.AtomicNumber !=0 && uuid_atom_type) {
			if (strncmp(uuid_name, desiredAtom.AtomicName, 4) == 0) {
				//this will *probably* only work for AtomicParsley created uuid atoms that don't have children
				short preceding_atom_pos = APar_FindPrecedingAtom(desiredAtom.AtomicNumber);
				parsedAtoms[preceding_atom_pos].NextAtomNumber = desiredAtom.NextAtomNumber;
			}
    }
    free(atom_hierarchy);
  }
  return;
}

void APar_freefree() {
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
			short prev_atom = APar_FindPrecedingAtom(eval_atom);
			parsedAtoms[prev_atom].NextAtomNumber = parsedAtoms[eval_atom].NextAtomNumber;
			//fprintf(stdout, "After this %s atom, the %s atom will be removed\n", parsedAtoms[prev_atom].AtomicName, parsedAtoms[eval_atom].AtomicName);
			eval_atom = prev_atom; //go back to the previous atom and continue the search
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

void APar_EncapsulateData(short thisnum, const char* atomData, uint8_t unsignedData[], const int atomDataClass) {
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
			
			if (data_length > 255 && !parsedAtoms[thisnum].uuidAtomType) { //restrict string length on normal non-uuid atoms; direct on-uuid atom text is not restricted.
				data_length = 255;
			}
			
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
			
		case AtomicDataClass_UInteger : //Other than the dataType numbers, I don't think there is a difference in ADC_Int & ADC_Bin - both seem based on unsigned char
			if ( strncmp(parsedAtoms[thisnum].AtomicName, "hdlr",4) == 0 ) {
				
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* 21);
				//apparently, a TODO item is to revisit how AtomicData is stored/input; this is repulsive (but effective)
				memcpy(parsedAtoms[thisnum].AtomicData, "\x00\x00\x00\x00\x6D\x64\x69\x72\x61\x70\x70\x6C\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 22 ); 
				parsedAtoms[thisnum].AtomicLength = 34;
				
			} else {
				//the first member of the unsignedData = # of members (sizeof on arrays become pointers & won't work when passed to a function)
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* ((unsignedData[0]-1) * sizeof(uint8_t))  ); //in other words: malloc unsignedData[0]-1 
				
				for (int i = 0; i < unsignedData[0]-1; i++) {
					parsedAtoms[thisnum].AtomicData[i] = unsignedData[i+1]; //set the data into the struct
				}
				
				parsedAtoms[thisnum].AtomicLength = (unsignedData[0]-1) + 12; //set the length here; +12 for 4bytes(atomLength) + 4bytes(atomName) + 4bytes (dataType)

			}
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_UInteger;
			break;
			
		case AtomicDataClass_UInt8_Binary :
			//two 'types' of data get here, the first are atoms that are set with strings (like purl & egid); the others are numerical
			if ( unsignedData == NULL && strlen(atomData) != 0) { //the strings like purl & egid
				data_length = strlen(atomData);
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* (data_length + 4) ); // 4 bytes null space after atomDataClass
				for (int i = 0; i < 4; i++) {
					parsedAtoms[thisnum].AtomicData[i] = 0; //this gets the 4bytes null...
				}
				for (int i = 0; i <= (int)data_length; i++) {
					parsedAtoms[thisnum].AtomicData[i+4] = atomData[i]; //...and this sets the rest of the data (with a string)
				}
				parsedAtoms[thisnum].AtomicLength = data_length + 12 + 4;
			
			} else {			
				//the first member of the unsignedData = # of members (sizeof on arrays become pointers & won't work when passed to a function)
				parsedAtoms[thisnum].AtomicData = (char*)malloc(sizeof(char)* ((unsignedData[0]-1) * sizeof(uint8_t))  ); // for tves/tvsn & tmpo
				
				for (int i = 0; i < unsignedData[0]-1; i++) {
					parsedAtoms[thisnum].AtomicData[i] = unsignedData[i+1];
				}
				
				parsedAtoms[thisnum].AtomicLength = (unsignedData[0]-1) + 12;
	
			}
			parsedAtoms[thisnum].AtomicDataClass = AtomicDataClass_UInt8_Binary;
			break;
	}

	if (parsedAtoms[thisnum].uuidAtomType) {
		parsedAtoms[thisnum].AtomicLength += 4; //because a uuid atom is [4bytes length][4bytes atom name of 'uuid'][4bytes uuid atom name]
	}

	return;
}

AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name, 
                                 char* remaining_hierarchy, short atom_level, bool asLastChild) {
	//the end boolean value below tells the function to locate where that atom (and its children) end
	AtomicInfo KeyInsertionAtom = APar_LocateAtomInsertionPoint(present_hierarchy, asLastChild);
	bool atom_shunted = false; //only shunt the NextAtomNumber once (for the first atom that is missing.
	int continuation_atom_number = 0;
	char* uuid_name = (char *)malloc(sizeof(char)*4);
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

void APar_Verify__udta_meta_hdlr__atom() { //only test if the atom is present for now, it will be created just before writeout time - to insure it only happens once.
	const char* udta_meta_hdlr__atom = "moov.udta.meta.hdlr";
	AtomicInfo hdlrAtom = APar_FindAtom(udta_meta_hdlr__atom, false, false, false, true);
	if ( hdlrAtom.AtomicNumber > 0 && hdlrAtom.AtomicNumber < atom_number ) {
		if ( strncmp(hdlrAtom.AtomicName, "hdlr", 4) != 0 ) {
			Create__udta_meta_hdlr__atom = true;
		}
	} else {
		Create__udta_meta_hdlr__atom = true; //we got a null value - which means there wan't a moov.udta.meta.hdlr atom
	}
	return;
}

void APar_AddMetadataInfo(const char* m4aFile, const char* atom_path, const int dataType, const char* atomPayload) {
	modified_atoms = true;
	bool retain_atom = true;
	
	if ( strlen(atomPayload) == 0) {
		retain_atom = false;
	}
	
	AtomicInfo desiredAtom = APar_FindAtom(atom_path, true, false, retain_atom, false); //finds the atom; if not present, creates the atom
		
	AtomicInfo parent_atom = parsedAtoms[ APar_FindParentAtom(desiredAtom.AtomicNumber, desiredAtom.AtomicLevel) ];

	if (! retain_atom) {
		APar_EliminateAtom(parent_atom.AtomicNumber, desiredAtom.NextAtomNumber);
		
	} else {
		if (dataType == AtomicDataClass_Text) {
			size_t total_bytes = strlen(atomPayload);
			if (total_bytes > 255) {
				fprintf(stdout, "AtomicParsley warning: %s was trimmed to 255 bytes (%u bytes over)\n", atom_path, (unsigned int)total_bytes-255);
			}
			
			APar_EncapsulateData(desiredAtom.AtomicNumber, atomPayload, NULL, dataType);
			
		} else if (dataType == AtomicDataClass_UInteger) {
			//determine what kinds of numbers we have before we go onto working at the atom level
			uint8_t pos_in_total = 0;
			uint8_t the_total = 0;
			if (strrchr(atomPayload, '/') != NULL) {
				
				char* duplicate_info = strdup(atomPayload);
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &pos_in_total); //sscanf into a an unsigned char (uint8_t is typedef'ed to a unsigned char by gcc)
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &the_total);
				
			} else {
				sscanf(atomPayload, "%hhu", &pos_in_total);
			}
			
			//tracknumber (trkn.data)
			if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "trkn", 4) == 0 ) {
				uint8_t track_data[13] = {0,    0, 0, 0, 0, 0, 0, 0, pos_in_total, 0, the_total, 0, 0}; // number of elements (doesn't go into atom value) + 12 uint8_t in atom (does go into atom); 10 & 12 hold the real values
				track_data[0] = (uint8_t)sizeof(track_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, track_data, dataType);
				
			//disknumber (disk.data)
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "disk", 4) == 0 ) {
				uint8_t disk_data[11] = {0,    0, 0, 0, 0, 0, 0, 0, pos_in_total, 0, the_total}; // number of elements (doesn't go into atom value) + 10 uint8_t in atom (does go into atom); 8 & 10 hold the real values
				disk_data[0] = (uint8_t)sizeof(disk_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, disk_data, dataType);

			}
			
		} else if (dataType == AtomicDataClass_UInt8_Binary) {
			if ( (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "cpil", 4) == 0) ||
					 (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "pcst", 4) == 0) ) {
				//compilation/podcast is 5 bytes of data after the data class.... (so is stik or rtng, but there are more tests on atomPayload)
				uint8_t boolean_value = 0; //this is just to remind that the atom value is *set*, it doesn't automatically show up by the presence of the atom
				if (strncmp(atomPayload, "true", 4) == 0) { //anything else *will* set the atom, but have a 0 value - Apple does this too
					boolean_value = 1;
				}
				uint8_t boolAtom_data[6] = {0,    0, 0, 0, 0, boolean_value}; // number of elements (doesn't go into atom value) + 5 uint8_t in atom (does go into atom)
				boolAtom_data[0] = (uint8_t)sizeof(boolAtom_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, boolAtom_data, dataType);
				
				
			} else if ( ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "stik", 4) == 0 ) || 
									( strncmp(parent_atom.AtomicName, "stik", 4) == 0) ) {
				uint8_t stik_value = 0;
				if (strncmp(atomPayload, "Movie", 7) == 0) {
					stik_value = 0; //for a vid to show up in podcasts, it needs pcst, stik & purl set as well
				} else if (strncmp(atomPayload, "Normal", 6) == 0) {
					stik_value = 1; 
				} else if (strncmp(atomPayload, "Whacked Bookmark", 16) == 0) {
					stik_value = 5;
				} else if (strncmp(atomPayload, "Music Video", 11) == 0) {
					stik_value = 6;
				} else if (strncmp(atomPayload, "Short Film", 10) == 0) {
					stik_value = 9;
					} else if (strncmp(atomPayload, "TV Show", 6) == 0) {
					stik_value = 10; //0x0A
				}
				uint8_t stik_data[6] = {0,    0, 0, 0, 0, stik_value}; // number of elements (doesn't go into atom value) + 5 uint8_t in atom (does go into atom)
				stik_data[0] = (uint8_t)sizeof(stik_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, stik_data, dataType);
				
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "rtng", 4) == 0 ) {
				uint8_t rating_value = 0;
				if (strncmp(atomPayload, "clean", 5) == 0) {
					rating_value = 2; //only \02 is clean
				} else if (strncmp(atomPayload, "explicit", 8) == 0) {
					rating_value = 4; //most non \00, \02 numbers are allowed
				}
				uint8_t rtng_data[6] = {0,    0, 0, 0, 0, rating_value}; // number of elements (doesn't go into atom value) + 5 uint8_t in atom (does go into atom)
				rtng_data[0] = (uint8_t)sizeof(rtng_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, rtng_data, dataType);
			
			} else if ( strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "tmpo", 4) == 0 ) {
				uint8_t bpm_value = 0;
				sscanf(atomPayload, "%hhu", &bpm_value );
				uint8_t bpm_data[7] = {0,    0, 0, 0, 0, 0, bpm_value}; // number of elements (doesn't go into atom value) + 6 uint8_t in atom (does go into atom)
				bpm_data[0] = (uint8_t)sizeof(bpm_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, bpm_data, dataType);
			
			} else if ( (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "tvsn", 4) == 0) ||
			            (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "tves", 4) == 0) ) {
				uint8_t data_value = 0; //unsigned char ui_data_value = 0;
				sscanf(atomPayload, "%hhu", &data_value );
				uint8_t given_data[9] = {0,    0, 0, 0, 0, 0, 0, 0, data_value}; // number of elements (doesn't go into atom value) + 8 uint8_t in atom (does go into atom)
				given_data[0] = (uint8_t)sizeof(given_data)/sizeof(uint8_t);
				APar_EncapsulateData(desiredAtom.AtomicNumber, NULL, given_data, dataType);
				
			} else if ( (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "purl", 4) == 0) ||
			            (strncmp(parsedAtoms[APar_FindPrecedingAtom(desiredAtom.AtomicNumber)].AtomicName, "egid", 4) == 0) ) {
				APar_EncapsulateData(desiredAtom.AtomicNumber, atomPayload, NULL, dataType); //the atomPayload string will get added in EncapsulateData
			}			
		}
	}
	
	return;
}

void APar_AddGenreInfo(const char* m4aFile, const char* atomPayload) {
	const char* standard_genre_atom = "moov.udta.meta.ilst.gnre";
	const char* std_genre_data_atom = "moov.udta.meta.ilst.gnre.data";
	const char* custom_genre_atom = "moov.udta.meta.ilst.©gen";
	const char* cstm_genre_data_atom = "moov.udta.meta.ilst.©gen.data";
	modified_atoms = true;
	
	if ( strlen(atomPayload) == 0) {
		APar_RemoveAtom(std_genre_data_atom, false); //find the atom; don't create if it's "" to remove
		APar_RemoveAtom(cstm_genre_data_atom, false); //find the atom; don't create if it's "" to remove
	} else {
	
		short genre_number = StringGenreToInt(atomPayload);
		AtomicInfo genreAtom;
		
		if (genre_number != 0) {
			//first find if a custom genre atom ("©gen") exists; erase the custom-string genre atom in favor of the standard genre atom
			//
			//because a sizeof(array) can't be done in a function (a pointer to the array is passed), the first element will be set to the number of elements, and won't be used as part of atom data.
			uint8_t genre_data[7] = {0,    0, 0, 0, 0, 0, genre_number}; // number of elements (doesn't go into atom value) + 6 uint8_t in atom (does go into atom); 6 holds the real value
			genre_data[0] = (uint8_t)sizeof(genre_data)/sizeof(uint8_t);
			
			AtomicInfo verboten_genre_atom = APar_FindAtom(custom_genre_atom, false, false, true, false);
			
			if (verboten_genre_atom.AtomicNumber != 0) {
				if (strlen(verboten_genre_atom.AtomicName) > 0) {
					if (strncmp(verboten_genre_atom.AtomicName, "©gen", 4) == 0) {
						APar_RemoveAtom(cstm_genre_data_atom, false);
					}
				}
			}
			
			genreAtom = APar_FindAtom(std_genre_data_atom, true, false, true, false);
			APar_EncapsulateData(genreAtom.AtomicNumber, atomPayload, genre_data, AtomicDataClass_UInteger);

		} else {
			
			AtomicInfo verboten_genre_atom = APar_FindAtom(standard_genre_atom, false, false, true, false);

			if (verboten_genre_atom.AtomicNumber > 5 && verboten_genre_atom.AtomicNumber < atom_number) {
				if (strncmp(verboten_genre_atom.AtomicName, "gnre", 4) == 0) {
					APar_RemoveAtom(std_genre_data_atom, false);
				}		
			}
			genreAtom = APar_FindAtom(cstm_genre_data_atom, true, false, true, false);
			APar_EncapsulateData(genreAtom.AtomicNumber, atomPayload, NULL, AtomicDataClass_Text);
		}
	}
	return;
}

void APar_AddMetadataArtwork(const char* m4aFile, const char* artworkPath, char* env_PicOptions) {
	modified_atoms = true;
	const char* artwork_atom = "moov.udta.meta.ilst.covr";
	if (strncasecmp(artworkPath, "REMOVE_ALL", 10) == 0) {
		APar_RemoveAtom(artwork_atom, false);
	
	} else {
		AtomicInfo desiredAtom = APar_FindAtom(artwork_atom, true, false, true, false);
		desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
	
		//determine if any picture preferences will impact the picture file in any way
		myPicturePrefs = ExtractPicPrefs(env_PicOptions);

#if defined(__ppc__)
		char* resized_filepath = ResizeGivenImage(artworkPath , myPicturePrefs);
		if ( strncmp(resized_filepath, "/", 1) == 0 ) {
			APar_EncapsulateData(desiredAtom.AtomicNumber, resized_filepath, NULL, APar_TestArtworkBinaryData(resized_filepath) );
			parsedAtoms[desiredAtom.AtomicNumber].tempFile = true; //THIS desiredAtom holds the temp pic file path
		
			if (myPicturePrefs.addBOTHpix) {
				//create another sparse atom to hold the new file path (otherwise the 2nd will just overwrite the 1st in EncapsulateData
				desiredAtom = APar_FindAtom(artwork_atom, true, false, true, false);
				desiredAtom = APar_CreateSparseAtom(artwork_atom, "data", NULL, 6, true);
				APar_EncapsulateData(desiredAtom.AtomicNumber, artworkPath, NULL, APar_TestArtworkBinaryData(artworkPath) );
			}
		} else {
			APar_EncapsulateData(desiredAtom.AtomicNumber, artworkPath, NULL, APar_TestArtworkBinaryData(artworkPath) );
		}
#else
		//perhaps some libjpeg based resizing/modification for non-Mac OS X based platforms
		APar_EncapsulateData(desiredAtom.AtomicNumber, artworkPath, NULL, APar_TestArtworkBinaryData(artworkPath) );
#endif
	}
	return;
}

void APar_Add_uuid_atom(const char* m4aFile, const char* atom_path, char* uuidName, const int dataType, const char* uuidValue, bool shellAtom) {
	modified_atoms = true;
	char uuid_path[256];
	for (int i = 0; i <= 256; i++) {
		uuid_path[i] = '\00';
	}
	
	if (shellAtom) {
		StringReEncode(uuidName, "ISO-8859-1", "UTF-8"); //all atom names characters are iso8859-1
	}
	
	sprintf(uuid_path, atom_path, uuidName); //gives "moov.udta.meta.ilst.©url"
	
	if ( strlen(uuidValue) == 0) {
		APar_RemoveAtom(uuid_path, true); //find the atom; don't create if it's "" to remove
		//APar_PrintAtomicTree();
	} else {
		//uuid atoms won't have 'data' child atoms - they will carry the data directly as opposed to traditional iTunes-style metadata that does store the information on 'data' atoms. But user-defined is user-defined, so that is how it will be defined here.
		AtomicInfo desiredAtom = APar_FindAtom(uuid_path, true, true, false, false); 
		
		if (dataType == AtomicDataClass_Text) {
			APar_EncapsulateData(desiredAtom.AtomicNumber, uuidValue, NULL, dataType);
		}
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
		
		if ( (strncmp(thisAtom.AtomicName, "mdat", 4) == 0) && (thisAtom.AtomicLevel == 1) && (thisAtom.AtomicLength == 0 || thisAtom.AtomicLength > 75) ) {
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
	parsedAtoms[stco_number].AtomicDataClass = AtomicDataClass_UInteger;
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
		if (parsedAtoms[thisAtomNumber].AtomicLength == 0) {				
	    new_file_size += (long)file_size - parsedAtoms[thisAtomNumber].AtomicStart; //used in progressbar; mdat.length = 1
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

	APar_Move_mdat_Atoms();
	
	if (Create__udta_meta_hdlr__atom) { //this boolean gets set in APar_Verify__udta_meta_hdlr__atom; the hdlr atom (with data) is required by iTunes to enable tagging
		
		//if Quicktime (Player at the least) is used to create any type of mp4 file, the entire udta hierarchy is missing
		//if iTunes doesn't find this "moov.udta.meta.hdlr" atom (and its data), it refuses to let any information be changed
		//the dreaded "Album Artwork Not Modifiable" shows up. It's because this atom is missing. Oddly, QT Player can see the info
		//this only works for mp4/m4a files - it doesn't work for 3gp (it writes perfectly fine, iTunes plays it (not modifiable)
		
		AtomicInfo hdlr_atom = APar_FindAtom("moov.udta.meta.hdlr", false, false, false, true);
		hdlr_atom = APar_CreateSparseAtom("moov.udta.meta", "hdlr", NULL, 4, false);
		APar_EncapsulateData(hdlr_atom.AtomicNumber, NULL, NULL, AtomicDataClass_UInteger);
	}
	
	short last_atom = APar_FindLastAtom();
	//fprintf(stdout, "Last atom is named %s, num:%i\n", parsedAtoms[last_atom].AtomicName, parsedAtoms[last_atom].AtomicNumber);
	short rev_atom_loop = APar_FindPrecedingAtom(parsedAtoms[last_atom].AtomicNumber);
	
	while (rev_atom_loop !=0) {
		short next_atom = 0;
		long atom_size = 0;
		//fprintf(stdout, "preceding atom is named %s, num:%i\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[rev_atom_loop].AtomicNumber);
		
		//if we were to eval our last atom, we would do it here, but it's either "free", "mdat", or a metadata "data" child atom
		//which means, that it's either a top level atom, or a child (and not a container atom), so length has already been determined
		//had it not (or we were on a mission to cover every base), we would eval the last atom before APar_FindPrecedingAtom.
		next_atom = rev_atom_loop;
		rev_atom_loop = APar_FindPrecedingAtom(parsedAtoms[rev_atom_loop].AtomicNumber);
		//fprintf(stdout, "current atom is named %s, num:%i\n", parsedAtoms[rev_atom_loop].AtomicName, parsedAtoms[rev_atom_loop].AtomicNumber);
		short previous_atom = APar_FindPrecedingAtom(rev_atom_loop);

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
	
	if (parsedAtoms[this_atom].AtomicLength > 1 && parsedAtoms[this_atom].AtomicLength < 8) { //mdat length of 1 means it extends to EOF - not yet supported
		return bytes_written;
	}
	
	//write the length of the atom first... taken from our tree in memory
	char4long(parsedAtoms[this_atom].AtomicLength, conv_buffer);
	fseek(temp_file, bytes_written_tally, SEEK_SET);
	fwrite(conv_buffer, 4, 1, temp_file);
	bytes_written += 4;
	
	if (parsedAtoms[this_atom].AtomicLength == 0) { //the spec says if an atom has a length of 0, it extends to EOF
		parsedAtoms[this_atom].AtomicLength = (long)file_size - parsedAtoms[this_atom].AtomicLength;
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
		
		char4long( (long)parsedAtoms[this_atom].AtomicDataClass, conv_buffer); //write the atom data class
		fwrite(conv_buffer, 4, 1, temp_file);
		bytes_written += 4;
		
		char4long( 0, conv_buffer); //write a 4 bytes of null space out
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
			//fprintf(stdout, "%li\n", parsedAtoms[this_atom].AtomicLength);
		}
		
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
		
		if (parsedAtoms[this_atom].AtomicData != NULL) {
			long atom_data_size = 0;
			//fwrite(conv_buffer, 4, 1, temp_file);
			if (strncmp(parsedAtoms[this_atom].AtomicName, "stsd", 4) == 0) {
					atom_data_size = 4;
					
			} else {
				if (parsedAtoms[this_atom].AtomicDataClass == AtomicDataClass_Text) {
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
			//bytes_written += (long)(strlen(parsedAtoms[this_atom].AtomicData)+1);
			fwrite(parsedAtoms[this_atom].AtomicData, atom_data_size, 1, temp_file);
			bytes_written += atom_data_size;
			
			APar_ShellProgressBar(bytes_written_tally + bytes_written);
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
		
		fprintf(stdout, "\n Started writing to temp file.\n");
		
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
		fprintf(stdout, "\n Finished writing to temp file.\n");
		fclose(temp_file);
		
	} else {
		fprintf(stdout, "AtomicParsley error: an error occurred while trying to create a temp file.\n");
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
