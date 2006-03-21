//==================================================================//
/*
    AtomicParsley - AtomicParsley.h

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
                                                                   */
//==================================================================//

#if defined (_WIN32) || defined (_MSC_VER)

#define MAXPATHLEN 255
//and I would really have no idea, but that sounds good
#else
//MAXPATHLEN
#include <sys/param.h>
#endif

#include "AP_AtomExtracts.h"

#ifndef _UINT8_T
#define _UINT8_T
typedef unsigned char         uint8_t;
#endif /*_UINT8_T */

#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short       uint16_t;
#endif /* _UINT16_T */

#ifndef _UINT32_T
#ifndef __uint32_t_defined
typedef unsigned int         uint32_t;
#endif
#endif /*_UINT32_T */

#ifndef _UINT64_T
#define _UINT64_T
#if defined (_MSC_VER)
typedef unsigned __int64	 uint64_t;
#else
typedef unsigned long long   uint64_t;
#endif /* _MSC_VER */
#endif /* _UINT64_T */


// standard classes represented as a 4byte value following the atom name (used mostly for user data atoms).
const int AtomicDataClass_UInteger = 0;     // also steps in to take the place of full blown atom versioning & flagging (1byte ver/3 bytes atom flags; 0x00 00 00 00)
const int AtomicDataClass_Text = 1;
const int AtomicDataClass_JPEGBinary = 13; // \x0D
const int AtomicDataClass_PNGBinary = 14;  // \x0E
const int AtomicDataClass_UInt8_Binary = 21;  // \x15 for cpil, tmpo, rtng, tool; iTMS atoms: cnID, atID, plID, geID, sfID, akID, stik

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

//but I don't think this is applicable to iTunes-style metadata: While 1 (utf8) is in fact utf8,
//it seems that 21 (would be BE-int) is uint8_t (or something along those lines of 8-bit/1byte, *probably* unsigned) - no endian-ness on 8-bit variables
////////////////////////////////////////////////

struct AtomicInfo  {
	short AtomicNumber;
	uint32_t AtomicStart;
	uint32_t AtomicLength;
	uint64_t AtomicLengthExtended;
	char* AtomicName;
	short AtomicLevel;
	int AtomicDataClass;
	char* AtomicData;
	int NextAtomNumber; //our first atom is numbered 0; the last points back to it - so watch it!
	bool tempFile; //used to delete temp pic files (if set as an environmental preference)
	//bool extended_atom;
	bool uuidAtomType;
	uint32_t stsd_codec;
};

struct PicPrefs  {
	int max_dimension;
	int dpi;
	int max_Kbytes;
	bool squareUp;
	bool allJPEG;
	bool allPNG;
	bool addBOTHpix;
	bool removeTempPix;
};

//currently this is only used on Mac OS X to set type/creator for generic '.mp4' file extension files. The Finder 4 character code TYPE is what determines whether a file appears as a video or an audio file in a broad sense.
typedef struct EmployedCodecs {
	bool has_avc1;
	bool has_mp4v;
	bool has_drmi;
	bool has_alac;
	bool has_mp4a;
	bool has_drms;
	bool has_timed_text; //carries the URL - in the mdat stream at a specific time - thus it too is timed.
	bool has_timed_jpeg; //no idea of podcasts support 'png ' or 'tiff'
	bool has_timed_tx3g; //this IS true timed text stream
	bool has_mp4s; //MPEG-4 Systems
	bool has_rtp_hint; //'rtp '; implies hinting
};

typedef struct {
	bool contains_esds;
	uint32_t section3_length;
	uint32_t section4_length;
	uint8_t descriptor_object_typeID;
	uint32_t max_bitrate;
	uint32_t avg_bitrate;
	uint32_t section5_length;
	uint8_t channels;
	uint32_t section6_length;
} esds_AudioInfo;

extern bool parsedfile;

extern bool modified_atoms;

extern bool alter_original;

extern bool move_mdat_atoms;

extern bool cvs_build;

extern EmployedCodecs track_codecs;

extern AtomicInfo parsedAtoms[];

#define AtomicParsley_version	"0.8.4"

#define MAX_ATOMS 350

//--------------------------------------------------------------------------------------------------------------------------------//
//--------------------------------------------------------------------------------------------------------------------------------//

#if defined (WIN32) && defined (__MINGW_H)
//Commented out because it would be a multiply defined symbol on a system that had strsep defined.
//char *strsep (char **stringp, const char *delim);
#endif

uint16_t UInt16FromBigEndian(const char *string);
uint32_t UInt32FromBigEndian(const char *string);
uint64_t UInt64FromBigEndian(const char *string);
void char4TOuint32(uint32_t lnum, char* data); //needed in the header for AP_NSFileUtils

void ShowVersionInfo();
FILE* openSomeFile(const char* file, bool open);
bool TestFileExistence(const char *filePath, bool errorOut);

void APar_PrintDataAtoms(const char *path, bool extract_pix, char* pic_output_path);
void APar_PrintAtomicTree();

void APar_ScanAtoms(const char *path, bool scan_for_tree_ONLY = false);

void APar_DetermineAtomLengths();
AtomicInfo APar_CreateSparseAtom(const char* present_hierarchy, char* new_atom_name,
                                 char* remaining_hierarchy, short atom_level, bool asLastChild);

void APar_AddMetadataInfo(const char* m4aFile, const char* atom_path, const int dataType, const char* atomPayload, bool limited_text);
void APar_AddGenreInfo(const char* m4aFile, const char* atomPayload);
void APar_AddMetadataArtwork(const char* m4aFile, const char* artworkPath, char* env_PicOptions);
void APar_Add_uuid_atom(const char* m4aFile, const char* atom_path, char* uuidName, const int dataType, const char* uuidValue, bool shellAtom);
void APar_StandardTime(char* &formed_time);
void APar_RemoveAtom(const char* atom_path, bool direct_find, bool uuid_atom_type);
void APar_freefree(uint8_t purge_level);
short APar_FindEndingAtom();

void APar_MetadataFileDump(const char* m4aFile);

void APar_TestTracksForKind(); //needed for AP_NSFileUtils
void APar_WriteFile(const char* m4aFile, const char* outfile, bool rewrite_original);

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
v0.7.5e 12/16/2005 ammends how atoms are added at the end of the hierarchy (notably this affects ffmpeg video files); writes "keyw", "catg", "pcst", "aART" atoms; read-only "purl" & "egid" added
v0.7.6  12/31/2005 ceased flawed null-termination (which was implemented more in my mind) of text 'data' atoms; UTF-8 output on Mac OS X & Linux - comment in DUSE_ICONV_CONVERSION in the build file to test it other platforms (maybe my win98Se isn't utf8 aware?); cygwin build accommodations; fix to the secondary "of" number for track/disk on non-PPC; implemented user-defined completely sanctioned 'uuid' atoms to hold.... anything (text only for now); "--tagtime", "--url" & "--information" now get set onto uuid atoms; allow creation of uuid atoms directly from the cli; cygwin-win98SE port added to binary releases; added '--freefree' to remove any&all 'free' atoms
v0.8    01/14/2006 switched over to uint8_t for former ADC_CPIL_TMPO & former ADC_Integer; added podcast stik setting & purl/egid; bugfixes to APar_RemoveAtom; bugfixes & optimizations to APar_FindAtom; changes to text output & set values for stik atom; increase in buffer size; limit non-uuid strings to 255bytes; fixed retreats in progress bar; added purd atom; support mdat.length=0 atom (length=1/64-bit isn't supported; I'll somehow cope with a < 4GB file); switch from long to uint32_t; better x86 bitshifting; added swtich to prevent moving mdat atoms (possible PSP requires mdat before moov); universal binary for Mac OS X release; no text limit on lyrics tag
v0.8.4  02/25/2006 fixed an imaging bug from preferences; fixed metaEnema screwing up the meta atom (APar_RemoveAtom bugfix to remove a direct_find atom); added --output, --overWrite; added --metaDump to dump ONLY metadata tags to a file; versioning for cvs builds; limited support for 64-bit mdat atoms (limited to a little less than a 32-bit atom; > 4GB); bugfixes to APar_RemoveAtom for removing uuid atoms or non-existing atoms & to delete all artwork, then add in 1 command ("--artwork REMOVE_ALL --artwork /path --artwork /path"); support 64-bit co64 atom; support MacOSX-style type/creator codes for tempfiles that end in ".mp4" (no need to change extn to ".m4v"/".m4a" anymore); moved purl/egid onto AtomicDataClass_UInteger (0x00 instead of 0x15) to mirror Apple's change on these tags; start incorporating Brian's Win32 fixes (if you malloc, memset is sure to follow; fopen); give the 'name' atom for '---' iTunes-internal tags for metadata printouts; allow --freefree remove 'free's up to a certain level (preserves iTunes padding); squash some memory leaks; change how CreateSparseAtom was matching atoms to accommodate EliminateAtom-ed atoms (facilitates the previous artwork amendments); exit on unsupported 'ftyp' file brands; anonymous 3rd party native win32 contributions; reworked APar_DetermineAtomLengths to accommodate proper tag setting with --mdatLock; parsing atoms under 'stsd' is no longer internally used - only for tree printing; reworked Mac OS X TYPE determination based on new stsd_codec structure member; revisit co64 offset calculations; start extracting track-level details (dates, language, encoder, channels); changed stco/co64 calculations to support non-muxed files; anonymous "Everyday is NOT like Sunday" contribution; changed unknown 0x15 flagged metadata atoms to hex printouts; move mdat only when moov precedes mdat; new flexible esds parsing
v0.8.X  ??/??/2006 prevent libmp4v2 artwork from a hexdump; changed how short strings were set; win32 change for uuid atoms to avoid sprintf; skip parsing 'free' atoms; work around foobar2000 0.9 non-compliant tagging scheme & added cli switch to give 'tags' the GoLytly - aka '--foobar2000Enema'

*/
