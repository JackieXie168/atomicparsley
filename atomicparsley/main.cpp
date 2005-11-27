//==================================================================//
/*
    AtomicParsley - main.cpp

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

    Copyright �2005 puck_lock
                                                                   */
//==================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
//#include <Carbon/Carbon.h>

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
#define Meta_compilation         'C'
#define Meta_BPM                 'B'
#define Meta_artwork             'r'
#define Meta_StandardDate        'Z'
#define Meta_advisory            'V'
#define Metadata_Purge           'P'
#define OPT_WriteBack            'O'

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
"  --artist           ,  -a   (str)    Set the artist tag: \"moov.udta.meta.ilst.�ART.data\"\n"
"  --title            ,  -s   (str)    Set the artist tag: \"moov.udta.meta.ilst.�nam.data\"\n"
"  --album            ,  -b   (str)    Set the artist tag: \"moov.udta.meta.ilst.�alb.data\"\n"
"  --genre            ,  -g   (str)    Set the genre tag: \"�gen\" (custom) or \"gnre\" (standard).\n"
"  --tracknum         ,  -k   (num)|(num/tot)  Set the track number (or track number & total tracks).\n"
"  --disk             ,  -d   (num)|(num/tot)  Set the disk number (or disk number & total disks).\n"
"  --comment          ,  -c   (str)    Set the artist tag: \"moov.udta.meta.ilst.�cmt.data\"\n"
"  --year             ,  -y   (num)    Set the year tag: \"moov.udta.meta.ilst.�day.data\"\n"
"  --lyrics           ,  -l   (str)    Set the artist tag: \"moov.udta.meta.ilst.�lyr.data\"\n"
"  --composer         ,  -w   (str)    Set the artist tag: \"moov.udta.meta.ilst.�wrt.data\"\n"
"  --copyright        ,  -x   (str)    Set the copyright tag: \"moov.udta.meta.ilst.cprt.data\"\n"
"  --grouping         ,  -G   (str)    Set the grouping tag: \"moov.udta.meta.ilst.�grp.data\"\n"
"  --artwork          ,  -A   (/path)  Set (multiple) artwork (jpeg or png) tag: \"covr.data\"\n"
"  --bpm              ,  -B   (num)    Set the tempo/bpm tag: \"moov.udta.meta.ilst.tmpo.data\"\n"
"  --compilation      ,  -C   (bool)   Sets the \"cpil\" atom (true or false to delete the atom)\n"
"  --advisory         ,  -y   (1of3)   Sets the iTunes lyrics advisory ('remove', 'clean', 'explicit') \n"
"  --tagtime          ,  -Z            Set the Coordinated Univeral Time of tagging on \"tdtg\"*\n"
"                                     *Denotes utterly non-standard behavior\n"
"\n"
"  --writeBack        ,  -O            If given, writes the file back into original file; deletes temp\n"
"\n"
"  --metaEnema        ,  -P            Douches away every atom under \"moov.udta.meta.ilst\" \n"
"------------------------------------------------------------------------------------------------\n"
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
"\n";

/* void CarbonParentFolder( const char *filepath, char* &basepath) {
	FSRef file_system_ref, parent_fs_ref;
	FSSpec file_spec;
	UInt8 *posixpath;
	posixpath=(UInt8*)malloc(sizeof(UInt8)*MAXPATHLEN);
  OSStatus status = noErr;
	status = FSPathMakeRef( (const UInt8 *)filepath, &file_system_ref, false);
	if (status == noErr) {
		FSGetCatalogInfo (&file_system_ref, kFSCatInfoNone, NULL, NULL, &file_spec, NULL);
		FSGetCatalogInfo (&file_system_ref, kFSCatInfoNone, NULL, NULL, &file_spec, &parent_fs_ref);
		status = FSRefMakePath ( &parent_fs_ref, posixpath, MAXPATHLEN);
		basepath = (char *)posixpath;
	} else {
		basepath = "";
	}
} */

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
		fprintf (stderr,"%s", longHelp_text); exit(0);
	} else if (argc == 2 && ((strncmp(argv[1],"-v",2) == 0) || (strncmp(argv[1],"-version",2) == 0)) ) {
		fprintf(stdout, "%s version: %s\n", argv[0], AtomicParsley_version);
		exit(0);
	} else if (argc == 2 && ( (strncmp(argv[1],"-help",5) == 0) || (strncmp(argv[1],"--help",6) == 0) || (strncmp(argv[1],"-h",5) == 0)) ) {
		fprintf (stderr,"%s", longHelp_text); exit(0);
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
    { "compilation",      required_argument,  NULL,						Meta_compilation },
		{ "advisory",         required_argument,  NULL,						Meta_advisory },
    { "bpm",              required_argument,  NULL,						Meta_BPM },
		{ "artwork",          required_argument,  NULL,						Meta_artwork },
		{ "tagtime",          0,                  NULL,						Meta_StandardDate },
		{ "metaEnema",        0,                  NULL,						Metadata_Purge },
		{ "writeBack",        0,                  NULL,						OPT_WriteBack },
		{ 0, 0, 0, 0 }
	};
		
	int c = -1;
	int option_index = 0;
	
	c = getopt_long_only(argc, argv, "hTtEe:m:a:d:g:c:l:w:y:G:k:A:B:C:V:ZP", long_options, &option_index);
	
	if (c == -1) {
		if (argc < 3 && argc > 2) {
			APar_ScanAtoms(m4afile);
			APar_PrintAtomicTree();
		}
		break;
	}
	
	
	signal(SIGTERM, kill_signal);
	signal(SIGKILL, kill_signal);
	signal(SIGINT,  kill_signal);
	
	switch(c) {
		// "optind" represents the count of arguments up to and including its optional flag:

		case '?': return 1;
			
		case OPT_HELP: {
			fprintf (stderr,"%s", longHelp_text); return 0;
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
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�ART.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_songtitle : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�nam.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_album : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�alb.data", AtomicDataClass_Text, optarg, false);
			
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
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�cmt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_year : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�day.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_lyrics : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�lyr.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_composer : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�wrt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_copyright : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.cprt.data", AtomicDataClass_Text, optarg, false);
			
			break;
		}
		
		case Meta_grouping : {
			APar_ScanAtoms(m4afile);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�grp.data", AtomicDataClass_Text, optarg, false);
			
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
			
			//...and apparently, �day isn't the right tag for tagging time... well.... for another day & tag then (id3v2: TDTG)
			//this is �REALLY� non-standard. I don't think any taggers would recognize it.
			APar_ScanAtoms(m4afile);
			char* formed_time = (char *)malloc(sizeof(char)*110);
			APar_StandardTime(formed_time);
			//APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.�day.data", AtomicDataClass_Text, formed_time, false);
			APar_AddMetadataInfo(m4afile, "moov.udta.meta.ilst.tdtg.data", AtomicDataClass_Text, formed_time, false);
			free(formed_time);
		}
		
		case Metadata_Purge : {
			APar_ScanAtoms(m4afile);
			APar_RemoveAtom("moov.udta.meta.ilst", false);
		}
		
		case OPT_WriteBack : {
			alter_original = true;
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