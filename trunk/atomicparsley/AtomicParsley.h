//==================================================================//
/*
    AtomicParsley - AtomicParsley.h

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

//////////////////////////////////////////what Quicktime has to say on the the data type subject:
/* Well-known data type code
enum {
  kQTMetaDataTypeBinary         = 0,
  kQTMetaDataTypeUTF8           = 1,
  kQTMetaDataTypeUTF16BE        = 2,
  kQTMetaDataTypeMacEncodedText = 3,
  kQTMetaDataTypeSignedIntegerBE = 21,  // The size of the integer is defined by the value size
  kQTMetaDataTypeUnsignedIntegerBE = 22, // The size of the integer is defined by the value size
  kQTMetaDataTypeFloat32BE      = 23,
  kQTMetaDataTypeFloat64BE      = 24
};*/
////////////////////////////////////////////////

struct AtomicInfo  {
	short AtomicNumber;
	long AtomicStart;
	long AtomicLength;
	char* AtomicName;
	short AtomicLevel;
	int AtomicDataClass;
	char* AtomicData;
	int NextAtomNumber; //out first atom is numbered 0; the last points back to it - so watch it!
	bool tempFile; //used to delete temp pic files (if set as an environmental preference)
};

extern bool parsedfile;

extern bool modified_atoms;

extern bool alter_original;

#define AtomicParsley_version	"0.7.5d"

//--------------------------------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------------------------------//

void openSomeFile(const char* file, bool open);
bool TestFileExistence(const char *filePath, bool errorOut);

void AtomizeFileInfo(AtomicInfo &thisAtom, long Astart, long Alength, char* Astring, short Alevel, int Aclass, int NextAtomNum);

void APar_PrintDataAtoms(const char *path, bool extract_pix, char* pic_output_path);
void APar_PrintAtomicTree();

void APar_ScanAtoms(const char *path);

void APar_DetermineAtomLengths();
AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name,
                                 char* remaining_hierarchy, short atom_level, bool asLastChild);

void APar_AddMetadataInfo(const char* m4aFile, const char* atom_path, const int dataType, const char* atomPayload, bool shellAtom);
void APar_AddGenreInfo(const char* m4aFile, const char* atomPayload);
void APar_AddMetadataArtwork(const char* m4aFile, const char* artworkPath, char* env_PicOptions);
void APar_StandardTime(char* &formed_time);
void APar_RemoveAtom(const char* atom_path, bool shellAtom);
short APar_FindEndingAtom();

void APar_WriteFile(const char* m4aFile, bool rewrite_original);

//--------------------------------------------------------------------------------------------------------------------------------//
/*
v0.1   10/05/2005 Parsing of atoms; intial Tree printout; extraction of all "covr.data" atoms out to files
v0.2   11/10/2005 AtomicInfo.NextAtomNumber introduced to facilitate dynamic atom tree reorganization; CreateSparseAtom added
v0.5   11/22/2005 Writes artist properly of variable lengths properly into an iTMS m4p file properly (other files don't fare well due to the stsd atom non-standard nature); a number of code-uglifying workarounds were employed to get get that far;
v0.6   11/25/2005 Added genre string/numerical support, support for genre's dual-atom ©gen/gnre nature, genre string->integer; bug fixes to APar_LocateAtomInsertionPoint when an atom is missing; APar_CreateSparseAtom for ordinary non-data atoms are now type -1 (which means they aren't of any interest to us besides length & name); implemnted the Integer data class; char4short; verified iTunes standard genres only go up to "Hard Rock"; added jpg/png artwork embedding into "covr" atoms; slight bugfix for APar_FindAtom (created spurious trailing "covr" atoms).
v0.6    GPL'ed at sourceforge.net
v0.65   11/25/2005  bugfixes to newly introduced bugs in APar_FindAtom; metaEnema to remove all metadata (safe even for m4p drm files); year implemented properly (tagtime moved onto non-standard 'tdtg' atom ala id3v2.4 - because I like that tag); added setting compilation "cpil" tag (an annoying 5byte tag); added advisory setting (maybe it'll give me a kick one cold winter day-do a "Get Info" in iTunes & in the main "Summary" tab view will be a new little icon next to artwork)
v0.7    11/26/2005 added a writeBack flag to for a less beta-like future; integrated NSImage resizing of artwork; environmental preferences for artwork modifications; build system mods for Mac-specific compiling; 
v0.7.1  11/27/2005 modified parsing & writing to support Apple Lossless (alac) mp4 files. The lovely "alac.alac" non-standard atoms (parents & carry data) caused unplayable files to be written. Only QT ISMA files get screwed now (no idea about Nero)
v0.7.2  11/29/2005 creates iTunes-required meta.hdlr; all the tags now get spit back when reading them (--textdata); slight fix to how atoms are parsed; all known m4a files now tag properly: iTunes (m4a, m4b, chapterized, alac), Quicktime (ISMA & mpeg4 - change filename ext to .m4a to see art; all QT products require the meta.hdlr addition), faac, Helix Producer & Nero; slight change to how PrintDataAtoms called FindParentAtom; added tag time on "©ed1" (edit date-might only really belong directly under udta); added "©url" to hold url; fixes to APar_RemoveAtom; added cli ability to remove all artwork
v0.7.3  12/02/2005 handles stsd (and child) atoms better; modifies all stco offsets when needed (not just the first); new oddball iTMS video "drmi" atom handling; new "stik" atom support (sets iTunes GetInfo->options:Movie,TV Show, Music Video); writes iTMS video drm TV shows well now; diffs in a hex editor are moov atom length, and then into stco, so all is well
v0.7.4  12/03/2005 "desc", "tvnn", "tvsh", "tven" & "tves" setting
v0.7.5b 12/09/2005 forced 'mdat' into being childless (chapterized mpeg4 files have atoms scattered througout mdat, but they aren't children); fixed issues with ffmpeg created mpeg4 files (that have mdat as 2nd atom; moov & chilren as last atoms); moved ffmpeg mdat atoms around to end; better atom adding at the end; subbed getopt_long_only to getopt_long for pre-10.4 users; added progressbar
v0.7.5c 12/10/2005 funnguy0's linux patches (thanks so much for that)
v0.7.5d 12/11/2005 endian issues for x86 mostly resolved; setting genre's segfaults; stik doesn't get set in a multi-option command, but does as a single atom setting; Debian port added to binaries (compiled under debian-31r0a-i386 with g++4.02-2, libc6_2.3.5-8 & libstdc++6_4.0.2-2) - under VirtualPC - with the nano editor!
v0.7.5e 12/12/2005 ammends how atoms are added at the end of the hierarchy (notably this affects ffmpeg video files)

*/
// goals for 0.9 Switch over to uint8, 16, & 32 to carry data; char got unweildy for non-textual data; short sucked for odd bytes.
// goals for 1.x full unicode support; support windows (even though Debian x86 works, it spirals horribly under mingw)
// TODO: revisit how atoms are parsed to get around the tricks for atoms under stsd
