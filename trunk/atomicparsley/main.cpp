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

    Copyright �2005-2006 puck_lock
		
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
#include <wchar.h>

#if defined (_MSC_VER)
#include "getopt.h"
#else
#include <getopt.h>
#endif

#include "AP_commons.h"
#include "AtomicParsley.h"
#include "AP_AtomExtracts.h"
#include "AP_iconv.h"                 /* for xmlInitEndianDetection used in endian utf16 conversion */
#include "AtomicParsley_genres.h"     //for stik comparison function

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
#define Meta_EncodingTool        0xB7

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

#define _3GP_Title               0xAB
#define _3GP_Author              0xAC
#define _3GP_Performer           0xAD
#define _3GP_Genre               0xAE
#define _3GP_Description         0xAF
#define _3GP_Copyright           0xB0
#define _3GP_Album               0xB1
#define _3GP_Year                0xB2
#define _3GP_Rating              0xB3
#define _3GP_Classification      0xB4
#define _3GP_Keyword             0xB5
#define _3GP_Location            0xB6

/*
http://developer.apple.com/documentation/QuickTime/APIREF/UserDataIdentifiers.htm#//apple_ref/doc/constant_group/User_Data_Identifiers
�aut		author
�cmt		comment
�cpy		Copyright
�des		description
�dir		Director
�dis		Disclaimer
�nam		FullName
�hst		HostComuter
�inf		Information
�key		Keywords
�mak		Make
�mod		Model
�fmt		Format
�prd		Producer
�PRD		Product
�swr		Software
�req		SpecialPlaybackRequirements
�wrn		Warning
�wrt		Writer
�ed1		EditDate1
�chp		TextChapter
�src		OriginalSource
�prf		Performers

http://developer.apple.com/documentation/QuickTime/APIREF/MetadataKeyConstants.htm#//apple_ref/doc/constant_group/Metadata_Key_Constants
auth		Author
cmmt		comment
dtor		Director
name		DisplayName
info		Information
prod		Producer

http://developer.apple.com/documentation/QuickTime/APIREF/UserDataIDs.htm#//apple_ref/doc/constant_group/User_Data_IDs
�enc		Encoded By
�ope		OriginalArtist
�url		URLLink*/

char *output_file;

int total_args;

static void kill_signal ( int sig );

static void kill_signal (int sig) {
    exit(0);
}

//less than 80 (max 78) char wide, giving a general (concise) overview
static char* shortHelp_text =
"\n"
"AtomicParlsey quick help for setting iTunes-style metadata into MPEG-4 files.\n"
"\n"
"General usage examples:\n"
"  AtomicParsley /path/to.mp4 -T 1\n"
"  AtomicParsley /path/to.mp4 -t +\n"
"  AtomicParsley /path/to.mp4 --artist \"Me\" --artwork /path/to/art.jpg\n"
"  Atomicparsley /path/to.mp4 --albumArtist \"You\" --podcastFlag true\n"
"  Atomicparsley /path/to.mp4 --stik \"TV Show\" --advisory explicit\n"
"\n"
"Getting information about the file & tags:\n"
"  -T  --test        Test file for mpeg4-ishness & print atom tree\n"
"  -t  --textdata    Prints tags embedded within the file\n"
"  -E  --extractPix  Extracts pix to the same folder as the mpeg-4 file\n"
"\n"
"Setting iTunes-style metadata tags\n"
"  --artist       (string)     Set the artist tag\n"
"  --title        (string)     Set the title tag\n"
"  --album        (string)     Set the album tag\n"
"  --genre        (string)     Genre tag (see --longhelp for more info)\n"
"  --tracknum     (num)[/tot]  Track number (or track number/total tracks)\n"
"  --disk         (num)[/tot]  Disk number (or disk number/total disks)\n"
"  --comment      (string)     Set the comment tag\n"
"  --year         (num|UTC)    Year tag (see --longhelp for \"Release Date\")\n"
"  --lyrics       (string)     Set lyrics (not subject to 256 byte limit)\n"
"  --composer     (string)     Set the composer tag\n"
"  --copyright    (string)     Set the copyright tag\n"
"  --grouping     (string)     Set the grouping tag\n"
"  --artwork      (/path)      Set a piece of artwork (jpeg or png only)\n"
"  --bpm          (number)     Set the tempo/bpm\n"
"  --albumArtist  (string)     Set the album artist tag\n"
"  --compilation  (boolean)    Set the compilation flag (true or false)\n"
"  --advisory     (string*)    Content advisory (*values: 'clean', 'explicit')\n"
"  --stik         (string*)    Sets the iTunes \"stik\" atom (see --longhelp)\n"
"  --description  (string)     Set the description tag\n"
"  --TVNetwork    (string)     Set the TV Network name\n"
"  --TVShowName   (string)     Set the TV Show name\n"
"  --TVEpisode    (string)     Set the TV episode/production code\n"
"  --TVSeasonNum  (number)     Set the TV Season number\n"
"  --TVEpisodeNum (number)     Set the TV Episode number\n"
"  --podcastFlag  (boolean)    Set the podcast flag (true or false)\n"
"  --category     (string)     Sets the podcast category\n"
"  --keyword      (string)     Sets the podcast keyword\n"
"  --podcastURL   (URL)        Set the podcast feed URL\n"
"  --podcastGUID  (URL)        Set the episode's URL tag\n"
"  --purchaseDate (UTC)        Set time of purchase\n"
"  --encodingTool (string)     Set the name of the encoder\n"
"\n"
"Deleting tags\n"
"  Set the value to \"\":        --artist \"\" --stik \"\" --bpm \"\"\n"
"  To delete (all) artwork:    --artwork REMOVE_ALL\n"
"  manually removal:           --manualAtomRemove \"moov.udta.meta.ilst.ATOM\"\n"
"\n"
"More detailed help is available with AtomicParsley --longhelp\n"
"Setting 3gp assets into 3GPP & derivative files: see --3gp-help\n"
"----------------------------------------------------------------------";

//an expansive, verbose, unconstrained (about 112 char wide) detailing of options
static char* longHelp_text =
"AtomicParsley help page for setting iTunes-style metadata into MPEG-4 files. \n"
"              (3gp help available with AtomicParsley --3gp-help)\n"
"Usage: AtomicParsley [mp4FILE]... [OPTION]... [ARGUMENT]... [ [OPTION2]...[ARGUMENT2]...] \n"
"\n"
"example: AtomicParsley /path/to.mp4 -e ~/Desktop/pix\n"
"example: Atomicparsley /path/to.mp4 --podcastURL \"http://www.url.net\" --tracknum 45/356\n"
"example: Atomicparsley /path/to.mp4 --copyright \"\342\204\227 \302\251 2006\"\n"
"example: Atomicparsley /path/to.mp4 --year \"2006-07-27T14:00:43Z\" --purchaseDate timestamp\n"
"\n"
#if defined (_MSC_VER)
"  Note: you can change the input/output behavior to raw 8-bit utf8 if the program name\n"
"        is appended with \"-utf8\". AtomicParsley-utf8.exe will have problems with files/\n"
"        folders with unicode characters in given paths.\n"
"\n"
#endif
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
"  --textdata         ,  -t      show user data text metadata relevant to brand (inc. # of any pics).\n"
"                        -t 1    show metadata regardless of brand with (1 can be anything)\n"
"                        -t +    show supplemental info like free space, available padding, user data\n"
"                                length & media data length\n"
"\n"
"  Extract any pictures in user data \"covr\" atoms to separate files. \n"
"  --extractPix       ,  -E                     Extract to same folder (basename derived from file).\n"
"  --extractPixToPath ,  -e  (/path/basename)   Extract to specific path (numbers added to basename).\n"
"                                                 example: --e ~/Desktop/SomeText\n"
"                                                 gives: SomeText_artwork_1.jpg  SomeText_artwork_2.png\n"
"                                               Note: extension comes from embedded image file format\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Tag setting options:\n"
"\n"
"  --artist           ,  -a   (str)    Set the artist tag: \"moov.udta.meta.ilst.\302�ART.data\"\n"
"  --title            ,  -s   (str)    Set the title tag: \"moov.udta.meta.ilst.\302�nam.data\"\n"
"  --album            ,  -b   (str)    Set the album tag: \"moov.udta.meta.ilst.\302�alb.data\"\n"
"  --genre            ,  -g   (str)    Set the genre tag: \"\302�gen\" (custom) or \"gnre\" (standard).\n"
"                                          see the standard list with \"AtomicParsley --genre-list\"\n"
"  --tracknum         ,  -k   (num)[/tot]  Set the track number (or track number & total tracks).\n"
"  --disk             ,  -d   (num)[/tot]  Set the disk number (or disk number & total disks).\n"
"  --comment          ,  -c   (str)    Set the comment tag: \"moov.udta.meta.ilst.\302�cmt.data\"\n"
"  --year             ,  -y   (num|UTC)    Set the year tag: \"moov.udta.meta.ilst.\302�day.data\"\n"
"                                          set with UTC \"2006-09-11T09:00:00Z\" for Release Date\n"
"  --lyrics           ,  -l   (str)    Set the lyrics tag: \"moov.udta.meta.ilst.\302�lyr.data\"\n"
"  --composer         ,  -w   (str)    Set the composer tag: \"moov.udta.meta.ilst.\302�wrt.data\"\n"
"  --copyright        ,  -x   (str)    Set the copyright tag: \"moov.udta.meta.ilst.cprt.data\"\n"
"  --grouping         ,  -G   (str)    Set the grouping tag: \"moov.udta.meta.ilst.\302�grp.data\"\n"
"  --artwork          ,  -A   (/path)  Set a piece of artwork (jpeg or png) on \"covr.data\"\n"
"  --bpm              ,  -B   (num)    Set the tempo/bpm tag: \"moov.udta.meta.ilst.tmpo.data\"\n"
"  --albumArtist      ,  -A   (str)    Set the album artist tag: \"moov.udta.meta.ilst.aART.data\"\n"
"  --compilation      ,  -C   (bool)   Sets the \"cpil\" atom (true or false to delete the atom)\n"
"  --advisory         ,  -y   (1of3)   Sets the iTunes lyrics advisory ('remove', 'clean', 'explicit') \n"
"  --stik             ,  -S   (1of7)   Sets the iTunes \"stik\" atom (--stik \"remove\" to delete) \n"
"                                           \"Movie\", \"Normal\", \"TV Show\" .... others: \n"
"                                           see the full list with \"AtomicParsley --stik-list\"\n"
"                                           or set in an integer value with --stik value=(num)\n"
"  --description      ,  -p   (str)    Sets the description on the \"desc\" atom\n"
"  --TVNetwork        ,  -n   (str)    Sets the TV Network name on the \"tvnn\" atom\n"
"  --TVShowName       ,  -H   (str)    Sets the TV Show name on the \"tvsh\" atom\n"
"  --TVEpisode        ,  -I   (str)    Sets the TV Episode on \"tven\":\"209\", but its a string: \"209 Part 1\"\n"
"  --TVSeasonNum      ,  -U   (num)    Sets the TV Season number on the \"tvsn\" atom\n"
"  --TVEpisodeNum     ,  -N   (num)    Sets the TV Episode number on the \"tves\" atom\n"

"  --podcastFlag      ,  -f   (bool)   Sets the podcast flag (values are \"true\" or \"false\")\n"
"  --category         ,  -q   (str)    Sets the podcast category; typically a duplicate of its genre\n"
"  --keyword          ,  -K   (str)    Sets the podcast keyword; invisible to MacOSX Spotlight\n"
"  --podcastURL       ,  -L   (URL)    Set the podcast feed URL on the \"purl\" atom\n"
"  --podcastGUID      ,  -J   (URL)    Set the episode's URL tag on the \"egid\" atom\n"
"  --purchaseDate     ,  -D   (UTC)    Set Universal Coordinated Time of purchase on a \"purd\" atom\n"
"                                       (use \"timestamp\" to set UTC to now; can be akin to id3v2 TDTG tag)\n"
"  --encodingTool     ,       (str)    Set the name of the encoder on the \"\302�too\" atom\n"
"\n"
" To delete a single atom, set the tag to null (except artwork):\n"
"  --artist \"\" --lyrics \"\"\n"
"  --artwork REMOVE_ALL \n"
"  --manualAtomRemove \"some.atom.path\" where some.atom.path can be:\n"
"                             \"moov.udta.ATOM\" for an child or parent atom in 'udta' (but not 3gp assets)\n"
"                             \"moov.udta.ATOM:lang=eng\" for a 3gp asset of a specific language\n"
"                             \"moov.udta.meta.ilst.ATOM\" or\n"
"                             \"moov.udta.meta.ilst.ATOM.data\" for iTunes-style metadata\n"
"                             \"moov.udta.meta.ilst.----.name:[foo]\" for reverse dns metadata\n"
"                                    Note: these atoms show up with 'AP -t' as: Atom \"----\" [foo]\n"
"                                          'foo' is actually carried on the 'name' atom\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Setting user-defined 'uuid' tags (all will appear in \"moov.udta.meta\"):\n"
"\n"
"  --information      ,  -i   (str)    Set an information tag on \"moov.udta.meta.uuid=\302�inf\"\n"
"  --url              ,  -u   (URL)    Set a URL tag on \"moov.udta.meta.uuid=\302�url\"\n"
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
"  --freefree         ,  -F   ?(num)?  Remove \"free\" atoms which only act as filler in the file\n"
"                                      ?(num)? - optional integer argument to delete 'free's to desired level\n"
"\n"
"                                      NOTE 1: levels begin at level 1 aka file level.\n"
"                                      NOTE 2: Level 0 (which doesn't exist) deletes level 1 atoms that pre-\n"
"                                              cede 'moov' & don't serve as padding. Typically, such atoms\n"
"                                              are created by libmp4ff or libmp4v2 as a byproduct of tagging.\n"
"                                      NOTE 3: When padding falls below MIN_PAD (typically zero), a default\n"
"                                              amount of padding (typically 2048 bytes) will be added. To\n"
"                                              achieve absolutely 0 bytes 'free' space with --freefree, set\n"
"                                              DEFAULT_PAD to 0.\n"
"  --metaDump         ,  -Q            Dumps out all metadata out to a new file next to original\n"
"                                          (for diagnostic purposes, please remove artwork before sending)\n"
"  --output           ,  -o            Specify the filename of tempfile (voids overWrite)\n"
"  --overWrite        ,  -W            Writes to temp file; deletes original, renames temp to original\n"

"------------------------------------------------------------------------------------------------\n"
" Padding & 'free' atoms:\n"
"\n"
"  A special type of atom called a 'free' atom is used for padding (all 'free' atoms contain NULL space).\n"
"  When changes to occur, these 'free' atom are used. They grows or shink, but the relative locations\n"
"  of certain other atoms (stco/mdat) remain the same. If there is no 'free' space, a full rewrite will occur.\n"
"  The locations of 'free' atom(s) that AP can use as padding must be follow 'moov.udta' & come before 'mdat'.\n"
"  A 'free' preceding 'moov' or following 'mdat' won't be used as padding for example. \n"
"\n"
"  Set the shell variable AP_PADDING with these values, separated by colons to alter padding behavior:\n"
"\n"
"  DEFAULT_PADDING=  -  the amount of padding added if the minimum padding is non-existant in the file\n"
"                       default = 2048\n"
"  MIN_PAD=          -  the minimum padding present before more padding will be added\n"
"                       default = 0\n"
"  MAX_PAD=          -  the maximum allowable padding; excess padding will be eliminated\n"
"                       default = 5000\n"
"\n"
"  If you use --freefree to eliminate 'free' atoms from the file, the DEFAULT_PADDING amount will still be\n"
"  added to any newly written files. Set DEFAULT_PADDING=0 to prevent any 'free' padding added at rewrite.\n"
"  You can set MIN_PAD to be assured that at least that amount of padding will be present - similarly,\n"
"  MAX_PAD limits any excessive amount of padding. All 3 options will in all likelyhood produce a full\n"
"  rewrite of the original file. Another case where a full rewrite will occur is when the original file\n"
"  is not optimized and has 'mdat' preceding 'moov'.\n"
"\n"
#if defined (_MSC_VER)
"  Examples:\n"
"     c:> SET AP_PADDING=\"DEFAULT_PAD=0\"      or    c:> SET AP_PADDING=\"DEFAULT_PAD=3128\"\n"
"     c:> SET AP_PADDING=\"DEFAULT_PAD=5128:MIN_PAD=200:MAX_PAD=6049\"\n"
#else
"  Examples (bash style):\n"
"     $ export AP_PADDING=\"DEFAULT_PAD=0\"      or    $ export AP_PADDING=\"DEFAULT_PAD=3128\"\n"
"     $ export AP_PADDING=\"DEFAULT_PAD=5128:MIN_PAD=200:MAX_PAD=6049\"\n"
#endif
"\n"
"  Note: while AtomicParsley is still in the beta stage, the original file will always remain untouched - \n"
"        unless given the --overWrite flag when if possible, utilizing available padding to update tags\n"
"        will be tried (falling back to a full rewrite if changes are greater than the found padding).\n"
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
;

//detailed options for 3gp branded files
static char* _3gpHelp_text =
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
"example: Atomicparsley /path/to.3gp --3gp-title \"I see London.\" --3gp-title \"Veo Madrid.\" lang=spa \n"
"                                    --3gp-title \"Widze Warsawa.\" lang=pol\n"
"\n"
"----------------------------------------------------------------------------------------------------\n"
"  3GPP text tags can be encoded in either UTF-8 (default input encoding) or UTF-16 (converted from UTF-8)\n"
"  Many 3GPP text tags can be set for a desired language by a 3-letter-lowercase code (default is \"eng\")\n"
"  See   http://www.w3.org/WAI/ER/IG/ert/iso639.htm   to obtain more codes (codes are *not* checked). For\n"
"  tags that support the language attribute (all except year), more than one tag of the same name (3 titles\n"
"  for example) differing in the language code is supported.\n"
"\n"
"  iTunes-style metadata is not supported by the 3GPP TS 26.244 version 6.4.0 Release 6 specification.\n"
"  3GPP tags are set in a different hierarchy: moov.udta (versus iTunes moov.udta.meta.ilst). Other 3rd\n"
"  party utilities may allow setting iTunes-style metadata in 3gp files. When a 3gp file is detected\n"
"  (file extension doesn't matter), only 3gp spec-compliant metadata will be read & written.\n"
"\n"
"  Note1: there are a number of different 'brands' that 3GPP files come marked as. Some will not be \n"
"         supported by AtomicParsley due simply to them being unknown and untested. You can compile your\n"
"         own AtomicParsley to evaluate it by adding the hex code into the source of APar_IdentifyBrand.\n"
"\n"
"  Note2: There are slight accuracy discrepancies in location's fixed point decimals set and retrieved.\n"
"\n"
"  Note3: QuickTime Player can see a limited subset of these tags, but only in 1 language & there seems to\n"
"         be an issue with not all unicode text displaying properly. This is an issue withing QuickTime -\n"
"         the exact same text (in utf8) displays properly in an MPEG-4 file. Some languages can also display\n"
"         more glyphs than others.\n"
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

void ExtractPaddingPrefs(char* env_padding_prefs) {
	pad_prefs.default_padding_size = DEFAULT_PADDING_LENGTH;
	pad_prefs.minimum_required_padding_size = MINIMUM_REQUIRED_PADDING_LENGTH;
	pad_prefs.maximum_present_padding_size = MAXIMUM_REQUIRED_PADDING_LENGTH;
	
	char* env_pad_prefs_ptr = env_padding_prefs;
	
	while (env_pad_prefs_ptr != NULL) {
		env_pad_prefs_ptr = strsep(&env_padding_prefs,":");
		
		if (env_pad_prefs_ptr == NULL) break;
		
		if (memcmp(env_pad_prefs_ptr, "DEFAULT_PAD=", 12) == 0) {
			strsep(&env_pad_prefs_ptr,"=");
			sscanf(env_pad_prefs_ptr, "%u", &pad_prefs.default_padding_size);
		}
		if (memcmp(env_pad_prefs_ptr, "MIN_PAD=", 8) == 0) {
			strsep(&env_pad_prefs_ptr,"=");
			sscanf(env_pad_prefs_ptr, "%u", &pad_prefs.minimum_required_padding_size);
		}
		if (memcmp(env_pad_prefs_ptr, "MAX_PAD=", 8) == 0) {
			strsep(&env_pad_prefs_ptr,"=");
			sscanf(env_pad_prefs_ptr, "%u", &pad_prefs.maximum_present_padding_size);
		}
	}
	//fprintf(stdout, "Def %u; Min %u; Max %u\n", pad_prefs.default_padding_size, pad_prefs.minimum_required_padding_size, pad_prefs.maximum_present_padding_size);
	return;
}

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
				packed_lang = PackLanguage(argv[start_optindargs +i], 5);
			
			} else if ( memcmp(argv[start_optindargs + i], "UTF16", 5) == 0 ) {
				asUTF16 = true;
			}			
		}
	}
	return;
}

//***********************************************

#if defined (_MSC_VER)
int wmain( int argc, wchar_t *arguments[]) {
	uint16_t name_len = wcslen(arguments[0]) +1;
	if (wmemcmp(arguments[0] + (name_len-9), L"-utf8.exe", 9) == 0 || wmemcmp(arguments[0] + (name_len-9), L"-UTF8.exe", 9) == 0) {
		UnicodeOutputStatus = UNIVERSAL_UTF8;
	} else {
		UnicodeOutputStatus = WIN32_UTF16;
	}

	char *argv[350];
	//for native Win32 & full unicode support (well, cli arguments anyway), take whatever 16bit unicode windows gives (utf16le), and convert EVERYTHING
	//that is sends to utf8; use those utf8 strings (mercifully subject to familiar standby's like strlen) to pass around the program like getopt_long
	//to get cli options; convert from utf8 filenames as required for unicode filename support on Windows using wide file functions. Here, EVERYTHING = 350.
	for(int z=0; z < argc; z++) {
		uint32_t wchar_length = wcslen(arguments[z])+1;
		argv[z] = (char *)malloc(sizeof(char)*8*wchar_length );
		memset(argv[z], 0, 8*wchar_length);
		if (UnicodeOutputStatus == WIN32_UTF16) {
			UTF16LEToUTF8((unsigned char*) argv[z], 8*wchar_length, (unsigned char*) arguments[z], wchar_length*2);
		} else {
			strip_bogusUTF16toRawUTF8((unsigned char*) argv[z], 8*wchar_length, arguments[z], wchar_length );
		}
	}
	argv[argc] = NULL;

#else
int main( int argc, char *argv[]) {
#endif
	if (argc == 1) {
		fprintf (stdout,"%s", shortHelp_text); exit(0);
	} else if (argc == 2 && ((strncmp(argv[1],"-v",2) == 0) || (strncmp(argv[1],"-version",2) == 0)) ) {
	
		ShowVersionInfo();
		
		exit(0);
		
	} else if (argc == 2) {
		if ( (strncmp(argv[1],"-help",5) == 0) || (strncmp(argv[1],"--help",6) == 0) || (strncmp(argv[1],"-h",5) == 0 ) ) {
			fprintf(stdout, "%s\n", shortHelp_text); exit(0);
			
		} else if ( (strncmp(argv[1],"--longhelp", 10) == 0) || (strncmp(argv[1],"-longhelp", 9) == 0) || (strncmp(argv[1],"-Lh", 3) == 0) ) {
						if (UnicodeOutputStatus == WIN32_UTF16) {
				int help_len = strlen(longHelp_text)+1;
				wchar_t* Lhelp_text = (wchar_t *)malloc(sizeof(wchar_t)*help_len);
				wmemset(Lhelp_text, 0, help_len);
				UTF8ToUTF16LE((unsigned char*)Lhelp_text, 2*help_len, (unsigned char*)longHelp_text, help_len);
#if defined (_MSC_VER)
				APar_unicode_win32Printout(Lhelp_text);
#endif
				free(Lhelp_text);
			} else {
				fprintf(stdout, "%s", longHelp_text);
			}
			exit(0);
			
		} else if ( (strncmp(argv[1],"--3gp-help", 10) == 0) || (strncmp(argv[1],"-3gp-help", 9) == 0) || (strncmp(argv[1],"--3gp-h", 7) == 0) ) {
			fprintf(stdout, "%s\n", _3gpHelp_text); exit(0);
			
		} else if ( memcmp(argv[1], "--genre-list", 12) == 0 ) {
			ListGenresValues(); exit(0);
			
		} else if ( memcmp(argv[1], "--stik-list", 11) == 0 ) {
			ListStikValues(); exit(0);
		}
	}
	
	if ( argc == 3 && memcmp(argv[2], "--brands", 8) == 0 ) {
			APar_ExtractBrands(argv[1]); exit(0);
		}
	
	total_args = argc;
	char *m4afile = argv[1];
	
	TestFileExistence(m4afile, true);
	xmlInitEndianDetection();
	
	char* padding_options = getenv("AP_PADDING");
	ExtractPaddingPrefs(padding_options);
	
	//it would probably be better to test output_file if provided & if --overWrite was provided.... probably only of use on Windows - and I'm not on it.
	if (strlen(m4afile) + 11 > MAXPATHLEN) {
		fprintf(stderr, "%c %s", '\a', "AtomicParsley error: filename/filepath was too long.\n");
		exit(1);
	}
	
	while (1) {
	static struct option long_options[] = {
		{ "help",						  0,									NULL,						OPT_HELP },
		{ "test",					  	optional_argument,	NULL,						OPT_TEST },
		{ "textdata",         optional_argument,  NULL,           OPT_ShowTextData },
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
		{ "encodingTool",     required_argument,  NULL,           Meta_EncodingTool },
		
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
	
	c = getopt_long(argc, argv, "hTtEe:a:b:c:d:f:g:i:k:l:n:o:pq::u:w:y:z:A:B:C:D:F:G:H:I:J:K:L:MN:QR:S:U:WXV:ZP 0xAB: 0xAC: 0xAD: 0xAE: 0xAF: 0xB0: 0xB1: 0xB2: 0xB3: 0xB4: 0xB5: 0xB6:", long_options, &option_index);
	
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
			tree_display_only = true;
			APar_ScanAtoms(m4afile, true);
			APar_PrintAtomicTree();
			if (argv[optind]) {
				APar_ExtractDetails( openSomeFile(m4afile, true) );
			}
			break;
		}
		
		case OPT_ShowTextData: {
			if (argv[optind]) { //for utilities that write iTunes-style metadata into 3gp branded files
				APar_ExtractBrands(m4afile);
				tree_display_only=true;
				APar_ScanAtoms(m4afile);
				
				openSomeFile(m4afile, true);
				
				if (memcmp(argv[optind], "+", 1) == 0) {
					APar_PrintDataAtoms(m4afile, false, NULL, PRINT_FREE_SPACE + PRINT_PADDING_SPACE + PRINT_USER_DATA_SPACE + PRINT_MEDIA_SPACE );
				} else {
					fprintf(stdout, "  3GPP assets:\n");
					APar_PrintUserDataAssests();
					fprintf(stdout, "---------------------------\n  iTunes-style metadata tags:\n");
					APar_PrintDataAtoms(m4afile, false, NULL, PRINT_FREE_SPACE + PRINT_PADDING_SPACE + PRINT_USER_DATA_SPACE + PRINT_MEDIA_SPACE );
					fprintf(stdout, "---------------------------\n");
				}
				
			} else {
				tree_display_only=true;
				APar_ScanAtoms(m4afile);
				openSomeFile(m4afile, true);
				
				if (metadata_style >= THIRD_GEN_PARTNER) {
					APar_PrintUserDataAssests();
				} else if (metadata_style == ITUNES_STYLE) {
					APar_PrintDataAtoms(m4afile, false, NULL, 0); //false, don't try to extractPix
				}
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
			APar_PrintDataAtoms(m4afile, true, base_path, 0); //exportPix to stripped m4afile path
			openSomeFile(m4afile, false);
			
			free(base_path);
			base_path = NULL;
			break;
		}
		
		case OPT_ExtractPixToPath: {
			APar_ScanAtoms(m4afile);
			openSomeFile(m4afile, true);
			APar_PrintDataAtoms(m4afile, true, optarg, 0); //exportPix to a different path
			openSomeFile(m4afile, false);
			break;
		}
				
		case Meta_artist : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "artist") ) {
				char major_brand[4];
				char4TOuint32(brand, &*major_brand);
				APar_assert(false, 4, &*major_brand);
				break;
			}
			short artistData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�ART.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(artistData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_songtitle : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "title") ) {
				break;
			}
			
			short titleData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�nam.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(titleData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_album : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "album") ) {
				break;
			}
			
			short albumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�alb.data", optarg, AtomFlags_Data_Text);
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
			
			uint16_t pos_in_total = 0;
			uint16_t the_total = 0; 
			if (strrchr(optarg, '/') != NULL) {

				char* duplicate_info = optarg;
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hu", &pos_in_total); //sscanf into a an unsigned char (uint8_t is typedef'ed to a unsigned char by gcc)
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hu", &the_total);
			} else {
				sscanf(optarg, "%hu", &pos_in_total);
			}
			
			short tracknumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.trkn.data", optarg, AtomFlags_Data_Binary);
			//tracknum: [0, 0, 0, 0,   0, 0, 0, pos_in_total, 0, the_total, 0, 0]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, pos_in_total, 16);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, the_total, 16);
			APar_Unified_atom_Put(tracknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			break;
		}
		
		case Meta_disknum : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "disc number") ) {
				break;
			}
			
			uint16_t pos_in_total = 0;
			uint16_t the_total = 0;
			if (strrchr(optarg, '/') != NULL) {
				
				char* duplicate_info = optarg;
				char* item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hu", &pos_in_total); //sscanf into a an unsigned char (uint8_t is typedef'ed to a unsigned char by gcc)
				item_stat = strsep(&duplicate_info,"/");
				sscanf(item_stat, "%hu", &the_total);
			
			} else {
				sscanf(optarg, "%hu", &pos_in_total);
			}
			
			short disknumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.disk.data", optarg, AtomFlags_Data_Binary);
			//disknum: [0, 0, 0, 0,   0, 0, 0, pos_in_total, 0, the_total]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, pos_in_total, 16);
			APar_Unified_atom_Put(disknumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, the_total, 16);
			break;
		}
		
		case Meta_comment : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "comment") ) {
				break;
			}
			
			short commentData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�cmt.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(commentData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_year : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "year") ) {
				break;
			}
			
			short yearData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�day.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(yearData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_lyrics : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "lyrics") ) {
				break;
			}
			
			short lyricsData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�lyr.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(lyricsData_atom, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Meta_composer : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "composer") ) {
				break;
			}
			
			short composerData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�wrt.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(composerData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_copyright : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "copyright") ) {
				break;
			}
			
			short copyrightData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.cprt.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(copyrightData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_grouping : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "grouping") ) {
				break;
			}
			
			short groupingData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�grp.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(groupingData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_compilation : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "compilation") ) {
				break;
			}
			
			if (strncmp(optarg, "false", 5) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.cpil.data", VERSIONED_ATOM, 0);
			} else {
				//compilation: [0, 0, 0, 0,   boolean_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short compilationData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.cpil.data", optarg, AtomFlags_Data_UInt);
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
				APar_RemoveAtom("moov.udta.meta.ilst.tmpo.data", VERSIONED_ATOM, 0);
			} else {
				uint16_t bpm_value = 0;
				sscanf(optarg, "%hu", &bpm_value );
				//bpm is [0, 0, 0, 0,   0, bpm_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short bpmData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tmpo.data", optarg, AtomFlags_Data_UInt);
				APar_Unified_atom_Put(bpmData_atom, NULL, UTF8_iTunesStyle_256byteLimited, bpm_value, 16);
			}
			break;
		}
		
		case Meta_advisory : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "content advisory") ) {
				break;
			}
			
			if (strncmp(optarg, "remove", 6) == 0 || strlen(optarg) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.rtng.data", VERSIONED_ATOM, 0);
			} else {
				uint8_t rating_value = 0;
				if (strncmp(optarg, "clean", 5) == 0) {
					rating_value = 2; //only \02 is clean
				} else if (strncmp(optarg, "explicit", 8) == 0) {
					rating_value = 4; //most non \00, \02 numbers are allowed
				}
				//rating is [0, 0, 0, 0,   rating_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short advisoryData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.rtng.data", optarg, AtomFlags_Data_UInt);
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
				APar_RemoveAtom("moov.udta.meta.ilst.stik.data", VERSIONED_ATOM, 0);
			} else {
				uint8_t stik_value = 0;
				
				if (memcmp(optarg, "value=", 6) == 0) {
					char* stik_val_str_ptr = optarg;
					strsep(&stik_val_str_ptr,"=");
					sscanf(stik_val_str_ptr, "%hhu", &stik_value);
				} else {
					stiks* return_stik = MatchStikString(optarg);
					if (return_stik != NULL) {
						stik_value = return_stik->stik_number;
					}
				}
				//stik is [0, 0, 0, 0,   stik_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short stikData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.stik.data", optarg, AtomFlags_Data_UInt);
				APar_Unified_atom_Put(stikData_atom, NULL, UTF8_iTunesStyle_256byteLimited, stik_value, 8);
			}
			break;
		}
		
		case Meta_EncodingTool : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "encoding tool") ) {
				break;
			}
			
			short encodingtoolData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.�too.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(encodingtoolData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_description : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "description") ) {
				break;
			}
			
			short descriptionData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.desc.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(descriptionData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_Network : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Network") ) {
				break;
			}
			
			short tvnetworkData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvnn.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(tvnetworkData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_ShowName : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Show name") ) {
				break;
			}
			
			short tvshownameData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvsh.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(tvshownameData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_Episode : { //if the show "ABC Lost 209", its "209"
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Episode string") ) {
				break;
			}
			
			short tvepisodeData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tven.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(tvepisodeData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_TV_SeasonNumber : { //if the show "ABC Lost 209", its 2; integer 2 not char "2"
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Season") ) {
				break;
			}
			
			uint16_t data_value = 0;
			sscanf(optarg, "%hu", &data_value );
			
			short tvseasonData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tvsn.data", optarg, AtomFlags_Data_UInt);
			//season is [0, 0, 0, 0,   0, 0, 0, data_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tvseasonData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tvseasonData_atom, NULL, UTF8_iTunesStyle_256byteLimited, data_value, 16);
			break;
		}
		
		case Meta_TV_EpisodeNumber : { //if the show "ABC Lost 209", its 9; integer 9 (0x09) not char "9"(0x39)
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "TV Episode number") ) {
				break;
			}
			
			uint16_t data_value = 0;
			sscanf(optarg, "%hu", &data_value );
			
			short tvepisodenumData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.tves.data", optarg, AtomFlags_Data_UInt);
			//episodenumber is [0, 0, 0, 0,   0, 0, 0, data_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
			APar_Unified_atom_Put(tvepisodenumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 0, 16);
			APar_Unified_atom_Put(tvepisodenumData_atom, NULL, UTF8_iTunesStyle_256byteLimited, data_value, 16);
			break;
		}
		
		case Meta_album_artist : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "album artist") ) {
				break;
			}
			
			short albumartistData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.aART.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(albumartistData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_podcastFlag : {
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast flag") ) {
				break;
			}
			
			if (strncmp(optarg, "false", 5) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.pcst.data", VERSIONED_ATOM, 0);
			} else {
				//podcastflag: [0, 0, 0, 0,   boolean_value]; BUT that first uint32_t is already accounted for in APar_MetaData_atom_Init
				short podcastFlagData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.pcst.data", optarg, AtomFlags_Data_UInt);
				APar_Unified_atom_Put(podcastFlagData_atom, NULL, UTF8_iTunesStyle_256byteLimited, 1, 8); //a hard coded uint8_t of: 1 denotes podcast flag
			}
			
			break;
		}
		
		case Meta_keyword : {    //TODO to the end of iTunes-style metadata & uuid atoms
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "keyword") ) {
				break;
			}
			
			short keywordData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.keyw.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(keywordData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_category : { // see http://www.apple.com/itunes/podcasts/techspecs.html for available categories
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "category") ) {
				break;
			}
			
			short categoryData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.catg.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(categoryData_atom, optarg, UTF8_iTunesStyle_256byteLimited, 0, 0);
			break;
		}
		
		case Meta_podcast_URL : { // usually a read-only value, but useful for getting videos into the 'podcast' menu
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast URL") ) {
				break;
			}
			
			short podcasturlData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.purl.data", optarg, AtomFlags_Data_Binary);
			APar_Unified_atom_Put(podcasturlData_atom, optarg, UTF8_iTunesStyle_Binary, 0, 0);
			break;
		}
		
		case Meta_podcast_GUID : { // Global Unique IDentifier; it is *highly* doubtful that this would be useful...
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "podcast GUID") ) {
				break;
			}
			
			short globalidData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.egid.data", optarg, AtomFlags_Data_Binary);
			APar_Unified_atom_Put(globalidData_atom, optarg, UTF8_iTunesStyle_Binary, 0, 0);
			break;
		}
		
		case Meta_PurchaseDate : { // might be useful to *remove* this, but adding it... although it could function like id3v2 tdtg...
			APar_ScanAtoms(m4afile);
			if ( !APar_assert(metadata_style == ITUNES_STYLE, 1, "purchase date") ) {
				break;
			}
			char* purd_time;
			bool free_memory = false;
			if (optarg != NULL) {
				if (strncmp(optarg, "timestamp", 9) == 0) {
					purd_time = (char *)malloc(sizeof(char)*255);
					free_memory = true;
					APar_StandardTime(purd_time);
				} else {
					purd_time = optarg;
				}
			} else {
				purd_time = optarg;
			}
			
			short globalidData_atom = APar_MetaData_atom_Init("moov.udta.meta.ilst.purd.data", optarg, AtomFlags_Data_Text);
			APar_Unified_atom_Put(globalidData_atom, purd_time, UTF8_iTunesStyle_256byteLimited, 0, 0);
			if (free_memory) {
				free(purd_time);
				purd_time = NULL;
			}
			break;
		}
		
		//uuid atoms
		
		case Meta_StandardDate : {
			APar_ScanAtoms(m4afile);
			char* formed_time = (char *)malloc(sizeof(char)*110);
			APar_StandardTime(formed_time);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "tdtg", AtomFlags_Data_Text, formed_time, false); //filed apple bug report
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "tdtg", AtomFlags_Data_Text, formed_time, false);
			short tdtgUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "tdtg", AtomFlags_Data_Text, formed_time, false);
			APar_Unified_atom_Put(tdtgUUID, formed_time, UTF8_iTunesStyle_Unlimited, 0, 0);
			free(formed_time);
			break;
		}
		
		case Meta_URL : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "�url", AtomFlags_Data_Text, optarg, false); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "�url", AtomFlags_Data_Text, optarg, false);
			short urlUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "�url", AtomFlags_Data_Text, optarg, false);
			APar_Unified_atom_Put(urlUUID, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Meta_Information : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "�inf", AtomFlags_Data_Text, optarg, false); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "�inf", AtomFlags_Data_Text, optarg, false);
			short infoUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", "�inf", AtomFlags_Data_Text, optarg, false);
			APar_Unified_atom_Put(infoUUID, optarg, UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}

		case Meta_uuid : {
			APar_ScanAtoms(m4afile);
			
			//uuid atoms are handled differently, because they are user/private-extension atoms
			//a standard path is provided in the "path.form", however a uuid atom has a name of 'uuid' in the vein of the traditional atom name
			//PLUS a uuid extended 4byte name (1st argument), and then the number of the datatype (0,1,21) & the actual data  (3rd argument)
			
			// --meta-uuid "�foo" 1 'http://www.url.org'
			
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", optarg, AtomFlags_Data_Text, argv[optind +1], true); //apple iTunes bug; not allowed
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", optarg, AtomFlags_Data_Text, argv[optind +1], true);
			
			short genericUUID = APar_uuid_atom_Init("moov.udta.meta.uuid=%s", optarg, AtomFlags_Data_Text, argv[optind +1], true);
			APar_Unified_atom_Put(genericUUID, argv[optind +1], UTF8_iTunesStyle_Unlimited, 0, 0);
			break;
		}
		
		case Manual_atom_removal : {
			APar_ScanAtoms(m4afile);
			
			char* compliant_name = (char*)malloc(sizeof(char)* strlen(optarg) +1);
			memset(compliant_name, 0, strlen(optarg) +1);
			UTF8Toisolat1((unsigned char*)compliant_name, strlen(optarg), (unsigned char*)optarg, strlen(optarg) );
			
			if (strstr(optarg, "uuid=") != NULL) {
				APar_RemoveAtom(compliant_name, EXTENDED_ATOM, 0);
				
			} else if (memcmp(compliant_name + (strlen(compliant_name) - 4), "data", 4) == 0) {
				APar_RemoveAtom(compliant_name, VERSIONED_ATOM, 0);
				
			} else {
				size_t string_len = strlen(compliant_name);
				//reverseDNS atom path
				if (strstr(optarg, ":[") != NULL && memcmp(compliant_name + string_len-1, "]", 1) == 0 ) {
					APar_RemoveAtom(compliant_name, VERSIONED_ATOM, 0);
				
				//packed language asset
				} else if (memcmp(compliant_name + string_len - 9, ":lang=", 6) == 0 ) {
					uint16_t packed_lang = PackLanguage(compliant_name + string_len - 3, 0);
					memset(compliant_name + string_len - 9, 0, 1);
					APar_RemoveAtom(compliant_name, PACKED_LANG_ATOM, packed_lang);
					
				} else {
					APar_RemoveAtom(compliant_name, UNKNOWN_ATOM, 0);
				}
			}
			free(compliant_name);
			compliant_name = NULL;
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
			
			short title_3GP_atom = APar_UserData_atom_Init("moov.udta.titl", optarg, packed_lang);
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
			
			short author_3GP_atom = APar_UserData_atom_Init("moov.udta.auth", optarg, packed_lang);
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
			
			short performer_3GP_atom = APar_UserData_atom_Init("moov.udta.perf", optarg, packed_lang);
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
			
			short genre_3GP_atom = APar_UserData_atom_Init("moov.udta.gnre", optarg, packed_lang);
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
			
			short description_3GP_atom = APar_UserData_atom_Init("moov.udta.dscp", optarg, packed_lang);
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
			
			short copyright_3GP_atom = APar_UserData_atom_Init("moov.udta.cprt", optarg, packed_lang);
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
			
			short album_3GP_atom = APar_UserData_atom_Init("moov.udta.albm", optarg, packed_lang);
			APar_Unified_atom_Put(album_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
			
			//cygle through the remaining independant arguments (before the next --cli_flag) and figure out if any are useful to us; already have lang & utf16
			for (int i= 0; i < 3; i++) { //3 possible arguments for this tag (the first - which doesn't count - is the data for the tag itself)
				if ( argv[optind + i] && optind + i <= total_args) {
					if ( memcmp(argv[optind + i], "track=", 6) == 0 ) {
						char* track_num = argv[optind + i];
						strsep(&track_num,"=");
						sscanf(track_num, "%hhu", &tracknum);
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
			
			short rec_year_3GP_atom = APar_UserData_atom_Init("moov.udta.yrrc", optarg, 0);
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
						char* entity = argv[optind + i];
						strsep(&entity,"=");
						memcpy(&rating_entity, entity, 4);
					}
					if ( memcmp(argv[optind + i], "criteria=", 9) == 0 ) {
						char* criteria = argv[optind + i];
						strsep(&criteria,"=");
						memcpy(&rating_criteria, criteria, 4);
					}
				}
			}
			short rating_3GP_atom = APar_UserData_atom_Init("moov.udta.rtng", optarg, packed_lang);
			
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
						char* cls_entity = argv[optind + i];
						strsep(&cls_entity, "=");
						memcpy(&classification_entity, cls_entity, 4);
					}
					if ( memcmp(argv[optind + i], "index=", 6) == 0 ) {
						char* cls_idx = argv[optind + i];
						strsep(&cls_idx, "=");
						sscanf(cls_idx, "%hu", &classification_index);
					}
				}
			}
			short classification_3GP_atom = APar_UserData_atom_Init("moov.udta.clsf", optarg, packed_lang);
			
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
				char* arg_keywords = optarg;
				char* keywords_globbed = strsep(&arg_keywords,"="); //separate out 'keyword='
				keywords_globbed = strsep(&arg_keywords,"="); //this is what we want to work on: just the keywords
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

				short keyword_3GP_atom = APar_UserData_atom_Init("moov.udta.kywd", keyword_strlen ? "temporary" : "", packed_lang); //just a "temporary" valid string to satisfy a test there
				if (keyword_strlen > 0) {
					APar_Unified_atom_Put(keyword_3GP_atom, NULL, UTF8_3GP_Style, (uint32_t)packed_lang, 16);
					APar_Unified_atom_Put(keyword_3GP_atom, NULL, UTF8_3GP_Style, keyword_count, 8);
					char* formed_keyword_struct = (char*)malloc(sizeof(char)* set_UTF16_text ? keyword_strlen * 4 : keyword_strlen * 2); // *4 should carry utf16's BOM & TERM
					memset(formed_keyword_struct, 0, set_UTF16_text ? keyword_strlen * 4 : keyword_strlen * 2 );
					uint32_t keyword_struct_bytes = APar_3GP_Keyword_atom_Format(keywords_globbed, keyword_count, set_UTF16_text, formed_keyword_struct);
					APar_atom_Binary_Put(keyword_3GP_atom, formed_keyword_struct, keyword_struct_bytes, 3);
					free(formed_keyword_struct);
					formed_keyword_struct = NULL;
				}
			} else {
				APar_UserData_atom_Init("moov.udta.kywd", "", packed_lang);
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
						char* _long = argv[optind + i];
						strsep(&_long,"=");
						sscanf(_long, "%lf", &longitude);
						if (_long[strlen(_long)-1] == 'W') {
							longitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "latitude=", 9) == 0 ) {
						char* _latt = argv[optind + i];
						strsep(&_latt,"=");
						sscanf(_latt, "%lf", &latitude);
						if (_latt[strlen(_latt)-1] == 'S') {
							latitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "altitude=", 9) == 0 ) {
						char* _alti = argv[optind + i];
						strsep(&_alti,"=");
						sscanf(_alti, "%lf", &altitude);
						if (_alti[strlen(_alti)-1] == 'B') {
							altitude*=-1;
						}
					}
					if ( memcmp(argv[optind + i], "role=", 5) == 0 ) {
						char* _role = argv[optind + i];
						strsep(&_role,"=");
						if (strncmp(_role, "shooting location", 17) == 0 || strncmp(_role, "shooting", 8) == 0) {
							role = 0;
						} else if (strncmp(_role, "real location", 13) == 0 || strncmp(_role, "real", 4) == 0) {
							role = 1;
						} else if (strncmp(_role, "fictional location", 18) == 0 || strncmp(_role, "fictional", 9) == 0) {
							role = 2;
						}
					}
					if ( memcmp(argv[optind + i], "body=", 5) == 0 ) {
						char* _astrobody = argv[optind + i];
						strsep(&_astrobody,"=");
						astronomical_body = _astrobody;
					}
					if ( memcmp(argv[optind + i], "notes=", 6) == 0 ) {
						char* _add_notes = argv[optind + i];
						strsep(&_add_notes,"=");
						additional_notes = _add_notes;
					}
				}
			}
			
			//fprintf(stdout, "long, lat, alt = %lf %lf %lf\n", longitude, latitude, altitude);
			
			if (longitude < -180.0 || longitude > 180.0 || latitude < -90.0 || latitude > 90.0) {
				fprintf(stdout, "AtomicParsley warning: longitude or latitude was invalid; skipping setting location\n");
			} else {
			
				short location_3GP_atom = APar_UserData_atom_Init("moov.udta.loci", optarg, packed_lang);
				APar_Unified_atom_Put(location_3GP_atom, optarg, (set_UTF16_text ? UTF16_3GP_Style : UTF8_3GP_Style), (uint32_t)packed_lang, 16);
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, (uint32_t)role, 8);
				
				APar_Unified_atom_Put(location_3GP_atom, NULL, false, float_to_16x16bit_fixed_point(longitude), 32);
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
			APar_RemoveAtom("moov.udta.meta.ilst", SIMPLE_ATOM, 0);
			
			break;
		}
		
		case UserData_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta", SIMPLE_ATOM, 0);
			
			break;
		}
		
		case foobar_purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.tags", SIMPLE_ATOM, 0);
			
			break;
		}
		
		case Opt_FreeFree : {
			APar_ScanAtoms(m4afile);
			int free_level = -1;
			if (argv[optind]) {
				sscanf(argv[optind], "%i", &free_level);
			}
			APar_freefree(free_level);
			
			break;
		}
		
		case Opt_Keep_mdat_pos : {
			move_moov_atom = false;
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
			
			APar_FreeMemory();
			#if defined (_MSC_VER)
				for(int zz=0; zz < argc; zz++) {
					if (argv[zz] > 0) {
						free(argv[zz]);
						argv[zz] = NULL;
					}
				}
			#endif
			exit(0); //das right, this is a flag that doesn't get used with other flags.
		}
		
		case OPT_OutputFile : {
			output_file = optarg;
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
	} else {
		if (m4afile != NULL && argc > 3 && !tree_display_only) {
			fprintf(stdout, "No changes.\n");
		}
	}
	APar_FreeMemory();
#if defined (_MSC_VER)
	for(int zz=0; zz < argc; zz++) {
		if (argv[zz] > 0) {
			free(argv[zz]);
			argv[zz] = NULL;
		}
	}
#endif
	return 0;
}
