//==================================================================//
/*
    AtomicParsley - main.cpp

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
		* Brian Story - porting getopt & native Win32 patches
                                                                   */
//==================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

//TODO: At some point, cvs commit the getopt.x files and then switch to a unified release; then uncomment this stuff
#if defined (_MSC_VER)
#include "getopt.h"
#else
#include <getopt.h>
#endif

#include "AP_commons.h"
#include "AtomicParsley.h"
#include "AP_AtomExtracts.h"
#include "AP_iconv.h" /* for xmlInitEndianDetection used in endian utf16 conversion */

// define one-letter cli options for
#define OPT_HELP                 'h'
#define OPT_TEST		             'T'
#define OPT_ShowTextData		     't'
#define OPT_ExtractPix           'E'
#define OPT_ExtractPixToPath		 'e'
#define Meta_artist              'a'
#define Meta_songtitle           's'
#define Meta_album               'b'
#define Meta_tracknum            'k'
#define Meta_disknum             'd'
#define Meta_genre               'g'
#define Meta_comment             'c'
#define Meta_year                'y'
#define Meta_lyrics              'l'
#define Meta_composer            'w'
#define Meta_copyright           'x'
#define Meta_grouping            'G'
#define Meta_album_artist        'A'
#define Meta_compilation         'C'
#define Meta_BPM                 'B'
#define Meta_artwork             'r'
#define Meta_advisory            'V'
#define Meta_stik                'S'
#define Meta_description         'p'
#define Meta_TV_Network          'n'
#define Meta_TV_ShowName         'H'
#define Meta_TV_EpisodeNumber    'N'
#define Meta_TV_SeasonNumber     'U'
#define Meta_TV_Episode          'I'
#define Meta_podcastFlag         'f'
#define Meta_category            'q'
#define Meta_keyword             'K'
#define Meta_podcast_URL         'L'
#define Meta_podcast_GUID        'J'
#define Meta_PurchaseDate        'D'

#define Meta_StandardDate        'Z'
#define Meta_URL                 'u'
#define Meta_Information         'i'
#define Meta_uuid                'z'

#define Metadata_Purge           'P'
#define UserData_Purge           'X'
#define foobar_purge             '.'
#define Meta_dump                'Q'
#define Manual_atom_removal      'R'
#define Opt_FreeFree             'F'
#define Opt_Keep_mdat_pos        'M'
#define OPT_OutputFile           'o'

#define OPT_OverWrite            'W'

#define _3GP_Title               '1'
#define _3GP_Author              '2'
#define _3GP_Performer           '3'
#define _3GP_Genre               '4'
#define _3GP_Description         '5'
#define _3GP_Copyright           '6'
#define _3GP_Album               '7'
#define _3GP_Year                '8'
#define _3GP_Rating              '9'
#define _3GP_Classification      '0'
#define _3GP_Keyword             '+'
#define _3GP_Location            '_'

/*
http://developer.apple.com/documentation/QuickTime/APIREF/UserDataIdentifiers.htm#//apple_ref/doc/constant_group/User_Data_Identifiers
©aut		author
©cmt		comment
©cpy		Copyright
©des		description
©dir		Director
©dis		Disclaimer
©nam		FullName
©hst		HostComuter
©inf		Information
©key		Keywords
©mak		Make
©mod		Model
©fmt		Format
©prd		Producer
©PRD		Product
©swr		Software
©req		SpecialPlaybackRequirements
©wrn		Warning
©wrt		Writer
©ed1		EditDate1
©chp		TextChapter
©src		OriginalSource
©prf		Performers

http://developer.apple.com/documentation/QuickTime/APIREF/MetadataKeyConstants.htm#//apple_ref/doc/constant_group/Metadata_Key_Constants
auth		Author
cmmt		comment
dtor		Director
name		DisplayName
info		Information
prod		Producer

http://developer.apple.com/documentation/QuickTime/APIREF/UserDataIDs.htm#//apple_ref/doc/constant_group/User_Data_IDs
©enc		Encoded By
©ope		OriginalArtist
©url		URLLink*/

char *output_file;

int total_args;

static void kill_signal ( int sig );

static void kill_signal (int sig) {
    exit(0);
}

static const char* longHelp_text =
"AtomicParsley help page for setting iTunes-style metadata into MPEG-4 files. \n"
"              (3gp help available with AtomicParsley --3gp-help)\n"
"Usage: AtomicParsley [mp4FILE]... [OPTION]... [ARGUMENT]... [ [OPTION2]...[ARGUMENT2]...] \n"
"\n"
"example: AtomicParsley /path/to.mp4 --E \n"
"example: AtomicParsley /path/to.mp4 --T 1 \n"
"example: Atomicparsley /path/to.mp4 --artist \"Me\" --artwork /path/art.jpg\n"
"example: Atomicparsley /path/to.mp4 --albumArtist \"You\" --podcastFlag true --freefree 2\n"
"example: Atomicparsley /path/to.mp4 --stik \"TV Show\" --advisory explicit --purchaseDate timestamp\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Atom Tree\n"
"\n"
"  --test             ,  -T      Tests file to see if its a valid MPEG-4 file.\n"
"                                Prints out the hierarchical atom tree & some track-level info.\n"
"                                Supplemental track level info with \"-T 1\" or \"--test foo\"\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Atom contents (printing on screen & extracting artwork(s) to files)\n"
"\n"
"  --textdata         ,  -t      prints contents of user data text items out (inc. # of any pics).\n"
"\n"
"  Extract any pictures in user data \"covr\" atoms to separate files. \n"
"  --extractPix       ,  -E                     Extract to same folder (basename derived from file).\n"
"  --extractPixToPath ,  -e  (/path/basename)   Extract to specific path (numbers added to basename).\n"
"                                                 example: --e ~/Desktop/SomeText\n"
"                                                 gives: SomeText_artwork_1.jpg  SomeText_artwork_2.png\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Tag setting options:\n"
"\n"
"  --artist           ,  -a   (str)    Set the artist tag: \"moov.udta.meta.ilst.\302©ART.data\"\n"
"  --title            ,  -s   (str)    Set the title tag: \"moov.udta.meta.ilst.\302©nam.data\"\n"
"  --album            ,  -b   (str)    Set the album tag: \"moov.udta.meta.ilst.\302©alb.data\"\n"
"  --genre            ,  -g   (str)    Set the genre tag: \"\302©gen\" (custom) or \"gnre\" (standard).\n"
"  --tracknum         ,  -k   (num)[/tot]  Set the track number (or track number & total tracks).\n"
"  --disk             ,  -d   (num)[/tot]  Set the disk number (or disk number & total disks).\n"
"  --comment          ,  -c   (str)    Set the comment tag: \"moov.udta.meta.ilst.\302©cmt.data\"\n"
"  --year             ,  -y   (num)    Set the year tag: \"moov.udta.meta.ilst.\302©day.data\"\n"
"  --lyrics           ,  -l   (str)    Set the lyrics tag: \"moov.udta.meta.ilst.\302©lyr.data\"\n"
"  --composer         ,  -w   (str)    Set the composer tag: \"moov.udta.meta.ilst.\302©wrt.data\"\n"
"  --copyright        ,  -x   (str)    Set the copyright tag: \"moov.udta.meta.ilst.cprt.data\"\n"
"  --grouping         ,  -G   (str)    Set the grouping tag: \"moov.udta.meta.ilst.\302©grp.data\"\n"
"  --artwork          ,  -A   (/path)  Set (multiple) artwork (jpeg or png) tag: \"covr.data\"\n"
"  --bpm              ,  -B   (num)    Set the tempo/bpm tag: \"moov.udta.meta.ilst.tmpo.data\"\n"
"  --albumArtist      ,  -A   (str)    Set the album artist tag: \"moov.udta.meta.ilst.aART.data\"\n"
"  --compilation      ,  -C   (bool)   Sets the \"cpil\" atom (true or false to delete the atom)\n"
"  --advisory         ,  -y   (1of3)   Sets the iTunes lyrics advisory ('remove', 'clean', 'explicit') \n"
"  --stik             ,  -S   (1of6)   Sets the iTunes \"stik\" atom (options below + \"remove\") \n"
"                                                \"Movie\", \"Normal\", \"Whacked Bookmark\", \n"
"                                                \"Music Video\", \"Short Film\", \"TV Show\" \n"
"  --description      ,  -p   (str)    Sets the description on the \"desc\" atom\n"
"  --TVNetwork        ,  -n   (str)    Sets the TV Network name on the \"tvnn\" atom\n"
"  --TVShowName       ,  -H   (str)    Sets the TV Show name on the \"tvsh\" atom\n"
"  --TVEpisode        ,  -I   (str)    Sets the TV Episode on \"tven\":\"209\", but its a string: \"209 Part 1\"\n"
"  --TVSeasonNum      ,  -U   (num)    Sets the TV Season number on the \"tvsn\" atom\n"
"  --TVEpisodeNum     ,  -N   (num)    Sets the TV Episode number on the \"tves\" atom\n"

"  --podcastFlag      ,  -f   (bool)   Sets the podcast flag (values are \"true\" or \"false\")\n"
"  --category         ,  -q   (str)    Sets the podcast category; typically a duplicate of its genre\n"
"  --keyword          ,  -q   (str)    Sets the podcast keyword; invisible to MacOSX Spotlight\n"
"  --podcastURL       ,  -L   (URL)    Set the podcast feed URL on the \"purl\" atom\n"
"  --podcastGUID      ,  -J   (URL)    Set the episode's URL tag on the \"egid\" atom\n"
"  --purchaseDate     ,  -D   (UTC)    Set Universal Coordinated Time of purchase on a \"purd\" atom\n"
"                                       (use \"timestamp\" to set UTC to now; can be akin to id3v2 TDTG tag)\n"
"\n"
" To delete a single atom, set the tag to null (except artwork):\n"
"  --artist \"\" --lyrics \"\"\n"
"  --artwork REMOVE_ALL \n"
"  --manualAtomRemove 'moov.udta.meta.ist.hymn' (only works on iTunes-style metadata)\n"
"------------------------------------------------------------------------------------------------\n"
" Setting user-defined 'uuid' tags (all will appear in \"moov.udta.meta\"):\n"
"\n"
"  --information      ,  -i   (str)    Set an information tag on \"moov.udta.meta.uuid=\302©inf\"\n"
"  --url              ,  -u   (URL)    Set a URL tag on \"moov.udta.meta.uuid=\302©url\"\n"
"  --tagtime          ,  -Z            Set the Coordinated Univeral Time of tagging on \"uuid=tdtg\"\n"
"\n"
"  --meta-uuid        ,  -z   (args)   Define & set your own uuid=atom with text data:\n"
"                                        format is 4char_atom_name, 1 or \"text\" & the string to set\n"
"Example: \n"
"  --meta-uuid \"tagr\" 1 'Johnny Appleseed' --meta-uuid \"\302\251sft\" 1 'OpenShiiva encoded.' \n"
"------------------------------------------------------------------------------------------------\n"
" File-level options:\n"
"\n"
"  --metaEnema        ,  -P            Douches away every atom under \"moov.udta.meta.ilst\" \n"
"  --foobar2000Enema  ,  -2            Eliminates foobar2000's non-compliant so-out-o-spec tagging scheme\n"
"  --mdatLock         ,  -M            Prevents moving mdat atoms to the end (poss. useful for PSP files)\n"
"  --freefree         ,  -F   ?(num)?  Remove \"free\" atoms which only act as padding in the file\n"
"                                          (optional: numerical argument - delete 'free' up to desired level)\n"
"  --metaDump         ,  -Q            Dumps out all metadata out to a new file next to original\n"
"                                          (for diagnostic purposes, please remove artwork before sending)\n"
"  --output           ,  -o            Specify the filename of tempfile (voids overWrite)\n"
"  --overWrite        ,  -W            Writes to temp file; deletes original, renames temp to original\n"

"------------------------------------------------------------------------------------------------\n"

#if defined (DARWIN_PLATFORM)
"                   Environmental Variables (affecting picture placement)\n"
"\n"
"  set PIC_OPTIONS in your shell to set these flags; preferences are separated by colons (:)\n"
"\n"
" MaxDimensions=400 (default: 0; unlimited); sets maximum pixel dimensions\n"
" DPI=72            (default: 72); sets dpi\n"
" MaxKBytes=100     (default: 0; unlimited);  maximum kilobytes for file (jpeg only)\n"
" AddBothPix=true   (default: false); add original & converted pic (for archival purposes)\n"
" AllPixJPEG | AllPixPNG =true (default: false); force conversion to a specific picture format\n"
" SquareUp          (include to square images to largest dimension, allows an [ugly] 160x1200->1200x1200)\n"
" removeTempPix     (include to delete temp pic files created when resizing images after tagging)\n"
"\n"
" Examples: (bash-style)\n"
" export PIC_OPTIONS=\"MaxDimensions=400:DPI=72:MaxKBytes=100:AddBothPix=true:AllPixJPEG=true\"\n"
" export PIC_OPTIONS=\"SquareUp:removeTempPix\"\n"
"------------------------------------------------------------------------------------------------\n"
#endif

"\n";


static const char* _3gpHelp_text =
"AtomicParsley 3gp help page for setting 3GPP-style metadata.\n"
"Usage: AtomicParsley [3gpFILE] --option [argument] [optional_arguments]  [ --option2 [argument2]...] \n"
"\n"
"example: AtomicParsley /path/to.3gp --t \n"
"example: AtomicParsley /path/to.3gp --T 1 \n"
"example: Atomicparsley /path/to.3gp --3gp-performer \"Enjoy Yourself\" lang=pol UTF16\n"
"example: Atomicparsley /path/to.3gp --3gp-year 2006 --3gp-album \"White Label\" track=8 lang=fra\n"
"\n"
"example: Atomicparsley /path/to.3gp --3gp-keyword keywords=foo1,foo2,foo3 UTF16\n"
"example: Atomicparsley /path/to.3gp --3gp-location 'Bethesda Terrace' latitude=40.77 longitude=73.98W \n"
"                                                    altitude=4.3B role='real' body=Earth notes='Underground'\n"
"\n"
"----------------------------------------------------------------------------------------------------\n"
"  3GPP text tags can be encoded in either UTF-8 (default input encoding) or UTF-16 (converted from UTF-8)\n"
"  Many 3GPP text tags can be set for a desired language by a 3-letter-lowercase code (default is \"eng\")\n"
"  See   http://www.w3.org/WAI/ER/IG/ert/iso639.htm   to obtain more codes (codes are *not* checked)\n"
"\n"
"  iTunes-style metadata is not supported by the 3GPP TS 26.444 version 6.4.0 Release 6 specification.\n"
"  3GPP tags are set in a different hierarchy: moov.udta (versus iTunes moov.udta.meta.ilst). Some 3rd\n"
"  party utilities may allow setting iTunes-style metadata in 3gp files. When a 3gp file is detected\n"
"  (file extension doesn't matter), only 3gp spec-compliant metadata will be read & written.\n"
"\n"
"  Note: support for each kind of tag with more than 1 language is *not* implemented but allowed for.\n"
"        For each kind of tag, only 1 language is supported.\n"
"\n"
"  Note2: there are a number of different 'brands' that 3GPP files come marked as. Some will not be \n"
"         supported by AtomicParsley due simply to them being unknown and untested. You can compile your\n"
"         own AtomicParsley to evaluate it by adding the hex code into the source of APar_IdentifyBrand.\n"
"\n"
"  Note3: There are slight accuracy discrepancies in location's fixed point decimals set and retrieved.\n"
"\n"
"----------------------------------------------------------------------------------------------------\n"
" Tag setting options (default lang is 'eng'; default encoding is UTF8):\n"
"     required arguments are in (parentheses); optional arguments are in [brackets]\n"
"\n"
"  --3gp-title           (str)   [lang=3str]   [UTF16]  .........  Set a 3gp media title tag\n"
"  --3gp-author          (str)   [lang=3str]   [UTF16]  .........  Set a 3gp author of the media tag\n"
"  --3gp-performer       (str)   [lang=3str]   [UTF16]  .........  Set a 3gp performer or artist tag\n"
"  --3gp-genre           (str)   [lang=3str]   [UTF16]  .........  Set a 3gp genre asset tag\n"
"  --3gp-description     (str)   [lang=3str]   [UTF16]  .........  Set a 3gp description or caption tag\n"
"  --3gp-copyright       (str)   [lang=3str]   [UTF16]  .........  Set a 3gp copyright notice tag\n"
"\n"
"  --3gp-album           (str)   [track=int]  [lang=3str] [UTF16]  Set a 3gp album tag (& opt. tracknum)\n"
"  --3gp-year            (int)   ................................  Set a 3gp recording year tag (4 digit only)\n"
"\n"
"  --3gp-rating          (str)  [entity=4str]  [criteria=4str]  [lang=3str]  [UTF16]  Set a 3gp rating tag\n"
"  --3gp-classification  (str)  [entity=4str]  [criteria=4str]  [lang=3str]  [UTF16]  Set classification tag\n"
"\n"
"  --3gp-keyword         (str)    [lang=3str]   [UTF16]     Format of str is 'keywords=word1,word2,word3,word4'\n"
"\n"
"  --3gp-location        (str)    [lang=3str]   [UTF16]     Set a 3gp location tag (defaults to Central Park)\n"
"                                 [longitude=fxd.pt]  [latitude=fxd.pt]  [altitude=fxd.pt]\n"
"                                 [role=str]  [body=str]  [notes=str]\n"
"                                 fxd.pt values are decimal coordinates (55.01209, 179.25W, 63)\n"
"                                 'role=' values: 'shooting location', 'real location', 'fictional location'\n"
"                                         a negative value in coordinates will be seen as a cli flag\n"
"                                         append 'S', 'W' or 'B': lat=55S, long=90.23W, alt=90.25B\n"
"\n";

void GetBasePath(const char *filepath, char* &basepath) {
	//with a myriad of m4a, m4p, mp4, whatever else comes up... it might just be easiest to strip off the end.
	int split_here = 0;
	for (int i=strlen(filepath); i >= 0; i--) {
		const char* this_char=&filepath[i];
		if ( strncmp(this_char, ".", 1) == 0 ) {
			split_here = i;
			break;
		}
	}
	memcpy(basepath, filepath, (size_t)split_here);
	
	return;
}

void find_optional_args(char *argv[], int start_optindargs, uint16_t &packed_lang, bool &asUTF16, int max_optargs) {
	asUTF16 = false;
	packed_lang = 5575; //und = 0x55C4 = 21956, but QTPlayer doesn't like und //eng =  0x15C7 = 5575
	
	for (int i= 0; i <= max_optargs-1; i++) {
		if ( argv[start_optindargs + i] && start_optindargs + i <= total_args ) {
			if ( memcmp(argv[start_optindargs + i], "lang=", 5) == 0 ) {
				packed_lang = PackLanguage(argv[start_optindargs +i]);
			
			} else if ( memcmp(argv[start_optindargs + i], "UTF16", 5) == 0 ) {
				asUTF16 = true;
			}			
		}
	}
	return;
}

//***********************************************

int main( int argc, char *argv[])
{
	if (argc == 1) {
		fprintf (stdout,"%s", longHelp_text); exit(0);
	} else if (argc == 2 && ((strncmp(argv[1],"-v",2) == 0) || (strncmp(argv[1],"-version",2) == 0)) ) {
	
		ShowVersionInfo();
		
		exit(0);
		
	} else if (argc == 2) {
		if ( (strncmp(argv[1],"-help",5) == 0) || (strncmp(argv[1],"--help",6) == 0) || (strncmp(argv[1],"-h",5) == 0 ) ) {
			fprintf (stdout,"%s", longHelp_text); exit(0);
		} else if ( (strncmp(argv[1],"--3gp-help", 10) == 0) || (strncmp(argv[1],"-3gp-help", 9) == 0) || (strncmp(argv[1],"--3gp-h", 7) == 0) ) {
			fprintf (stdout,"%s", _3gpHelp_text); exit(0);
		}
	}
	
	total_args = argc;
	char *m4afile = argv[1];
	
	TestFileExistence(m4afile, true);
	xmlInitEndianDetection();
	
	//it would probably be better to test output_file if provided & if --overWrite was provided.... probably only of use on Windows - and I'm not on it.
	if (strlen(m4afile) + 11 > MAXPATHLEN) {
		fprintf(stderr, "%c %s", '\a', "AtomicParsley error: filename/filepath was too long.\n");
		exit(1);
	}
	
	while (1) {
	static struct option long_options[] = {
		{ "help",						  0,									NULL,						OPT_HELP },
		{ "test",					  	optional_argument,	NULL,						OPT_TEST },
		{ "textdata",         0,                  NULL,           OPT_ShowTextData },
		{ "extractPix",				0,									NULL,           OPT_ExtractPix },
		{ "extractPixToPath", required_argument,	NULL,				    OPT_ExtractPixToPath },
		{ "artist",           required_argument,  NULL,						Meta_artist },
		{ "title",            required_argument,  NULL,						Meta_songtitle },
		{ "album",            required_argument,  NULL,						Meta_album },
		{ "genre",            required_argument,  NULL,						Meta_genre },
		{ "tracknum",         required_argument,  NULL,						Meta_tracknum },
		{ "disknum",          required_argument,  NULL,						Meta_disknum },
		{ "comment",          required_argument,  NULL,						Meta_comment },
		{ "year",             required_argument,  NULL,						Meta_year },
		{ "lyrics",           required_argument,  NULL,						Meta_lyrics },
		{ "composer",         required_argument,  NULL,						Meta_composer },
		{ "copyright",        required_argument,  NULL,						Meta_copyright },
		{ "grouping",         required_argument,  NULL,						Meta_grouping },
		{ "albumArtist",      required_argument,  NULL,           Meta_album_artist },
    { "compilation",      required_argument,  NULL,						Meta_compilation },
		{ "advisory",         required_argument,  NULL,						Meta_advisory },
    { "bpm",              required_argument,  NULL,						Meta_BPM },
		{ "artwork",          required_argument,  NULL,						Meta_artwork },
		{ "stik",             required_argument,  NULL,           Meta_stik },
    { "description",      required_argument,  NULL,           Meta_description },
    { "TVNetwork",        required_argument,  NULL,           Meta_TV_Network },
    { "TVShowName",       required_argument,  NULL,           Meta_TV_ShowName },
    { "TVEpisode",        required_argument,  NULL,           Meta_TV_Episode },
    { "TVEpisodeNum",     required_argument,  NULL,           Meta_TV_EpisodeNumber },
    { "TVSeasonNum",      required_argument,  NULL,           Meta_TV_SeasonNumber },		
		{ "podcastFlag",      required_argument,  NULL,           Meta_podcastFlag },
		{ "keyword",          required_argument,  NULL,           Meta_keyword },
		{ "category",         required_argument,  NULL,           Meta_category },
		{ "podcastURL",       required_argument,  NULL,           Meta_podcast_URL },
		{ "podcastGUID",      required_argument,  NULL,           Meta_podcast_GUID },
		{ "purchaseDate",     required_argument,  NULL,           Meta_PurchaseDate },
		
		{ "tagtime",          0,                  NULL,						Meta_StandardDate },
		{ "information",      required_argument,  NULL,           Meta_Information },
		{ "url",              required_argument,  NULL,           Meta_URL },
		{ "meta-uuid",        required_argument,  NULL,           Meta_uuid },
		
		{ "freefree",         optional_argument,  NULL,           Opt_FreeFree },
		{ "mdatLock",         0,                  NULL,           Opt_Keep_mdat_pos },
		{ "metaEnema",        0,                  NULL,						Metadata_Purge },
		{ "manualAtomRemove", required_argument,  NULL,           Manual_atom_removal },
		{ "udtaEnema",        0,                  NULL,           UserData_Purge },
		{ "foobar2000Enema",  0,                  NULL,           foobar_purge },
		{ "metaDump",         0,                  NULL,						Meta_dump },
		{ "output",           required_argument,  NULL,						OPT_OutputFile },
		{ "overWrite",        0,                  NULL,						OPT_OverWrite },
		
		{ "3gp-title",        required_argument,  NULL,           _3GP_Title },
		{ "3gp-author",       required_argument,  NULL,           _3GP_Author },
		{ "3gp-performer",    required_argument,  NULL,           _3GP_Performer },
		{ "3gp-genre",        required_argument,  NULL,           _3GP_Genre },
		{ "3gp-description",  required_argument,  NULL,           _3GP_Description },
		{ "3gp-copyright",    required_argument,  NULL,           _3GP_Copyright },
		{ "3gp-album",        required_argument,  NULL,           _3GP_Album },
		{ "3gp-year",         required_argument,  NULL,           _3GP_Year },
		
		{ "3gp-rating",       required_argument,  NULL,           _3GP_Rating },
		{ "3gp-classification",  required_argument,  NULL,           _3GP_Classification },
		{ "3gp-keyword",      required_argument,  NULL,           _3GP_Keyword },
		{ "3gp-location",     required_argument,  NULL,           _3GP_Location },
		
		{ 0, 0, 0, 0 }
	};
		
	int c = -1;
	int option_index = 0; 
	
	c = getopt_long(argc, argv, "hTtEe:a:c:d:f:g:i:l:n:o:pq::u:w:y:z:G:k:A:B:C:D:F:H:I:J:K:L:MN:QR:S:U:WXV:ZP1:2:3:4:5:6:7:8:9:0:",
	                long_options, &option_index);
	
	if (c == -1) {
		if (argc < 3 && argc > 2) {
			APar_ScanAtoms(m4afile, true);
			APar_PrintAtomicTree();
		}
		break;
	}
	
	signal(SIGTERM, kill_signal);
#ifndef WIN32
	signal(SIGKILL, kill_signal);
#endif
	signal(SIGINT,  kill_signal);
	
	switch(c) {
		// "optind" represents the count of arguments up to and including its optional flag:

		case '?': return 1;
			
		case OPT_HELP: {
			fprintf (stdout,"%s", longHelp_text); return 0;
		}
					
		case OPT_TEST: {
			APar_ScanAtoms(m4afile, true);
			APar_PrintAtomicTree();
			if (argv[optind]) {
				APar_ExtractDetails( openSomeFile(m4afile, true) );
			}
			break;
		}
		
		case OPT_ShowTextData: {
			APar_ScanAtoms(m4afile);
			
			openSomeFile(m4afile, true);
			if (metadata_style >= THIRD_GEN_PARTNER) {
				APar_PrintUserDataAssests();
			} else if (metadata_style == ITUNES_STYLE) {
				APar_PrintDataAtoms(m4afile, false, NULL); //false, don't try to extractPix
			}
			openSomeFile(m4afile, false);
			break;
		}
					
		case OPT_ExtractPix: {
			char* base_path=(char*)malloc(sizeof(char)*MAXPATHLEN+1);
			memset(base_path, 0, MAXPATHLEN +1);
			
			GetBasePath( m4afile, base_path );
			APar_ScanAtoms(m4afile);
			openSomeFile(m4afile, true);
			APar_PrintDataAtoms(m4afile, true, base_path); //exportPix to stripped m4afile path
			openSomeFile(m4afile, false);
			break;;
		}
		
		case OPT_ExtractPixToPath: {
			APar_ScanAtoms(m4afile);
			openSomeFile(m4afile, true);
			APar_PrintDataAtoms(m4afile, true, optarg); //exportPix to a different path
			openSomeFile(m4afile, false);
			break;;
		}
				
		case Meta_artist : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "artist") ) {
				break;
			}
			
			short artistData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©ART.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(artistData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_songtitle : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "title") ) {
				break;
			}
			
			short titleData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©nam.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(titleData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_album : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "album") ) {
				break;
			}
			
			short albumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©alb.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(albumData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_genre : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "genre") ) {
				break;
			}
			
			APar_MetaData_atomGenre_Set(optarg);
			break;
		}
				
		case Meta_tracknum : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "track number") ) {
				break;
			}
			
			uint8_t pos_in_total = 0;
			uint8_t the_total = 0; 
			if (strrchr(optarg, '/') != NULL) {
			
				char* duplicate_info = strdup(optarg); //if optarg isn't strdup-ed, when you strsep the last time, it becomes NULL, and getopt has.... issues
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &pos_in_total); //sscanf into a an unsigned char (uint8_t is typedef'ed to a unsigned char by gcc)
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &the_total);
				free(duplicate_info);
				duplicate_info = NULL;
				
			} else {
				sscanf(optarg, "%hhu", &pos_in_total);
			}
			
			short tracknumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.trkn.data", optarg, AtomicDataClass_UInteger);
			//tracknum: [0, 0, 0, 0,   0, 0, 0, pos_in_total, 0, the_total, 0, 0]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, pos_in_total, 8);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, the_total, 8);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			break;
		}
		
		case Meta_disknum : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "disc number") ) {
				break;
			}
			
			uint8_t pos_in_total = 0;
			uint8_t the_total = 0;
			if (strrchr(optarg, '/') != NULL) {
					
				char* duplicate_info = strdup(optarg);
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &pos_in_total); //sscanf into a an unsigned char (uint8_t is typedef'ed to a unsigned char by gcc)
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hhu", &the_total);
				free(duplicate_info);
				duplicate_info = NULL;
			
			} else {
				sscanf(optarg, "%hhu", &pos_in_total);
			}
			
			short disknumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.disk.data", optarg, AtomicDataClass_UInteger);
			//disknum: [0, 0, 0, 0,   0, 0, 0, pos_in_total, 0, the_total]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, pos_in_total, 8);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, the_total, 8);
			break;
		}
		
		case Meta_comment : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "comment") ) {
				break;
			}
			
			short commentData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©cmt.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(commentData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_year : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "year") ) {
				break;
			}
			
			short yearData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©day.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(yearData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_lyrics : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "lyrics") ) {
				break;
			}
			
			short lyricsData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©lyr.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(lyricsData_atom, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Meta_composer : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "composer") ) {
				break;
			}
			
			short composerData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©wrt.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(composerData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_copyright : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "copyright") ) {
				break;
			}
			
			short copyrightData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.cprt.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(copyrightData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_grouping : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "grouping") ) {
				break;
			}
			
			short groupingData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.©grp.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(groupingData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_compilation : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "compilation") ) {
				break;
			}
			
			if (strncmp(optarg, "false", 5) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.cpil", false, false);
			} else {
				//compilation: [0, 0, 0, 0,   boolean_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short compilationData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.cpil.data", optarg, AtomicDataClass_UInt8_Binary);
				APar_Unified_atom_Put(compilationData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 1, 8); //a hard coded uint8_t of: 1 is compilation
			}
			break;
		}
		
		case Meta_BPM : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "BPM") ) {
				break;
			}
			
			if (strncmp(optarg, "0", 1) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.tmpo", false, false);
			} else {
				uint8_t bpm_value = 0;
				sscanf(optarg, "%hhu", &bpm_value );
				//bpm is [0, 0, 0, 0,   0, bpm_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short bpmData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tmpo.data", optarg, AtomicDataClass_UInt8_Binary);
				APar_Unified_atom_Put(bpmData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
				APar_Unified_atom_Put(bpmData_atom, NULL, UTF8_iTunesStyle_256byteLimited, bpm_value, 8);
			}
			break;
		}
		
		case Meta_advisory : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "content advisory") ) {
				break;
			}
			
			if (strncmp(optarg, "remove", 6) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.rtng", false, false);
			} else {
				uint8_t rating_value = 0;
				if (strncmp(optarg, "clean", 5) == 0) {
					rating_value = 2; //only \02 is clean
				} else if (strncmp(optarg, "explicit", 8) == 0) {
					rating_value = 4; //most non \00, \02 numbers are allowed
				}
				//rating is [0, 0, 0, 0,   rating_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short advisoryData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.rtng.data", optarg, AtomicDataClass_UInt8_Binary);
				APar_Unified_atom_Put(advisoryData_atom, NULL, UTF8_iTunesStyle_256byteLimited, rating_value, 8);
			}
			break;
		}
		
		case Meta_artwork : { //handled differently: there can be multiple "moov.udta.meta.ilst.covr.data" atoms
			char* env_PicOptions = getenv("PIC_OPTIONS");
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "coverart") ) {
				break;
			}
			
			APar_MetaData_atomArtwork_Set(optarg, env_PicOptions);
			break;
		}
				
		case Meta_stik : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "'stik'") ) {
				break;
			}
			
			if (strncmp(optarg, "remove", 6) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.stik", false, false);
			} else {
				uint8_t stik_value = 0;
				if (strncmp(optarg, "Movie", 7) == 0) {
					stik_value = 0; //for a vid to show up in podcasts, it needs pcst, stik & purl set as well
				} else if (strncmp(optarg, "Normal", 6) == 0) {
					stik_value = 1; 
				} else if (strncmp(optarg, "Whacked Bookmark", 16) == 0) {
					stik_value = 5;
				} else if (strncmp(optarg, "Music Video", 11) == 0) {
					stik_value = 6;
				} else if (strncmp(optarg, "Short Film", 10) == 0) {
					stik_value = 9;
					} else if (strncmp(optarg, "TV Show", 6) == 0) {
					stik_value = 10; //0x0A
				}
				//stik is [0, 0, 0, 0,   stik_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short stikData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.stik.data", optarg, AtomicDataClass_UInt8_Binary);
				APar_Unified_atom_Put(stikData_atom, NULL, UTF8_iTunesStyle_256byteLimited, stik_value, 8);
			}
			break;
		}
		
		case Meta_description : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "description") ) {
				break;
			}
			
			short descriptionData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.desc.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(descriptionData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_Network : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Network") ) {
				break;
			}
			
			short tvnetworkData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvnn.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(tvnetworkData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_ShowName : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Show name") ) {
				break;
			}
			
			short tvshownameData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvsh.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(tvshownameData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_Episode : { //if the show "ABC Lost 209", its "209"
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Episode string") ) {
				break;
			}
			
			short tvepisodeData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tven.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(tvepisodeData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_SeasonNumber : { //if the show "ABC Lost 209", its 2; integer 2 not char "2"
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Season") ) {
				break;
			}
			
			uint8_t data_value = 0;
			sscanf(optarg, "%hhu", &data_value );
			
			short tvseasonData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvsn.data", optarg, AtomicDataClass_UInt8_Binary);
			//season is [0, 0, 0, 0,   0, 0, 0, data_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tvseasonData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tvseasonData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(tvseasonData_atom, NULL, UTF8_iTunesStyle_256byteLimited, data_value, 8);
			break;
		}
		
		case Meta_TV_EpisodeNumber : { //if the show "ABC Lost 209", its 9; integer 9 (0x09) not char "9"(0x39)
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Episode number") ) {
				break;
			}
			
			uint8_t data_value = 0;
			sscanf(optarg, "%hhu", &data_value );
			
			short tvepisodenumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tves.data", optarg, AtomicDataClass_UInt8_Binary);
			//episodenumber is [0, 0, 0, 0,   0, 0, 0, data_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tvepisodenumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tvepisodenumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 8);
			APar_Unified_atom_Put(tvepisodenumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, data_value, 8);
			break;
		}
		
		case Meta_album_artist : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "album artist") ) {
				break;
			}
			
			short albumartistData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.aART.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(albumartistData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_podcastFlag : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast flag") ) {
				break;
			}
			
			if (strncmp(optarg, "false", 5) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.pcst", false, false);
			} else {
				//podcastflag: [0, 0, 0, 0,   boolean_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short podcastFlagData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.pcst.data", optarg, AtomicDataClass_UInt8_Binary);
				APar_Unified_atom_Put(podcastFlagData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 1, 8); //a hard coded uint8_t of: 1 denotes podcast flag
			}
			
			break;
		}
		
		case Meta_keyword : {    //TODO to the end of iTunes-style metadata & uuid atoms
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "keyword") ) {
				break;
			}
			
			short keywordData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.keyw.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(keywordData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_category : { // see http://www.apple.com/itunes/podcasts/techspecs.html for available categories
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "category") ) {
				break;
			}
			
			short categoryData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.catg.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(categoryData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_podcast_URL : { // usually a read-only value, but useful for getting videos into the 'podcast' menu
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast URL") ) {
				break;
			}
			
			short podcasturlData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.purl.data", optarg, AtomicDataClass_UInteger);
			APar_Unified_atom_Put(podcasturlData_atom, optarg, UTF8_iTunesStyle_Binary, 0, 0);
			break;
		}
		
		case Meta_podcast_GUID : { // Global Unique IDentifier; it is *highly* doubtful that this would be useful...
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast GUID") ) {
				break;
			}
			
			short globalidData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.egid.data", optarg, AtomicDataClass_UInteger);
			APar_Unified_atom_Put(globalidData_atom, optarg, UTF8_iTunesStyle_Binary, 0, 0);
			break;
		}
		
		case Meta_PurchaseDate : { // might be useful to *remove* this, but adding it... although it could function like id3v2 tdtg...
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "purchase date") ) {
				break;
			}
			char* purd_time = (char *)malloc(sizeof(char)*255);
			if (optarg != NULL) {
				if (strncmp(optarg, "timestamp", 9) == 0) {
					APar_StandardTime(purd_time);
				} else {
					purd_time = strdup(optarg);
				}
			} else {
				purd_time = strdup(optarg);
			}
			
			short globalidData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.purd.data", optarg, AtomicDataClass_Text);
			APar_Unified_atom_Put(globalidData_atom, purd_time, UTF8_iTunesStyle_256byteLimited, 0, 0);
			free(purd_time);
			purd_time=NULL;			
			break;
		}
		
		//uuid atoms
		
		case Meta_StandardDate : {
			APar_ScanAtoms(m4afile);
			char* formed_time = (char *)malloc(sizeof(char)*110);
			APar_StandardTime(formed_time);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "tdtg", AtomicDataClass_Text, formed_time, false); //filed apple bug report
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "tdtg", AtomicDataClass_Text, formed_time, false);
			short tdtgUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "tdtg", AtomicDataClass_Text, formed_time, false);
			APar_Unified_atom_Put(tdtgUUID, formed_time, UTF8_iTunesStyle_Unlimited, 0, 0);
			free(formed_time);
			break;
		}
		
		case Meta_URL : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "©url", AtomicDataClass_Text, optarg, false); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "©url", AtomicDataClass_Text, optarg, false);
			short urlUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "©url", AtomicDataClass_Text, optarg, false);
			APar_Unified_atom_Put(urlUUID, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Meta_Information : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "©inf", AtomicDataClass_Text, optarg, false); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "©inf", AtomicDataClass_Text, optarg, false);
			short infoUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "©inf", AtomicDataClass_Text, optarg, false);
			APar_Unified_atom_Put(infoUUID, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}

		case Meta_uuid : {
			APar_ScanAtoms(m4afile);
			
			//uuid atoms are handled differently, because they are user/private-extension atoms
			//a standard path is provided in the "path.form", however a uuid atom has a name of 'uuid' in the vein of the traditional atom name
			//PLUS a uuid extended 4byte name (1st argument), and then the number of the datatype (0,1,21) & the actual data  (3rd argument)
			
			// --meta-uuid "©foo" 1 'http://www.url.org'
			
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", optarg, AtomicDataClass_Text, argv[optind +1], true); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", optarg, AtomicDataClass_Text, argv[optind +1], true);
			
			short genericUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", optarg, AtomicDataClass_Text, argv[optind +1], true);
			APar_Unified_atom_Put(genericUUID, argv[optind +1], UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Manual_atom_removal : {
			APar_ScanAtoms(m4afile);
			
			char* compliant_name = (char*)malloc(sizeof(char)* strlen(optarg) +1);
			memset(compliant_name, 0, strlen(optarg) +1);
			UTF8Toisolat1((unsigned char*)compliant_name, strlen(optarg), (unsigned char*)optarg, strlen(optarg) );
			
			APar_RemoveAtom(compliant_name, false, false);
			break;
		}
		
		//3gp tags
		
		case _3GP_Title : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "title") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short title_3GP_atom = APar_UserData_atom_Init("moov.udta.titl", optarg);
			APar_Unified_atom_Put(title_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Author : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "author") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short author_3GP_atom = APar_UserData_atom_Init("moov.udta.auth", optarg);
			APar_Unified_atom_Put(author_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Performer : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "performer") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short performer_3GP_atom = APar_UserData_atom_Init("moov.udta.perf", optarg);
			APar_Unified_atom_Put(performer_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Genre : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "genre") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short genre_3GP_atom = APar_UserData_atom_Init("moov.udta.gnre", optarg);
			APar_Unified_atom_Put(genre_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Description : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "description") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short description_3GP_atom = APar_UserData_atom_Init("moov.udta.dscp", optarg);
			APar_Unified_atom_Put(description_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Copyright : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "copyright") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 2);
			
			short copyright_3GP_atom = APar_UserData_atom_Init("moov.udta.cprt", optarg);
			APar_Unified_atom_Put(copyright_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;
		}
		
		case _3GP_Album : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER_VER1_REL6, 3, NULL) ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 3);
			uint8_t tracknum = 0;
			
			short album_3GP_atom = APar_UserData_atom_Init("moov.udta.albm", optarg);
			APar_Unified_atom_Put(album_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			
			//cygle through the remaining independant arguments (before the next --cli_flag) and figure out if any are useful to us; already have lang & utf16
			for (int i= 0; i < 3; i++) { //3 possible arguments for this tag (the first - which doesn't count - is the data for the tag itself)
				if ( argv[optind + i] && optind + i <= total_args) {
					if ( memcmp(argv[optind + i], "track=", 6) == 0 ) {
						strsep(&argv[optind + i],"=");
						sscanf(argv[optind + i], "%hhu", &tracknum);
						APar_Unified_atom_Put(album_3GP_atom, NULL, UTF8_3GP_Style, (uint32_t)tracknum, 8);
					}			
				}
			}
			break;
		}
		
		case _3GP_Year : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "year") ) {
				break;
			}
			uint16_t year_tag = 0;
			sscanf(optarg, "%hu", &year_tag);
			
			short rec_year_3GP_atom = APar_UserData_atom_Init("moov.udta.yrrc", optarg);
			APar_Unified_atom_Put(rec_year_3GP_atom, NULL, UTF8_3GP_Style, (uint32_t)year_tag, 16);
			break;	
		}
		
		case _3GP_Rating : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "rating") ) {
				break;
			}
			char rating_entity[5] = { 0x20, 0x20, 0x20, 0x20, 0 }; //'    ' (4 spaces) - thats what it will be if not provided
			char rating_criteria[5] = { 0x20, 0x20, 0x20, 0x20, 0 };
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 4);
			
			for (int i= 0; i < 4; i++) { //3 possible arguments for this tag (the first - which doesn't count - is the data for the tag itself)
				if ( argv[optind + i] && optind + i <= total_args) {
					if ( memcmp(argv[optind + i], "entity=", 7) == 0 ) {
						strsep(&argv[optind + i],"=");
						memcpy(&rating_entity, argv[optind + i], 4);
					}
					if ( memcmp(argv[optind + i], "criteria=", 9) == 0 ) {
						strsep(&argv[optind + i],"=");
						memcpy(&rating_criteria, argv[optind + i], 4);
					}
				}
			}
			short rating_3GP_atom = APar_UserData_atom_Init("moov.udta.rtng", optarg);
			
			APar_Unified_atom_Put(rating_3GP_atom, NULL, UTF8_3GP_Style, UInt32FromBigEndian(rating_entity), 32);
			APar_Unified_atom_Put(rating_3GP_atom, NULL, UTF8_3GP_Style, UInt32FromBigEndian(rating_criteria), 32);
			APar_Unified_atom_Put(rating_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;	
		}
		
		case _3GP_Classification : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "classification") ) {
				break;
			}
			char classification_entity[5] = { 0x20, 0x20, 0x20, 0x20, 0 }; //'    ' (4 spaces) - thats what it will be if not provided
			uint16_t classification_index = 0;
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 4);
			
			for (int i= 0; i < 4; i++) { //3 possible arguments for this tag (the first - which doesn't count - is the data for the tag itself)
				if ( argv[optind + i] && optind + i <= total_args) {
					if ( memcmp(argv[optind + i], "entity=", 7) == 0 ) {
						strsep(&argv[optind + i],"=");
						memcpy(&classification_entity, argv[optind + i], 4);
					}
					if ( memcmp(argv[optind + i], "index=", 6) == 0 ) {
						strsep(&argv[optind + i],"=");
						sscanf(argv[optind + i], "%hu", &classification_index);
					}
				}
			}
			short classification_3GP_atom = APar_UserData_atom_Init("moov.udta.clsf", optarg);
			
			APar_Unified_atom_Put(classification_3GP_atom, NULL, UTF8_3GP_Style, UInt32FromBigEndian(classification_entity), 32);
			APar_Unified_atom_Put(classification_3GP_atom, NULL, UTF8_3GP_Style, classification_index, 16);
			APar_Unified_atom_Put(classification_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			break;	
		}
		
		case _3GP_Keyword : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "keyword") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 3);
			
			if (strrchr(optarg, '=') != NULL) { //must be in the format of:   keywords=foo1,foo2,foo3,foo4
				char* keywords_globbed = strsep(&optarg,"="); //separate out 'keyword='
				keywords_globbed = strsep(&optarg,"="); //this is what we want to work on: just the keywords
				char* keyword_ptr = keywords_globbed;
				uint32_t keyword_strlen = strlen(keywords_globbed);
				uint8_t keyword_count = 0;
				uint32_t key_index = 0;
				
				if (keyword_strlen > 0) { //if there is anything past the = then it counts as a keyword
					keyword_count++;
				}
				
				while (true) { //count occurrences of comma here
					if (*keyword_ptr == ',') {
						keyword_count++;
					}
					keyword_ptr++;
					key_index++;
					if (keyword_strlen == key_index) {
						break;
					}
				}

				short keyword_3GP_atom = APar_UserData_atom_Init("moov.udta.kywd", keyword_strlen ? "temporary" : ""); //just a "temporary" valid string to satisfy a test there
				if (keyword_strlen > 0) {
					APar_Unified_atom_Put(keyword_3GP_atom, NULL, UTF8_3GP_Style, (uint32_t)packed_lang, 16);
					APar_Unified_atom_Put(keyword_3GP_atom, NULL, UTF8_3GP_Style, keyword_count, 8);
					char* formed_keyword_struct = (char*)malloc(sizeof(char)* set_UTF16_text ? keyword_strlen * 4 : keyword_strlen * 2); //*4 should carry utf16's BOM & TERM
					memset(formed_keyword_struct, 0, set_UTF16_text ? keyword_strlen * 4 : keyword_strlen * 2 );
					uint32_t keyword_struct_bytes = APar_3GP_Keyword_atom_Format(keywords_globbed, keyword_count, set_UTF16_text, formed_keyword_struct);
					APar_atom_Binary_Put(keyword_3GP_atom, formed_keyword_struct, keyword_struct_bytes, 3);
					free(formed_keyword_struct);
					formed_keyword_struct = NULL;
				}
			} else {
				APar_UserData_atom_Init("moov.udta.kywd", "");
			}
			break;	
		}
		
		case _3GP_Location : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style >= THIRD_GEN_PARTNER, 2, "location") ) {
				break;
			}
			bool set_UTF16_text = false;
			uint16_t packed_lang = 0;
			find_optional_args(argv, optind, packed_lang, set_UTF16_text, 9);
			
			
			double longitude = -73.98; //if you don't provide a place, you WILL be put right into Central Park. Welcome to New York City... now go away.
			double latitude = 40.77;
			double altitude = 4.3;
			uint8_t role = 0;
			char* astronomical_body = "Earth";
			char* additional_notes = "no notes";
			
			for (int i= 0; i < 9; i++) { //9 possible arguments for this tag (the first - which doesn't count - is the data for the tag itself)
				if ( argv[optind + i] && optind + i <= total_args) {
					if ( memcmp(argv[optind + i], "longitude=", 10) == 0 ) {
						strsep(&argv[optind + i],"=");
						sscanf(argv[optind + i], "%lf", &longitude);
						//fprintf(stdout, "%s %i\n", argv[optind + i], argv[optind + i][strlen(argv[optind + i])-1]);
						if (argv[optind + i][strlen(argv[optind + i])-1] == 'W') {
							longitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "latitude=", 9) == 0 ) {
						strsep(&argv[optind + i],"=");
						sscanf(argv[optind + i], "%lf", &latitude);
						//fprintf(stdout, "%s %i\n", argv[optind + i], argv[optind + i][strlen(argv[optind + i])-1]);
						if (argv[optind + i][strlen(argv[optind + i])-1] == 'S') {
							latitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "altitude=", 9) == 0 ) {
						strsep(&argv[optind + i],"=");
						sscanf(argv[optind + i], "%lf", &altitude);
						//fprintf(stdout, "%s %i\n", argv[optind + i], argv[optind + i][strlen(argv[optind + i])-1]);
						if (argv[optind + i][strlen(argv[optind + i])-1] == 'B') {
							altitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "role=", 5) == 0 ) {
						strsep(&argv[optind + i],"=");
						if (strncmp(argv[optind + i], "shooting location", 17) == 0 || strncmp(argv[optind + i], "shooting", 8) == 0) {
							role = 0;
						} else if (strncmp(argv[optind + i], "real location", 13) == 0 || strncmp(argv[optind + i], "real", 4) == 0) {
							role = 1;
						} else if (strncmp(argv[optind + i], "fictional location", 18) == 0 || strncmp(argv[optind + i], "fictional", 9) == 0) {
							role = 2;
						}
					}
					if ( memcmp(argv[optind + i], "body=", 5) == 0 ) {
						strsep(&argv[optind + i],"=");
						astronomical_body = argv[optind + i];
					}
					if ( memcmp(argv[optind + i], "notes=", 6) == 0 ) {
						strsep(&argv[optind + i],"=");
						additional_notes = argv[optind + i];
					}
				}
			}
			
			//fprintf(stdout, "long, lat, alt = %lf %lf %lf\n", longitude, latitude, altitude);
			
			if (longitude < -180.0 || longitude > 180.0 || latitude < -90.0 || latitude > 90.0) {
				fprintf(stdout, "AtomicParsley warning: longitude or latitude was invalid; skipping setting location\n");
			} else {
			
				short location_3GP_atom = APar_UserData_atom_Init("moov.udta.loci", optarg);
				APar_Unified_atom_Put(location_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, (uint32_t)role, 8);
				
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, float_to_16x16bit_fixed_point(longitude), 32);
				//fprintf(stdout, "%lf %lf %lf\n", longitude, latitude, altitude);
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, float_to_16x16bit_fixed_point(latitude), 32);
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, float_to_16x16bit_fixed_point(altitude), 32);
				APar_Unified_atom_Put(location_3GP_atom, astronomical_body, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), 0, 0);
				APar_Unified_atom_Put(location_3GP_atom, additional_notes, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), 0, 0);
			}
			break;	
		}
		
		//utility functions
		
		case Metadata_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.meta.ilst", true, false);
			
			break;
		}
		
		case UserData_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta", true, false);
			
			break;
		}
		
		case foobar_purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.tags", true, false);
			
			break;
		}
		
		case Opt_FreeFree : {
			APar_ScanAtoms(m4afile);
			uint8_t free_level = 0;
			if (argv[optind]) {
				sscanf(argv[optind], "%hhu", &free_level);
			}
			APar_freefree(free_level);
			
			break;
		}
		
		case Opt_Keep_mdat_pos : {
			move_mdat_atoms = false;
			break;
		}
		
		case OPT_OverWrite : {
			alter_original = true;
			break;
		}
		
		case Meta_dump : {
			APar_ScanAtoms(m4afile);
			openSomeFile(m4afile, true);
			APar_MetadataFileDump(m4afile);
			openSomeFile(m4afile, false);
			
			exit(0); //das right, this is a flag that doesn't get used with other flags.
		}
		
		case OPT_OutputFile : {
			output_file = (char *)malloc(sizeof(char)* MAXPATHLEN +1);
			memset(output_file, 0, MAXPATHLEN +1);
			output_file = strdup(optarg);
			break;
		}
		
		} /* end switch */
	} /* end while */
	
	//after all the modifications are enacted on the tree in memory, THEN write out the changes
	if (modified_atoms) {
		APar_DetermineAtomLengths();
		openSomeFile(m4afile, true);
		APar_WriteFile(m4afile, output_file, alter_original);
		if (!alter_original) {
			//The file was opened orignally as read-only; when it came time to writeback into the original file, that FILE was closed, and a new one opened with write abilities, so to close a FILE that no longer exists would.... be retarded.
			openSomeFile(m4afile, false);
		}
		if (!output_file) {
			free(output_file);
			output_file=NULL;
		}
	}
	return 0;
}
