//==================================================================//
/*
    AtomicParsley - AtomicParsley.h

    AtomicParlsey is GPL software; you can freely distribute, 
    redistribute, modify & use under the terms of the GNU General
    Public License; either version 2 or its successor.

    AtomicParlsey is distributed under the GPL "AS IS", without
    any warranty; without the implied warranty of merchantability
    or fitness for either a expressly or implied particular purpose.

    Please see the included GNU General Public License (GPL) for 
    your rights and further details; see the file COPYING. If you
    cannot, write to the Free Software Foundation, 59 Temple Place
    Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

    Copyright ©2005 puck_lock
                                                                   */
//==================================================================//

//MAXPATHLEN
#include <sys/param.h>  

//stco, stsd, drms.... probably more atoms where Apple lies that are parent & leaf atoms
const int AtomicDataClass_NonStandardAtom = -99; //currently unused; perhaps a switch to atom_version = ? would be better.

// standard classes represented as a 4byte value following the atom name (used mostly for user data atoms).
const int AtomicDataClass_Integer = 0;     // bit of a misnomer: this class of data holds 2 byte short integers
const int AtomicDataClass_Text = 1;	       // terminated with a NULL character (an iTunes ©gen.data atom doesn't, but it works)
const int AtomicDataClass_JPEGBinary = 13; // \x0D
const int AtomicDataClass_PNGBinary = 14;  // \x0E
const int AtomicDataClass_CPIL_TMPO = 21;  // \x15 for cpil, tmpo, rtng, tool; iTMS atoms: cnID, atID, plID, geID, sfID, akID, stik

struct AtomicInfo  {
	short AtomicNumber;
	long AtomicStart;
	long AtomicLength;
	char* AtomicName;
	short AtomicLevel;
	int AtomicDataClass;
	char* AtomicData;
	int NextAtomNumber; //out first atom is numbered 0; the last points back to it - so watch it!
};

extern bool parsedfile;

extern bool modified_atoms;

extern bool alter_original;

#define AtomicParsley_version	"0.65"

//--------------------------------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------------------------------//

void openSomeFile(const char* file, bool open);
bool TestFileExistence(const char *filePath, bool errorOut);

void AtomizeFileInfo(AtomicInfo &thisAtom, long Astart, long Alength, char* Astring, short Alevel, int Aclass, int NextAtomNum);

void APar_PrintDataAtoms(const char *path, bool extract_pix, char* pic_output_path);
void APar_PrintAtomicTree();

void APar_ScanAtoms(const char *path);

void APar_DetermineAtomLengths();
AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name, char* remaining_hierarchy, short atom_level);

void APar_AddMetadataInfo(const char* m4aFile, const char* atom_path, const int dataType, const char* atomPayload, bool shellAtom);
void APar_AddGenreInfo(const char* m4aFile, const char* atomPayload);
void APar_AddMetadataArtwork(const char* m4aFile, const char* artworkPath);
void APar_StandardTime(char* &formed_time);
void APar_RemoveAtom(const char* atom_path, bool shellAtom);

void APar_WriteFile(const char* m4aFile, bool rewrite_original);

//--------------------------------------------------------------------------------------------------------------------------------//
// v0.1  10/05/2005 Parsing of atoms; intial Tree printout; extraction of all "covr.data" atoms out to files
// v0.2  11/10/2005 AtomicInfo.NextAtomNumber introduced to facilitate dynamic atom tree reorganization; CreateSparseAtom added
// v0.5  11/22/2005 Writes artist properly of variable lengths properly into an iTMS m4p file properly (other files don't fare well due to the stsd atom non-standard nature); a number of code-uglifying workarounds were employed to get get that far;
// v0.6  11/25/2005 Added genre string/numerical support, support for genre's dual-atom ©gen/gnre nature, genre string->integer; bug fixes to APar_LocateAtomInsertionPoint when an atom is missing; APar_CreateSparseAtom for ordinary non-data atoms are now type -1 (which means they aren't of any interest to us besides length & name); implemnted the Integer data class; char4short; verified iTunes standard genres only go up to "Hard Rock"; added jpg/png artwork embedding into "covr" atoms; slight bugfix for APar_FindAtom (created spurious trailing "covr" atoms).
// v0.6  GPL'ed at sourceforge.net
// v0.7  bugfixes to newly introduced bugs in APar_FindAtom; metaEnema to remove all metadata (safe even for m4p drm files); year implemented properly (tagtime moved onto non-standard 'tdtg' atom ala id3v2.4 - because I like that tag); added setting compilation "cpil" tag (an annoying 5byte tag); added advisory setting (maybe it'll give me a kick one cold winter day-do a "Get Info" in iTunes & in the main "Summary" tab view will be a new little icon next to artwork); added a writeBack flag to for a less beta-like future

// goals for v0.7: integrate "APar_NSImage.mm" NSImage resizing of artwork; environmental preferences for artwork modifications prior to embedding; "cpil" & "tmpo" atom support;
// goals for v0.99 supporting big endian systems
// goals for 1.x UTF-8 support; perhaps even full blown UTF-16 (unlikely).