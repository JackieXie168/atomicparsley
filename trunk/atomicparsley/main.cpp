//==================================================================//
/*
    AtomicParsley - main.cpp

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
                                                                   */
//==================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include "AtomicParsley.h"

// define one-letter cli options for
#define OPT_HELP                 'h'
#define OPT_TEST		             'T'
#define OPT_ShowTextData		     't'
#define OPT_ExtractPix           'E'
#define OPT_ExtractPixToPath		 'e'
#define META_add                 'm'
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
#define Meta_StandardDate        'Z'
#define Meta_advisory            'V'
#define Metadata_Purge           'P'
#define Meta_URL                 'u'
#define Meta_Information         'i'
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

#define OPT_WriteBack            'O'


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

/*
Apparently, iTunes places comment as "©cmt" (UserDataID) as opposed to "cmmt" (MetaDataConstant)
And yet iTunes (or iTMS) puts copyright as "cprt" instead of "©cpy"

so I believe that I can use ©ed1 to store UTC tagging time (id3v2.4 = TDTG); and ©url can carry WOAR; ©inf can be WXXX
as ©ope is TOPE.... so.... mood would be ©moo (TMOO)
*/

bool modified_atoms = false;

static void kill_signal ( int sig );

static void kill_signal (int sig) {
    exit(0);
}

static const char* longHelp_text =
"AtomicParsley longhelp.\n"
"Usage: AtomicParsley [m4aFILE]... [OPTION(s)]...[ARGUMENT(s)]...\n"
"\n"
"example: AtomicParsley /path/to.m4a --E \n"
"example: Atomicparsley /path/to.m4a --artist \"Me\" --artwork /path/art.jpg\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Atom Tree\n"
"\n"
"  --test             ,  -T      Tests header of an m4a to see if its a usable m4a file.\n"
"                               Also prints out the hierarchical atom tree.\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Atom contents (printing on screen & extracting artwork(s) to files)\n"
"\n"
"  --textdata         ,  -t      prints contents of user data text items out (inc. # of any pics).\n"
"\n"
"  Extract any pictures in user data \"covr\" atoms to separate files. \n"
"  --extractPix       ,  -E                     Extract to same folder (basename derived from file).\n"
"  --extractPixToPath ,  -e  (/path/basename)   Extract to specific path (numbers affixed to name).\n"
"\n"
"------------------------------------------------------------------------------------------------\n"
" Setting Tags:\n"
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
"  --tagtime          ,  -Z            Set the Coordinated Univeral Time of tagging on \"©ed1\"*\n"
"  --information      ,  -i            Set an information tag on \"moov.udta.meta.ilst.©inf.data\"*\n"
"  --url              ,  -u            Set a URL tag on \"moov.udta.meta.ilst.©url.data\"*\n"
"  --stik             ,  -S   (1of4)   Sets the iTunes \"stik\" atom (options available below) \n"
"                                           (\"Movie\", \"Whacked Bookmark\", \"Music Video\", \"TV Show\") \n"
"  --description      ,  -p   (str)    Sets the description - used in TV shows\n"
"  --TVNetwork        ,  -n   (str)    Sets the TV Network name on the \"tvnn\" atom\n"
"  --TVShowName       ,  -H   (str)    Sets the TV Show name on the \"tvsh\" atom\n"
"  --TVEpisode        ,  -I   (str)    Sets the TV Episode on \"tven\":\"209\", but its a string: \"209 Part 1\"\n"
"  --TVSeasonNum      ,  -U   (num)    Sets the TV Season number on the \"tvsn\" atom\n"
"  --TVEpisodeNum     ,  -N   (num)    Sets the TV Episode number on the \"tves\" atom\n"

"  --podcastFlag      ,  -f   (bool)   Sets the podcast flag (values are \"true\" or \"false\")\n"
"  --category         ,  -q   (str)    Sets the podcast category; typically a duplicate of its genre\n"
"  --keyword          ,  -q   (str)    Sets the podcast keyword; invisible to MacOSX Spotlight\n"
"                                     *Denotes utterly non-standard behavior; invisible to iTunes\n"
"\n"
"  --writeBack        ,  -O            If given, writes the file back into original file; deletes temp\n"
"\n"
"  --metaEnema        ,  -P            Douches away every atom under \"moov.udta.meta.ilst\" \n"
"\n"
" To delete a single atom, set the tag to null (except artwork):\n"
"  --artist \"\" --lyrics \"\"\n"
"  --artwork REMOVE_ALL \n"
"------------------------------------------------------------------------------------------------\n"

#if defined (__ppc__) || defined (__ppc64__)
"                   Environmental Variables (affecting picture placement)\n"
"\n"
" export these variables in your shell to set these flags; preferences are separated by colons (:)\n"
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
			split_here = i-1;
			break;
		}
	}
	for (int iout=0; iout <= split_here; iout++) {
		basepath[iout] = filepath[iout];
	}
	return;
}

//***********************************************

int main( int argc, char *argv[])
{
	if (argc == 1) {
		fprintf (stdout,"%s", longHelp_text); exit(0);
	} else if (argc == 2 && ((strncmp(argv[1],"-v",2) == 0) || (strncmp(argv[1],"-version",2) == 0)) ) {
		fprintf(stdout, "%s version: %s\n", argv[0], AtomicParsley_version);
		exit(0);
	} else if (argc == 2 && ( (strncmp(argv[1],"-help",5) == 0) || (strncmp(argv[1],"--help",6) == 0) || (strncmp(argv[1],"-h",5) == 0)) ) {
		fprintf (stdout,"%s", longHelp_text); exit(0);
	}
	
	char *m4afile = argv[1];
	TestFileExistence(m4afile, true);
	
	while (1) {
	static struct option long_options[] = {
		{ "help",						  0,									NULL,						OPT_HELP },
		{ "test",					  	0,									NULL,						OPT_TEST },
		{ "textdata",         0,                  NULL,           OPT_ShowTextData },
		{ "extractPix",				0,									NULL,           OPT_ExtractPix },
		{ "extractPixToPath", required_argument,	NULL,				    OPT_ExtractPixToPath },
		{ "addMetaInfo",      required_argument,  NULL,						META_add },
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
		{ "tagtime",          0,                  NULL,						Meta_StandardDate },
		{ "metaEnema",        0,                  NULL,						Metadata_Purge },
		{ "writeBack",        0,                  NULL,						OPT_WriteBack },
		{ "information",      required_argument,  NULL,           Meta_Information },
		{ "url",              required_argument,  NULL,           Meta_URL },
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
		
		{ 0, 0, 0, 0 }
	};
		
	int c = -1;
	int option_index = 0; 
	
	//c = getopt_long_only(argc, argv, "hTtEe:m:a:d:g:c:i:l:n:p:u:w:y:G:k:A:B:C:H:I:N:S:U:V:ZP", long_options, &option_index);
	c = getopt_long(argc, argv, "hTtEe:m:a:d:f:g:c:i:l:n:pq::u:w:y:G:k:A:B:C:H:I:K:N:S:U:V:ZP", long_options, &option_index);
	
	if (c == -1) {
		if (argc < 3 && argc > 2) {
			APar_ScanAtoms(m4afile);
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
			APar_ScanAtoms(m4afile);
			APar_PrintAtomicTree();
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
			char* base_path=(char*)malloc(sizeof(char)*MAXPATHLEN);
			//CarbonParentFolder( m4afile, base_path );
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
		
		case META_add: {  //creating atoms directly from the shell is unimplemented, currently unusable
			APar_ScanAtoms(m4afile);
			
			//APar_AddMetadataInfo(m4afile, optarg, AtomicDataClass_Text, argv[optind], true);
			break;
		}
		
		case Meta_artist : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©ART.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_songtitle : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©nam.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_album : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©alb.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_genre : {
			APar_ScanAtoms(m4afile);
			APar_AddGenreInfo(m4afile, optarg);
			
			break;
		}
				
		case Meta_tracknum : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.trkn.data", AtomicDataClass_Integer, optarg, false);
			
			break;
		}
		
		case Meta_disknum : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.disk.data", AtomicDataClass_Integer, optarg, false);
			
			break;
		}
		
		case Meta_comment : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©cmt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_year : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©day.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_lyrics : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©lyr.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_composer : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©wrt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		//if copyright is switched to ©cpy tag, then QTplayer can see the copyright, but iTunes no longer will.
		case Meta_copyright : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.cprt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_grouping : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©grp.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_compilation : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "false", 5) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.cpil", false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.cpil.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			}
			break;
		}
		
		case Meta_BPM : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "0", 1) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.tmpo", false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tmpo.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			}
			
			break;
		}
		
		case Meta_advisory : {
			APar_ScanAtoms(m4afile);
			if (strncmp(optarg, "remove", 6) == 0) {
				APar_RemoveAtom("moov.udta.meta.ilst.rtng", false);
			} else {
				APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.rtng.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			}
			break;
		}
		
		case Meta_artwork : { //handled differently: there can be multiple "moov.udta.meta.ilst.covr.data" atoms
			char* env_PicOptions = getenv("PIC_OPTIONS");
			APar_ScanAtoms(m4afile);
			APar_AddMetadataArtwork(m4afile, optarg, env_PicOptions);
			break;
		}
		
		case Meta_StandardDate : {
			//this tag will emerge will a trailing NULL; iTMS doesn't have the trailing NULL
			
			//tagging time will be carried on the "edits list 1" atom; unrecognized by anything - an extrapolation of info
			APar_ScanAtoms(m4afile);
			char* formed_time = (char *)malloc(sizeof(char)*110);
			APar_StandardTime(formed_time);
			//APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tdtg.data", AtomicDataClass_Text, formed_time, false);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©ed1.data", AtomicDataClass_Text, formed_time, false);
			free(formed_time);
			break;
		}
		
		case Meta_URL : {
			//though I've never come across a "©url" atom, its likely to be this
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©url.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_Information : {
			//though I've never come across a "©url" atom, its likely to be this
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.©inf.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_stik : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.stik.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			
			break;
		}
		
		case Meta_description : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.desc.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_TV_Network : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvnn.data", AtomicDataClass_Text, optarg, false); //iTunes: no null-term; AP: null-terminated
			
			break;
		}
		
		case Meta_TV_ShowName : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvsh.data", AtomicDataClass_Text, optarg, false); //iTunes: no null-term; AP: null-terminated
			
			break;
		}
		
		case Meta_TV_Episode : { //if the show "ABC Lost 209", its "209"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tven.data", AtomicDataClass_Text, optarg, false); //iTunes: no null-term; AP: null-terminated
			
			break;
		}
		
		case Meta_TV_SeasonNumber : { //if the show "ABC Lost 209", its 2; integer 2 not char "2"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tvsn.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			
			break;
		}
		
		case Meta_TV_EpisodeNumber : { //if the show "ABC Lost 209", its 2; integer 9 not char "9"
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tves.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			
			break;
		}
		
		case Meta_album_artist : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.aART.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_podcastFlag : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.pcst.data", AtomicDataClass_CPIL_TMPO, optarg, false);
			
			break;
		}
		
		case Meta_keyword : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.keyw.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_category : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.catg.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Metadata_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.meta.ilst", false);
			
			break;
		}
		
		case OPT_WriteBack : {
			alter_original = true;
			break;
		}
		
		} /* end switch */
	} /* end while */
	
	//after all the modifications are enacted on the tree in memory, THEN write out the changes
	if (modified_atoms) {
		APar_DetermineAtomLengths();
		openSomeFile(m4afile, true);
		APar_WriteFile(m4afile, alter_original);
		if (!alter_original) {
			//The file was opened orignally as read-only; when it came time to writeback into the original file, that FILE was closed, and a new one opened with write abilities, so to close a FILE that no longer exists would.... be retarded.
			openSomeFile(m4afile, false);
		}
	}
	return 0;
}
