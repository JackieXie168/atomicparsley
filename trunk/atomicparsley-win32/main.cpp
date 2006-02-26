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


#include "getopt.h"
#include "AtomicParsley.h"

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
#define Meta_dump                'Q'
#define Opt_FreeFree             'F'
#define Opt_Keep_mdat_pos        'M'
#define OPT_OutputFile           'o'

#define OPT_OverWrite            'W'

//to pass limited text to APar_AddMetadataInfo for a non-text atom:
#define indeterminate false


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

static void kill_signal ( int sig );

static void kill_signal (int sig) {
    exit(0);
}

static const char* longHelp_text =
"AtomicParsley longhelp.\n"
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
"  --artist           ,  -a   (str)    Set the artist tag: \"moov.udta.meta.ilst.©ART.data\"\n"
"  --title            ,  -s   (str)    Set the title tag: \"moov.udta.meta.ilst.©nam.data\"\n"
"  --album            ,  -b   (str)    Set the album tag: \"moov.udta.meta.ilst.©alb.data\"\n"
"  --genre            ,  -g   (str)    Set the genre tag: \"©gen\" (custom) or \"gnre\" (standard).\n"
"  --tracknum         ,  -k   (num)|(num/tot)  Set the track number (or track number & total tracks).\n"
"  --disk             ,  -d   (num)|(num/tot)  Set the disk number (or disk number & total disks).\n"
"  --comment          ,  -c   (str)    Set the comment tag: \"moov.udta.meta.ilst.©cmt.data\"\n"
"  --year             ,  -y   (num)    Set the year tag: \"moov.udta.meta.ilst.©day.data\"\n"
"  --lyrics           ,  -l   (str)    Set the lyrics tag: \"moov.udta.meta.ilst.©lyr.data\"\n"
"  --composer         ,  -w   (str)    Set the composer tag: \"moov.udta.meta.ilst.©wrt.data\"\n"
"  --copyright        ,  -x   (str)    Set the copyright tag: \"moov.udta.meta.ilst.cprt.data\"\n"
"  --grouping         ,  -G   (str)    Set the grouping tag: \"moov.udta.meta.ilst.©grp.data\"\n"
"  --artwork          ,  -A   (/path)  Set (multiple) artwork (jpeg or png) tag: \"covr.data\"\n"
"  --bpm              ,  -B   (num)    Set the tempo/bpm tag: \"moov.udta.meta.ilst.tmpo.data\"\n"
"  --albumArtist      ,  -A   (str)    Set the album artist tag: \"moov.udta.meta.ilst.aART.data\"\n"
"  --compilation      ,  -C   (bool)   Sets the \"cpil\" atom (true or false to delete the atom)\n"
"  --advisory         ,  -y   (1of3)   Sets the iTunes lyrics advisory ('remove', 'clean', 'explicit') \n"
"  --stik             ,  -S   (1of6)   Sets the iTunes \"stik\" atom (options available below) \n"
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
"  --metaEnema        ,  -P            Douches away every atom under \"moov.udta.meta.ilst\" \n"
"\n"
" To delete a single atom, set the tag to null (except artwork):\n"
"  --artist \"\" --lyrics \"\"\n"
"  --artwork REMOVE_ALL \n"
"------------------------------------------------------------------------------------------------\n"
" Setting user-defined 'uuid' tags (all will appear in \"moov.udta.meta\"):\n"
"\n"
"  --information      ,  -i   (str)    Set an information tag on \"moov.udta.meta.uuid=©inf\"\n"
"  --url              ,  -u   (URL)    Set a URL tag on \"moov.udta.meta.uuid=©url\"\n"
"  --tagtime          ,  -Z            Set the Coordinated Univeral Time of tagging on \"uuid=tdtg\"\n"
"\n"
"  --meta-uuid        ,  -z   (args)   Define & set your own uuid=atom with text data:\n"
"                                        format is 4char_atom_name, 1 or \"text\" & the string to set\n"
"Example: \n"
"  --meta-uuid \"tagr\" 1 'Johnny Appleseed' --meta-uuid \"\302\251sft\" 1 'OpenShiiva encoded.' \n"
"------------------------------------------------------------------------------------------------\n"
" File-level options:\n"
"\n"
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

//***********************************************

int main( int argc, char *argv[])
{
	if (argc == 1) {
		fprintf (stdout,"%s", longHelp_text); exit(0);
	} else if (argc == 2 && ((strncmp(argv[1],"-v",2) == 0) || (strncmp(argv[1],"-version",2) == 0)) ) {
	
		ShowVersionInfo();
		
		exit(0);
		
	} else if (argc == 2 && ( (strncmp(argv[1],"-help",5) == 0) || (strncmp(argv[1],"--help",6) == 0) || (strncmp(argv[1],"-h",5) == 0)) ) {
		fprintf (stdout,"%s", longHelp_text); exit(0);
	}
	
	char *m4afile = argv[1];
	
	TestFileExistence(m4afile, true);
	
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
		{ "metaDump",         0,                  NULL,						Meta_dump },
		{ "output",           required_argument,  NULL,						OPT_OutputFile },
		{ "overWrite",        0,                  NULL,						OPT_OverWrite },
		
		{ 0, 0, 0, 0 }
	};
		
	int c = -1;
	int option_index = 0; 
	
	c = getopt_long(argc, argv, "hTtEe:a:c:d:f:g:i:l:n:o:pq::u:w:y:z:G:k:A:B:C:D:F:H:I:J:K:L:MN:QS:U:WV:ZP", long_options, &option_index);
	
	if (c == -1) {
        optind++;   // Line added for getopt.h merge :: BStory
		if (argc < 3 && argc >= 2) {
			APar_ScanAtoms(m4afile, true);
			APar_PrintAtomicTree();
		}
        if (argc <= optind)   // Line added for getopt.h merge :: BStory
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
			APar_PrintDataAtoms(m4afile, false, NULL); //false, don't try to extractPix
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
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©ART.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_songtitle : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©nam.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_album : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©alb.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_genre : {
			APar_ScanAtoms(m4afile);
			APar_AddGenreInfo(m4afile, optarg);
			
			break;
		}
				
		case Meta_tracknum : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.trkn.data", AtomicDataClass_UInteger, optarg, indeterminate);
			
			break;
		}
		
		case Meta_disknum : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.disk.data", AtomicDataClass_UInteger, optarg, indeterminate);
			
			break;
		}
		
		case Meta_comment : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©cmt.data", AtomicDataClass_Text, optarg, true); //yuppers - limit on comment; cope & deal
			
			break;
		}
		
		case Meta_year : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©day.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_lyrics : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©lyr.data", AtomicDataClass_Text, optarg, false); //no limit on lyrics
			
			break;
		}
		
		case Meta_composer : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©wrt.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		//if copyright is switched to ©cpy tag, then QTplayer can see the copyright, but iTunes no longer will.
		case Meta_copyright : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.cprt.data", AtomicDataClass_Text, optarg, false); //I imagine the text limit would be there, but it streches off the panel, so I can't tell
			
			break;
		}
		
		case Meta_grouping : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©grp.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_compilation : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "false", 5) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.cpil", false, false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.cpil.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			}
			break;
		}
		
		case Meta_BPM : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "0", 1) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.tmpo", false, false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tmpo.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			}
			
			break;
		}
		
		case Meta_advisory : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "remove", 6) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.rtng", false, false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.rtng.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			}
			break;
		}
		
		case Meta_artwork : { //handled differently: there can be multiple "moov.udta.meta.ilst.covr.data" atoms
        	char* env_PicOptions = getenv("PIC_OPTIONS");
			APar_ScanAtoms(m4afile);
			APar_AddMetadataArtwork(m4afile, optarg, env_PicOptions);
        	break;
		}
				
		case Meta_stik : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.stik.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			
			break;
		}
		
		case Meta_description : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.desc.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_TV_Network : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvnn.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_TV_ShowName : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvsh.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_TV_Episode : { //if the show "ABC Lost 209", its "209"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tven.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_TV_SeasonNumber : { //if the show "ABC Lost 209", its 2; integer 2 not char "2"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvsn.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			
			break;
		}
		
		case Meta_TV_EpisodeNumber : { //if the show "ABC Lost 209", its 2; integer 9 not char "9"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tves.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			
			break;
		}
		
		case Meta_album_artist : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.aART.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_podcastFlag : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.pcst.data", AtomicDataClass_UInt8_Binary, optarg, indeterminate);
			
			break;
		}
		
		case Meta_keyword : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.keyw.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_category : { // see http://www.apple.com/itunes/podcasts/techspecs.html for available categories
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.catg.data", AtomicDataClass_Text, optarg, true);
			
			break;
		}
		
		case Meta_podcast_URL : { // usually a read-only value, but usefult for getting videos into the 'podcast' menu
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.purl.data", AtomicDataClass_UInteger, optarg, indeterminate);
			
			break;
		}
		
		case Meta_podcast_GUID : { // Global Unique IDentifier; it is *highly* doubtful that this would be useful...
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.egid.data", AtomicDataClass_UInteger, optarg, indeterminate);
			
			break;
		}
		
		case Meta_PurchaseDate : { // might be useful to *remove* this, but adding it... although it could function like id3v2 tdtg...
			char* purd_time = (char *)malloc(sizeof(char)*255);
			APar_ScanAtoms(m4afile);
			if (optarg != NULL) {
				if (strncmp(optarg, "timestamp", 9) == 0) {
					APar_StandardTime(purd_time);
				} else {
					purd_time = strdup(optarg);
				}
			} else {
				purd_time = strdup(optarg);
			}
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.purd.data", AtomicDataClass_Text, purd_time, true);
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
			APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "tdtg", AtomicDataClass_Text, formed_time, false);
			free(formed_time);
			break;
		}
		
		case Meta_URL : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "©url", AtomicDataClass_Text, optarg, false); //apple iTunes bug; not allowed
			APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "©url", AtomicDataClass_Text, optarg, false);
			break;
		}
		
		case Meta_Information : {
			APar_ScanAtoms(m4afile);
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", "©inf", AtomicDataClass_Text, optarg, false); //apple iTunes bug; not allowed
			APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", "©inf", AtomicDataClass_Text, optarg, false);
			break;
		}

		case Meta_uuid : {
			APar_ScanAtoms(m4afile);
			
			//uuid atoms are handled differently, because they are user/private-extension atoms
			//a standard path is provided in the "path.form", however a uuid atom has a name of 'uuid' in the vein of the traditional atom name
			//PLUS a uuid extended 4byte name (1st argument), and then the number of the datatype (0,1,21) & the actual data  (3rd argument)
			
			// --meta-uuid "©foo" 1 'http://www.url.org'
			
			//APar_Add_uuid_atom(m4afile, "moov.udta.meta.ilst.uuid=%s", optarg, AtomicDataClass_Text, argv[optind +1], true); //apple iTunes bug; not allowed
			APar_Add_uuid_atom(m4afile, "moov.udta.meta.uuid=%s", optarg, AtomicDataClass_Text, argv[optind +1], true);
			
			break;
		}
		
		case Metadata_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.meta.ilst", true, false);
			
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
