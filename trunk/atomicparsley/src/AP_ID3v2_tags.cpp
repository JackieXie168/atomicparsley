//==================================================================//
/*
    AtomicParsley - AP_ID3v2_tags.cpp

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

    Copyright ©2006 puck_lock
                                                                   */
//==================================================================//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "AtomicParsley.h"
#include "AP_commons.h"
#include "AP_ID3v2_tags.h"
#include "APar_zlib.h"
#include "AP_iconv.h"
#include "APar_uuid.h"
#include "AP_CDTOC.h"

#include "AP_ID3v2_FrameDefinitions.h"

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

ID3v2Tag* GlobalID3Tag = NULL;

//prefs
uint8_t AtomicParsley_ID3v2Tag_MajorVersion = 4;
uint8_t AtomicParsley_ID3v2Tag_RevisionVersion = 0;
uint8_t AtomicParsley_ID3v2Tag_Flags = 0;

enum ID3v2_TagFlags {
	ID32_TAGFLAG_BIT0 = 0x01,
	ID32_TAGFLAG_BIT1 = 0x02,
	ID32_TAGFLAG_BIT2 = 0x04,
	ID32_TAGFLAG_BIT3 = 0x08,
	ID32_TAGFLAG_FOOTER = 0x10,
	ID32_TAGFLAG_EXPERIMENTAL = 0x20,
	ID32_TAGFLAG_EXTENDEDHEADER = 0x40,
	ID32_TAGFLAG_UNSYNCRONIZATION = 0x80
};

enum ID3v2_FrameFlags {
	ID32_FRAMEFLAG_STATUS         =  0x4000,
	ID32_FRAMEFLAG_PRESERVE       =  0x2000,
	ID32_FRAMEFLAG_READONLY       =  0x1000,
	ID32_FRAMEFLAG_GROUPING       =  0x0040,
	ID32_FRAMEFLAG_COMPRESSED     =  0x0008,
	ID32_FRAMEFLAG_ENCRYPTED      =  0x0004,
	ID32_FRAMEFLAG_UNSYNCED       =  0x0002,
	ID32_FRAMEFLAG_LENINDICATED   =  0x0001
};

bool ID3v2Tag_Flag_Footer = false; //bit4; MPEG-4 'ID32' requires this to be false
bool ID3v2Tag_Flag_Experimental = true; //bit5
bool ID3v2Tag_Flag_ExtendedHeader = true; //bit6
bool ID3v2Tag_Flag_Unsyncronization = false; //bit7

///////////////////////////////////////////////////////////////////////////////////////
//                              generic functions                                    //
///////////////////////////////////////////////////////////////////////////////////////

uint32_t syncsafe32_to_UInt32(char* syncsafe_int) {
	if (syncsafe_int[0] & 0x80 || syncsafe_int[1] & 0x80 || syncsafe_int[2] & 0x80 || syncsafe_int[3] & 0x80) return 0;
	return (syncsafe_int[0] << 21) | (syncsafe_int[1] << 14) | (syncsafe_int[2] << 7) | syncsafe_int[3];
}

uint16_t syncsafe16_to_UInt16(char* syncsafe_int) {
	if (syncsafe_int[0] & 0x80 || syncsafe_int[1] & 0x80) return 0;
	return (syncsafe_int[0] << 7) | syncsafe_int[1];
}

void convert_to_syncsafe32(uint32_t in_uint, char* buffer) {
	buffer[0] = (in_uint >> 21) & 0x7F;
	buffer[1] = (in_uint >> 14) & 0x7F;
	buffer[2] = (in_uint >>  7) & 0x7F;
	buffer[3] = (in_uint >>  0) & 0x7F;
	return;
}

uint32_t UInt24FromBigEndian(const char *string) { //v2.2 frame lengths
	return (0 << 24 | (string[0] & 0xff) << 16 | (string[1] & 0xff) << 8 | string[2] & 0xff) << 0;
}

bool ID3v2_PaddingTest(char* buffer) {
	if (buffer[0] & 0x00 || buffer[1] & 0x00 || buffer[2] & 0x00 || buffer[3] & 0x00) return true;
	return false;
}

bool ID3v2_TestFrameID_NonConformance(char* frameid) {
	for (uint8_t i=0; i < 4; i++) {
		if ( !((frameid[i] >= '0' && frameid[i] <= '9') || ( frameid[i] >= 'A' && frameid[i] <= 'Z' )) ) {
			return true;
		}
	}
	return false;
}

bool ID3v2_TestTagFlag(uint8_t TagFlag, uint8_t TagBit) {
	if (TagFlag & TagBit) return true;
	return false;
}

bool ID3v2_TestFrameFlag(uint16_t FrameFlag, uint16_t FrameBit) {
	if (FrameFlag & FrameBit) return true;
	return false;
}

uint8_t TextField_TestBOM(char* astring) {
	if (((unsigned char*)astring)[0] == 0xFE && ((unsigned char*)astring)[1] == 0xFF) return 13; //13 looks like a B for BE
	if (((unsigned char*)astring)[0] == 0xFF && ((unsigned char*)astring)[1] == 0xFE) return 1; //1 looks like a l for LE
	return 0;
}

void APar_LimitBufferRange(uint32_t max_allowed, uint32_t target_amount) {
	if (target_amount > max_allowed) {
		fprintf(stderr, "AtomicParsley error: insufficient memory to process ID3 tags (%u>%u). Exiting.\n", target_amount, max_allowed);
		exit( target_amount - max_allowed );
	}
	return;
}

void APar_ValidateNULLTermination8bit(ID3v2Fields* this_field) {
	if (this_field->field_string[0] == 0) {
		this_field->field_length = 1;
	} else if (this_field->field_string[this_field->field_length-1] != 0) {
		this_field->field_length += 1;
	}
	return;
}

void APar_ValidateNULLTermination16bit(ID3v2Fields* this_field, uint8_t encoding) {
	if (this_field->field_string[0] == 0 && this_field->field_string[1] == 0) {
		this_field->field_length = 2;
		if (encoding == TE_UTF16LE_WITH_BOM) {
			if ( ((uint8_t)(this_field->field_string[0]) != 0xFF && (uint8_t)(this_field->field_string[1]) != 0xFE) || 
			     ((uint8_t)(this_field->field_string[0]) != 0xFE && (uint8_t)(this_field->field_string[1]) != 0xFF) ) {
				memcpy(this_field->field_string, "\xFF\xFE", 2);
				this_field->field_length = 4;
			}
		}
	} else if (this_field->field_string[this_field->field_length-2] != 0 && this_field->field_string[this_field->field_length-1] != 0) {
		this_field->field_length += 2;
	}
	return;
}

bool APar_EvalFrame_for_Field(int frametype, int fieldtype) {
	uint8_t frametype_idx = GetFrameCompositionDescription(frametype);
	
	for (uint8_t fld_i = 0; fld_i < FrameTypeConstructionList[frametype_idx].ID3_FieldCount; fld_i++) {
		if (FrameTypeConstructionList[frametype_idx].ID3_FieldComponents[fld_i] == fieldtype) {
			return true;
		}
	}
	return false;
}

uint32_t ID3v2_desynchronize(char* buffer, uint32_t bufferlen) {
	char* buf_ptr = buffer;
	uint32_t desync_count = 0;
	
	for (uint32_t i = 0; i < bufferlen; i++) {
		if ((unsigned char)buffer[i] == 0xFF && (unsigned char)buffer[i+1] == 0x00) {
			buf_ptr[desync_count] = buffer[i];
			i++;
		} else {
			buf_ptr[desync_count] = buffer[i];
		}
		desync_count++;
	}
	return desync_count;
}

///////////////////////////////////////////////////////////////////////////////////////
//                             test functions                                        //
///////////////////////////////////////////////////////////////////////////////////////

void WriteZlibData(char* buffer, uint32_t buff_len) {
	char* indy_atom_path = (char *)malloc(sizeof(char)*MAXPATHLEN); //this malloc can escape memset because its only for in-house testing
	strcat(indy_atom_path, "/Users/");
	strcat(indy_atom_path, getenv("USER") );
	strcat(indy_atom_path, "/Desktop/id3framedata.txt");

	FILE* test_file = fopen(indy_atom_path, "wb");
	if (test_file != NULL) {
		
		fwrite(buffer, (size_t)buff_len, 1, test_file);
	}
	fclose(test_file);
	free(indy_atom_path);
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                               cli functions                                       //
///////////////////////////////////////////////////////////////////////////////////////

void ListID3FrameIDstrings() {
	char* frametypestr = NULL;
	char* presetpadding = NULL;
	uint16_t total_known_frames = (uint16_t)(sizeof(KnownFrames)/sizeof(*KnownFrames));
	fprintf(stdout, "ID3v2.4 Implemented Frames:\nframeID    type                   alias       Description\n--------------------------------------------------------------------------\n");
	for (uint16_t i = 1; i < total_known_frames; i++) {
		if (strlen(KnownFrames[i].ID3V2p4_FrameID) != 4) continue;
		if (KnownFrames[i].ID3v2_FrameType == ID3_TEXT_FRAME) {
			frametypestr = "text frame             ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_TEXT_FRAME_USERDEF) {
			frametypestr = "user defined text frame";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_URL_FRAME) {
			frametypestr = "url frame              ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_URL_FRAME_USERDEF) {
			frametypestr = "user defined url frame ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_TEXT_FRAME_USERDEF) {
			frametypestr = "user text frame        ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_UNIQUE_FILE_ID_FRAME) {
			frametypestr = "file ID                ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_DESCRIBED_TEXT_FRAME) {
			frametypestr = "described text frame   ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_ATTACHED_PICTURE_FRAME) {
			frametypestr = "picture frame          ";
		} else if (KnownFrames[i].ID3v2_FrameType == ID3_ATTACHED_OBJECT_FRAME) {
			frametypestr = "encapuslated object frm";
		}
		
		int strpad = 12 - strlen(KnownFrames[i].CLI_frameIDpreset);
		if (strpad == 12) {
			presetpadding = "            ";
		} else if (strpad == 11) {
			presetpadding = "           ";
		} else if (strpad == 10) {
			presetpadding = "          ";
		} else if (strpad == 9) {
			presetpadding = "         ";
		} else if (strpad == 8) {
			presetpadding = "        ";
		} else if (strpad == 7) {
			presetpadding = "       ";
		} else if (strpad == 6) {
			presetpadding = "      ";
		} else if (strpad == 5) {
			presetpadding = "     ";
		} else if (strpad == 4) {
			presetpadding = "    ";
		} else if (strpad == 3) {
			presetpadding = "   ";
		} else if (strpad == 2) {
			presetpadding = "  ";
		} else if (strpad == 1) {
			presetpadding = " ";
		} else if (strpad <= 0) {
			presetpadding = "";
		}

		fprintf(stdout, "%s   %s    %s%s | %s\n", KnownFrames[i].ID3V2p4_FrameID, frametypestr, KnownFrames[i].CLI_frameIDpreset, presetpadding, KnownFrames[i].ID3V2_FrameDescription);
	}
	return;
}

void List_imagtype_strings() {
	uint8_t total_imgtyps = (uint8_t)(sizeof(ImageTypeList)/sizeof(*ImageTypeList));
	fprintf(stdout, "These 'image types' are used to identify pictures embedded in 'APIC' ID3 tags:\n      usage is \"AP --ID3Tag APIC /path.jpg --imagetype=\"str\"\n      str can be either the hex listing *or* the full string\n      default is 0x00 - meaning 'Other'\n   Hex       Full String\n  ----------------------------\n");
	for (uint8_t i=0; i < total_imgtyps; i++) {
		fprintf(stdout, "  %s      \"%s\"\n", ImageTypeList[i].hexstring, ImageTypeList[i].imagetype_str);
	}
	return;
}

char* ConvertCLIFrameStr_TO_frameID(char* frame_str) {
	char* discovered_frameID = NULL;
	uint16_t total_known_frames = (uint16_t)(sizeof(KnownFrames)/sizeof(*KnownFrames));
	uint8_t frame_str_len = strlen(frame_str) + 1;
	
	for (uint16_t i = 0; i < total_known_frames; i++) {
		if (memcmp(KnownFrames[i].CLI_frameIDpreset, frame_str, frame_str_len) == 0) {
			if (AtomicParsley_ID3v2Tag_MajorVersion == 2) discovered_frameID = KnownFrames[i].ID3V2p2_FrameID;
			if (AtomicParsley_ID3v2Tag_MajorVersion == 3) discovered_frameID = KnownFrames[i].ID3V2p3_FrameID;
			if (AtomicParsley_ID3v2Tag_MajorVersion == 4) discovered_frameID = KnownFrames[i].ID3V2p4_FrameID;
			
			if (strlen(discovered_frameID) == 0) discovered_frameID = NULL;
			break;
		}
	}
	return discovered_frameID;
}

//0 = description
//1 = mimetype
//2 = type
bool TestCLI_for_FrameParams(int frametype, uint8_t testparam) {
	if (frametype == ID3_URL_FRAME_USERDEF && testparam == 0) return true;
	
	if (frametype == ID3_UNIQUE_FILE_ID_FRAME && testparam == 3) {
		return true;
	}
	
	uint8_t frametype_idx = GetFrameCompositionDescription(frametype);
	
	for (uint8_t fld_i = 0; fld_i < FrameTypeConstructionList[frametype_idx].ID3_FieldCount; fld_i++) {
		if (FrameTypeConstructionList[frametype_idx].ID3_FieldComponents[fld_i] == ID3_DESCRIPTION_FIELD && testparam == 0) {
			return true;
		}
		if (FrameTypeConstructionList[frametype_idx].ID3_FieldComponents[fld_i] == ID3_MIME_TYPE_FIELD && testparam == 1) {
			return true;
		}
		if (FrameTypeConstructionList[frametype_idx].ID3_FieldComponents[fld_i] == ID3_PIC_TYPE_FIELD && testparam == 2) {
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////
//                         frame identity functions                                  //
///////////////////////////////////////////////////////////////////////////////////////

uint16_t MatchID3FrameIDstr(const char* foundFrameID, uint8_t tagVersion) {
	uint16_t matchingFrameID = 0; //return the UnknownFrame if it can't be found
	uint16_t total_known_frames = (uint16_t)(sizeof(KnownFrames)/sizeof(*KnownFrames));
	uint8_t frameLen = (tagVersion >= 3 ? 4 : 3) +1;
	
	for (uint16_t i = 0; i < total_known_frames; i++) {
		char* testFrameID = NULL;
		if (tagVersion == 2) testFrameID = KnownFrames[i].ID3V2p2_FrameID;
		if (tagVersion == 3) testFrameID = KnownFrames[i].ID3V2p3_FrameID;
		if (tagVersion == 4) testFrameID = KnownFrames[i].ID3V2p4_FrameID;
		
		if (memcmp(foundFrameID, testFrameID, frameLen) == 0) {
			matchingFrameID = i;
			break;
		}
	}
	return matchingFrameID;
}

uint8_t GetFrameCompositionDescription(int ID3v2_FrameTypeID) {
	uint8_t matchingFrameDescription = 0; //return the UnknownFrame/UnknownField if it can't be found
	uint8_t total_frame_descrips = (uint8_t)(sizeof(FrameTypeConstructionList)/sizeof(*FrameTypeConstructionList));
	
	for (uint8_t i = 0; i < total_frame_descrips; i++) {		
		if (FrameTypeConstructionList[i].ID3_FrameType == ID3v2_FrameTypeID) {
			matchingFrameDescription = i;
			break;
		}
	}
	return matchingFrameDescription;
}

int FrameStr_TO_FrameType(char* frame_str) {
	char* eval_framestr = NULL;
	int frame_type = 0;
	uint16_t total_known_frames = (uint16_t)(sizeof(KnownFrames)/sizeof(*KnownFrames));
	uint8_t frame_str_len = strlen(frame_str) + 1;
	
	for (uint16_t i = 0; i < total_known_frames; i++) {
		if (AtomicParsley_ID3v2Tag_MajorVersion == 2) eval_framestr = KnownFrames[i].ID3V2p2_FrameID;
		if (AtomicParsley_ID3v2Tag_MajorVersion == 3) eval_framestr = KnownFrames[i].ID3V2p3_FrameID;
		if (AtomicParsley_ID3v2Tag_MajorVersion == 4) eval_framestr = KnownFrames[i].ID3V2p4_FrameID;
			
		if (memcmp(frame_str, eval_framestr, frame_str_len) == 0) {
			frame_type = KnownFrames[i].ID3v2_FrameType;
			break;
		}
	}
	return frame_type;
}

///////////////////////////////////////////////////////////////////////////////////////
//                            id3 parsing functions                                  //
///////////////////////////////////////////////////////////////////////////////////////

uint32_t APar_ExtractField(char* buffer, uint32_t maxFieldLen, ID3v2Frame* thisFrame, uint8_t fieldNum, int fieldType, uint8_t textEncoding) {
	uint32_t bytes_used = 0;
	switch(fieldType) {
		case ID3_UNKNOWN_FIELD : { //the difference between this unknown field & say a binary data field is the unknown field is always the first (and only) field
			thisFrame->ID3v2_Frame_Fields->ID3v2_Field_Type = ID3_UNKNOWN_FIELD;
			thisFrame->ID3v2_Frame_Fields->field_length = maxFieldLen;
			thisFrame->ID3v2_Frame_Fields->field_string = (char*)calloc(1, sizeof(char)*(maxFieldLen+1 > 16 ? maxFieldLen+1 : 16));
			thisFrame->ID3v2_Frame_Fields->alloc_length = sizeof(char)*(maxFieldLen+1 > 16 ? maxFieldLen+1 : 16);
			memcpy(thisFrame->ID3v2_Frame_Fields->field_string, buffer, maxFieldLen);
			
			bytes_used = maxFieldLen;
			break;
		}
		case ID3_PIC_TYPE_FIELD :
		case ID3_GROUPSYMBOL_FIELD :
		case ID3_TEXT_ENCODING_FIELD : {
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->ID3v2_Field_Type = fieldType;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length = 1;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string = (char*)calloc(1, sizeof(char)*16);
			memcpy((thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string, buffer, 1);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->alloc_length = sizeof(char)*16;
			
			bytes_used = 1;
			break;
		}
		case ID3_LANGUAGE_FIELD : {
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->ID3v2_Field_Type = ID3_LANGUAGE_FIELD;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length = 3;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string = (char*)calloc(1, sizeof(char)*16);
			memcpy((thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string, buffer, 3);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->alloc_length = sizeof(char)*16;
			
			bytes_used = 3;
			break;
		}
		case ID3_TEXT_FIELD :
		case ID3_URL_FIELD :
		case ID3_BINARY_DATA_FIELD : { //this class of fields may contains NULLs but is *NOT* NULL terminated in any form
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->ID3v2_Field_Type = fieldType;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length = maxFieldLen;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string = (char*)calloc(1, sizeof(char)*maxFieldLen+1 > 16 ? maxFieldLen+1 : 16);
			memcpy((thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string, buffer, maxFieldLen);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->alloc_length = (sizeof(char)*maxFieldLen+1 > 16 ? maxFieldLen+1 : 16);
			
			bytes_used = maxFieldLen;
			break;
		}
		case ID3_MIME_TYPE_FIELD :
		case ID3_OWNER_FIELD : { //difference between ID3_OWNER_FIELD & ID3_DESCRIPTION_FIELD field classes is the owner field is always 8859-1 encoded (single NULL term)
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->ID3v2_Field_Type = fieldType;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length = findstringNULLterm(buffer, 0, maxFieldLen);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string = (char*)calloc(1, sizeof(char)*
			                                    (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 > 16 ? (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 : 16);
			memcpy((thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string, buffer, (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->alloc_length = (sizeof(char)*maxFieldLen+1 > 16 ? maxFieldLen+1 : 16);
						
			bytes_used = (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length;
			break;
		
		}
		case ID3_FILENAME_FIELD :
		case ID3_DESCRIPTION_FIELD : {
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->ID3v2_Field_Type = fieldType;
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length = findstringNULLterm(buffer, textEncoding, maxFieldLen);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string = (char*)calloc(1, sizeof(char)*
			                                    (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 > 16 ? (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 : 16);
			memcpy((thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string, buffer, (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length);
			(thisFrame->ID3v2_Frame_Fields+fieldNum)->alloc_length = (sizeof(char)*
			                                    (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 > 16 ? (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length +1 : 16);
						
			bytes_used = (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_length;
			break;
		}
	}
	//fprintf(stdout, "%u, %s, %s\n", bytes_used, buffer, (thisFrame->ID3v2_Frame_Fields+fieldNum)->field_string);
	return bytes_used;
}

void APar_ScanID3Frame(ID3v2Frame* targetframe, char* frame_ptr, uint32_t frameLen) {
	switch(targetframe->ID3v2_FrameType) {
		case ID3_UNKNOWN_FRAME : {
			APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_UNKNOWN_FIELD, 0);
			break;
		}
		case ID3_TEXT_FRAME : {
			APar_ExtractField(frame_ptr, 1, targetframe, 0, ID3_TEXT_ENCODING_FIELD, 0);
			
			APar_ExtractField(frame_ptr + 1, frameLen - 1, targetframe, 1, ID3_TEXT_FIELD, targetframe->ID3v2_Frame_Fields->field_string[0]);
			break;
		}
		case ID3_URL_FRAME : {
			APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_URL_FIELD, 0);
			break;
		}
		case ID3_TEXT_FRAME_USERDEF :
		case ID3_URL_FRAME_USERDEF : {
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, 1, targetframe, 0, ID3_TEXT_ENCODING_FIELD, 0);
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 1, ID3_DESCRIPTION_FIELD, 
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			
			offset_into_frame += skipNULLterm(frame_ptr + offset_into_frame, targetframe->ID3v2_Frame_Fields->field_string[0], frameLen - offset_into_frame);
			
			if (targetframe->ID3v2_FrameType == ID3_TEXT_FRAME_USERDEF) {
				APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 2, ID3_TEXT_FIELD, 
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			} else if (targetframe->ID3v2_FrameType == ID3_URL_FRAME_USERDEF) {
				APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 2, ID3_URL_FIELD, 
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			}
			break;
		}
		case ID3_UNIQUE_FILE_ID_FRAME : {
			
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_OWNER_FIELD, 0);
			offset_into_frame++; //iso-8859-1 owner field is NULL terminated
			
			APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 1, ID3_BINARY_DATA_FIELD, 0);
			break;
		}
		case ID3_CD_ID_FRAME : {
			APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_BINARY_DATA_FIELD, 0);
			break;
		}
		case ID3_DESCRIBED_TEXT_FRAME : {
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, 1, targetframe, 0, ID3_TEXT_ENCODING_FIELD, 0);
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, 3, targetframe, 1, ID3_LANGUAGE_FIELD, 0);
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 2, ID3_DESCRIPTION_FIELD, 
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			
			offset_into_frame += skipNULLterm(frame_ptr + offset_into_frame, targetframe->ID3v2_Frame_Fields->field_string[0], frameLen - offset_into_frame);
			
			if (frameLen > offset_into_frame) {
				APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 3, ID3_TEXT_FIELD, 
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			}
			break;
		}
		case ID3_ATTACHED_OBJECT_FRAME :
		case ID3_ATTACHED_PICTURE_FRAME : {
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, 1, targetframe, 0, ID3_TEXT_ENCODING_FIELD, 0);
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - 1, targetframe, 1, ID3_MIME_TYPE_FIELD, 0);
			
			offset_into_frame += 1; //should only be 1 NULL
			
			if (targetframe->ID3v2_FrameType == ID3_ATTACHED_PICTURE_FRAME) {
				offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, 1, targetframe, 2, ID3_PIC_TYPE_FIELD, 0);
			} else {
				offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 2, ID3_FILENAME_FIELD, 0);
			
				offset_into_frame+=skipNULLterm(frame_ptr + offset_into_frame, targetframe->ID3v2_Frame_Fields->field_string[0], frameLen - offset_into_frame);
			}
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 3, ID3_DESCRIPTION_FIELD,
																																																targetframe->ID3v2_Frame_Fields->field_string[0]);
			
			offset_into_frame += skipNULLterm(frame_ptr + offset_into_frame, targetframe->ID3v2_Frame_Fields->field_string[0], frameLen - offset_into_frame);
			
			if (frameLen > offset_into_frame) {
				offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 4, ID3_BINARY_DATA_FIELD, 0);
			}
			break;
		}
		case ID3_PRIVATE_FRAME : { //the only difference between the 'priv' frame & the 'ufid' frame is ufid is limited to 64 bytes
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_OWNER_FIELD, 0);
			offset_into_frame++; //iso-8859-1 owner field is NULL terminated
			
			APar_ExtractField(frame_ptr + offset_into_frame, frameLen - 1, targetframe, 1, ID3_BINARY_DATA_FIELD, 0);
			break;
		}
		case ID3_GROUP_ID_FRAME : {
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_OWNER_FIELD, 0);
			offset_into_frame++; //iso-8859-1 owner field is NULL terminated
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, 1, targetframe, 1, ID3_GROUPSYMBOL_FIELD, 0);
			
			if (frameLen > offset_into_frame) {
				APar_ExtractField(frame_ptr + offset_into_frame, frameLen - offset_into_frame, targetframe, 2, ID3_BINARY_DATA_FIELD, 0);
			}
			break;
		}
		case ID3_SIGNATURE_FRAME : {
			APar_ExtractField(frame_ptr, 1, targetframe, 0, ID3_GROUPSYMBOL_FIELD, 0);
			
			APar_ExtractField(frame_ptr + 1, frameLen - 1, targetframe, 1, ID3_BINARY_DATA_FIELD, 0);
			break;
		}
		case ID3_PLAYCOUNTER_FRAME : {
			APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_COUNTER_FIELD, 0);
			break;
		}
		case ID3_POPULAR_FRAME : {
			uint32_t offset_into_frame = 0;
			offset_into_frame += APar_ExtractField(frame_ptr, frameLen, targetframe, 0, ID3_OWNER_FIELD, 0); //surrogate for 'emai to user' field
			offset_into_frame++; //iso-8859-1 email address field is NULL terminated
			
			offset_into_frame += APar_ExtractField(frame_ptr + offset_into_frame, 1, targetframe, 1, ID3_BINARY_DATA_FIELD, 0);
			
			if (frameLen > offset_into_frame) {
				APar_ExtractField(frame_ptr, frameLen - offset_into_frame, targetframe, 2, ID3_COUNTER_FIELD, 0);
			}
			break;
		}
		case ID3_OLD_V2P2_PICTURE_FRAME : {
			break; //unimplemented
		}
	}
	return;
}

void APar_ID32_ScanID3Tag(FILE* source_file, AtomicInfo* id32_atom) {
	char* id32_fulltag = (char*)calloc(1, sizeof(char)*id32_atom->AtomicLength);
	char* fulltag_ptr = id32_fulltag;
	
	if (id32_atom->AtomicLength < 20) return;
	APar_readX(id32_fulltag, source_file, id32_atom->AtomicStart+14, id32_atom->AtomicLength-14); //+10 = 4bytes ID32 atom length + 4bytes ID32 atom name + 2 bytes packed lang
	
	if (memcmp(id32_fulltag, "ID3", 3) != 0) return;
	fulltag_ptr+=3;
	
	id32_atom->ID32_TagInfo = (ID3v2Tag*)calloc(1, sizeof(ID3v2Tag));
	id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion = *fulltag_ptr;
	fulltag_ptr++;
	id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion = *fulltag_ptr;
	fulltag_ptr++;
	id32_atom->ID32_TagInfo->ID3v2Tag_Flags = *fulltag_ptr;
	fulltag_ptr++;
	
	if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion != 4) {
		fprintf(stdout, "AtomicParsley warning: an ID32 atom was encountered using an unsupported ID3v2 tag version: %u. Skipping\n", id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion);
		return;
	}
	if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4 && id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion != 0) {
		fprintf(stdout, "AtomicParsley warning: an ID32 atom was encountered using an unsupported ID3v2.4 tag revision: %u. Skipping\n", id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion);
		return;
	}
	
	if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_BIT0+ID32_TAGFLAG_BIT1+ID32_TAGFLAG_BIT2+ID32_TAGFLAG_BIT3)) return;
	
	if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_FOOTER)) {
		fprintf(stdout, "AtomicParsley error: an ID32 atom was encountered with a forbidden footer flag. Exiting.\n");
		free(id32_fulltag);
		id32_fulltag = NULL;
		return;
	}
		
	if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_EXPERIMENTAL)) {
#if defined(DEBUG_V)
		fprintf(stdout, "AtomicParsley warning: an ID32 atom was encountered with an experimental flag set.\n");
#endif
	}
	
	if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
		id32_atom->ID32_TagInfo->ID3v2Tag_Length = syncsafe32_to_UInt32(fulltag_ptr);
		fulltag_ptr+=4;
	} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3) {
		id32_atom->ID32_TagInfo->ID3v2Tag_Length = UInt32FromBigEndian(fulltag_ptr); //TODO: when testing ends, this switches to syncsafe
		fulltag_ptr+=4;
	} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 2) {
		id32_atom->ID32_TagInfo->ID3v2Tag_Length = UInt24FromBigEndian(fulltag_ptr);
		fulltag_ptr+=3;
	}
	
	if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_UNSYNCRONIZATION)) {
		//uint32_t newtagsize = ID3v2_desynchronize(id32_fulltag, id32_atom->ID32_TagInfo->ID3v2Tag_Length);
		//fprintf(stdout, "New tag size is %u\n", newtagsize);
		//WriteZlibData(id32_fulltag, newtagsize);
		//exit(0);
		fprintf(stdout, "AtomicParsley error: an ID3 tag with the unsynchronized flag set which is not supported. Skipping.\n");
		free(id32_fulltag);
		id32_fulltag = NULL;
		return;
	}

	if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_EXTENDEDHEADER)) {
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
			id32_atom->ID32_TagInfo->ID3v2_Tag_ExtendedHeader_Length = syncsafe32_to_UInt32(fulltag_ptr);
		} else {
			id32_atom->ID32_TagInfo->ID3v2_Tag_ExtendedHeader_Length = UInt32FromBigEndian(fulltag_ptr); //TODO: when testing ends, this switches to syncsafe; 2.2 doesn't have it
		}
		fulltag_ptr+= id32_atom->ID32_TagInfo->ID3v2_Tag_ExtendedHeader_Length;
	}
	
	id32_atom->ID32_TagInfo->ID3v2_FirstFrame = NULL;
	id32_atom->ID32_TagInfo->ID3v2_FrameList = NULL;
	
	//loop through parsing frames
	while (fulltag_ptr < id32_fulltag + (id32_atom->AtomicLength-14) ) {
		uint32_t fullframesize = 0;

		if (ID3v2_PaddingTest(fulltag_ptr)) break;
		if (ID3v2_TestFrameID_NonConformance(fulltag_ptr)) break;
		
		ID3v2Frame* target_list_frameinfo = (ID3v2Frame*)calloc(1, sizeof(ID3v2Frame));
		target_list_frameinfo->ID3v2_NextFrame = NULL;
		target_list_frameinfo->ID3v2_Frame_ID = MatchID3FrameIDstr(fulltag_ptr, id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion); //hardcoded way
		target_list_frameinfo->ID3v2_FrameType = KnownFrames[target_list_frameinfo->ID3v2_Frame_ID].ID3v2_FrameType;
		
		uint8_t FrameCompositionList = GetFrameCompositionDescription(target_list_frameinfo->ID3v2_FrameType);
		target_list_frameinfo->ID3v2_FieldCount = FrameTypeConstructionList[FrameCompositionList].ID3_FieldCount;
		target_list_frameinfo->ID3v2_Frame_ExpandedLength = 0;
		target_list_frameinfo->eliminate_frame = false;
		uint8_t frame_offset = 0;

		if (id32_atom->ID32_TagInfo->ID3v2_FrameList != NULL) id32_atom->ID32_TagInfo->ID3v2_FrameList->ID3v2_NextFrame = target_list_frameinfo;

		//need to lookup how many components this Frame_ID is associated with. Do this by using the corresponding KnownFrames.ID3v2_FrameType
		//ID3v2_FrameType describes the general form this frame takes (text, text with description, attached object, attached picture)
		//the general form is composed of several fields; that number of fields needs to be malloced to target_list_frameinfo->ID3v2_Frame_Fields
		//and each target_list_frameinfo->ID3v2_Frame_Fields+num->field_string needs to be malloced and copied from id32_fulltag
		
		memset(target_list_frameinfo->ID3v2_Frame_Namestr, 0, 5);
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 2) {
			memcpy(target_list_frameinfo->ID3v2_Frame_Namestr, fulltag_ptr, 3);
			fulltag_ptr+= 3;
		} else {
			memcpy(target_list_frameinfo->ID3v2_Frame_Namestr, fulltag_ptr, 4);
			fulltag_ptr+= 4;
		}
		
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
			target_list_frameinfo->ID3v2_Frame_Length = syncsafe32_to_UInt32(fulltag_ptr);
			fulltag_ptr+=4;
		} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3) {
			target_list_frameinfo->ID3v2_Frame_Length = UInt32FromBigEndian(fulltag_ptr); //TODO: when testing ends, this switches to syncsafe
			fulltag_ptr+=4;
		} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 2) {
			target_list_frameinfo->ID3v2_Frame_Length = UInt24FromBigEndian(fulltag_ptr);
			fulltag_ptr+=3;
		}
		
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion >= 3) {
			target_list_frameinfo->ID3v2_Frame_Flags = UInt16FromBigEndian(fulltag_ptr); //v2.2 doesn't have frame level flags (but it does have field level flags)
			fulltag_ptr+=2;
			
			if (ID3v2_TestFrameFlag(target_list_frameinfo->ID3v2_Frame_Flags, ID32_FRAMEFLAG_UNSYNCED)) {
				//DE-UNSYNC frame
				fullframesize = target_list_frameinfo->ID3v2_Frame_Length;
				target_list_frameinfo->ID3v2_Frame_Length = ID3v2_desynchronize(fulltag_ptr+frame_offset, target_list_frameinfo->ID3v2_Frame_Length);
				target_list_frameinfo->ID3v2_Frame_Flags -= ID32_FRAMEFLAG_UNSYNCED;
			}
			
			//info based on frame flags (order based on the order of flags defined by the frame flags
			if (ID3v2_TestFrameFlag(target_list_frameinfo->ID3v2_Frame_Flags, ID32_FRAMEFLAG_GROUPING)) {
#if defined(DEBUG_V)
				fprintf(stdout, "Frame %s has a grouping flag set\n", target_list_frameinfo->ID3v2_Frame_Namestr);
#endif
				target_list_frameinfo->ID3v2_Frame_GroupingSymbol = *fulltag_ptr; //er, uh... wouldn't this also require ID32_FRAMEFLAG_LENINDICATED to be set???
				frame_offset++;
			}
			
			if (ID3v2_TestFrameFlag(target_list_frameinfo->ID3v2_Frame_Flags, ID32_FRAMEFLAG_COMPRESSED)) { // technically ID32_FRAMEFLAG_LENINDICATED should also be tested
#if defined(DEBUG_V)
				fprintf(stdout, "Frame %s has a compressed flag set\n", target_list_frameinfo->ID3v2_Frame_Namestr);
#endif
				if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
					target_list_frameinfo->ID3v2_Frame_ExpandedLength = syncsafe32_to_UInt32(fulltag_ptr+frame_offset);
					frame_offset+=4;
				} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3) {
					target_list_frameinfo->ID3v2_Frame_ExpandedLength = UInt32FromBigEndian(fulltag_ptr+frame_offset); //TODO: when testing ends, switch this to syncsafe
					frame_offset+=4;
				}
			}
		}
		
		
		target_list_frameinfo->ID3v2_Frame_Fields = (ID3v2Fields*)calloc(1, sizeof(ID3v2Fields)*target_list_frameinfo->ID3v2_FieldCount);
		char* expanded_frame = NULL;
		char* frame_ptr = NULL;
		uint32_t frameLen = 0;
		
		if (target_list_frameinfo->ID3v2_Frame_ExpandedLength != 0) {
#ifdef HAVE_ZLIB_H
			expanded_frame = (char*)calloc(1, sizeof(char)*target_list_frameinfo->ID3v2_Frame_ExpandedLength + 1);
			APar_zlib_inflate(fulltag_ptr+frame_offset, target_list_frameinfo->ID3v2_Frame_Length, expanded_frame, target_list_frameinfo->ID3v2_Frame_ExpandedLength);
			
			//WriteZlibData(expanded_frame, target_list_frameinfo->ID3v2_Frame_ExpandedLength);
			
			frame_ptr = expanded_frame;
			frameLen = target_list_frameinfo->ID3v2_Frame_ExpandedLength;
#else
			target_list_frameinfo->ID3v2_FrameType = ID3_UNKNOWN_FRAME;
			frame_ptr = fulltag_ptr+frame_offset;
			frameLen = target_list_frameinfo->ID3v2_Frame_ExpandedLength;
#endif
		} else {

			frame_ptr = fulltag_ptr+frame_offset;
			frameLen = target_list_frameinfo->ID3v2_Frame_Length; 
		}
		
		APar_ScanID3Frame(target_list_frameinfo, frame_ptr, frameLen);
		
		if (expanded_frame != NULL) {
			free(expanded_frame);
			expanded_frame = NULL;
		}

		if (target_list_frameinfo != NULL) {
			if (id32_atom->ID32_TagInfo->ID3v2_FrameCount == 0) {
				id32_atom->ID32_TagInfo->ID3v2_FirstFrame = target_list_frameinfo; //entrance to the linked list
			}
			id32_atom->ID32_TagInfo->ID3v2_FrameList = target_list_frameinfo; //this always points to the last frame that had the scan completed
		}
		
		if (fullframesize != 0) {
			fulltag_ptr+= fullframesize;
		} else {
			fulltag_ptr+= target_list_frameinfo->ID3v2_Frame_Length;
		}
		
		id32_atom->ID32_TagInfo->ID3v2_FrameCount++;
	}
	
	id32_atom->ID32_TagInfo->modified_tag = false; //if a frame is altered/added/removed, change this to true and render the tag & fill id32_atom-AtomicData with the tag
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                         id3 rendering functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_FrameFilter(AtomicInfo* id32_atom) {
	ID3v2Frame* MCDI_frame = NULL;
	ID3v2Frame* TRCK_frame = NULL;
	ID3v2Frame* thisFrame = id32_atom->ID32_TagInfo->ID3v2_FirstFrame;
	while (thisFrame != NULL) {
		if (!thisFrame->eliminate_frame) {
			if (thisFrame->ID3v2_FrameType == ID3_CD_ID_FRAME) {
				MCDI_frame = thisFrame;
			}
			if (thisFrame->ID3v2_Frame_ID == ID3v2_FRAME_TRACKNUM) {
				TRCK_frame = thisFrame;
			}
		}
		thisFrame = thisFrame->ID3v2_NextFrame;
	}
	
	if (MCDI_frame != NULL && TRCK_frame == NULL) {
		fprintf(stderr, "AP warning: the MCDI frame was skipped due to a missing TRCK frame\n");
		MCDI_frame->eliminate_frame = true;
	}
	return;
}

uint32_t APar_GetTagSize(AtomicInfo* id32_atom) { // a rough approximation of how much to malloc; this will be larger than will be ultimately required
	uint32_t tag_len = 0;
	uint16_t surviving_frame_count = 0;
	if (id32_atom->ID32_TagInfo->modified_tag = false) return tag_len;
	if (id32_atom->ID32_TagInfo->ID3v2_FrameCount == 0) return tag_len; //but a frame isn't removed by AP; its just marked for elimination
	if (id32_atom->ID32_TagInfo->ID3v2_FrameList == NULL) return tag_len; //something went wrong somewhere if this wasn't an entry to a linked list of frames
	if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion != 4) return tag_len; //only id3 version 2.4 tags are written
	
	ID3v2Frame* eval_frame = id32_atom->ID32_TagInfo->ID3v2_FirstFrame;
	while (eval_frame != NULL) {
		if (eval_frame->eliminate_frame == true)  {
			eval_frame = eval_frame->ID3v2_NextFrame;
			continue;
		}
		tag_len += 15; //4bytes frameID 'TCON', 4bytes frame length (syncsafe int), 2 bytes frame flags; optional group symbol: 1byte + decompressed length 4bytes
		tag_len += 2*eval_frame->ID3v2_FieldCount; //excess amount to ensure that text fields have utf16 BOMs & 2 byte NULL terminations as required
		if (ID3v2_TestFrameFlag(eval_frame->ID3v2_Frame_Flags, ID32_FRAMEFLAG_COMPRESSED)) {
			tag_len += eval_frame->ID3v2_Frame_ExpandedLength;
		} else {
			tag_len += eval_frame->ID3v2_Frame_Length;
		}
		surviving_frame_count++;
		eval_frame = eval_frame->ID3v2_NextFrame;
		if (surviving_frame_count == 0 && eval_frame == NULL) {
		}
	}
	if (surviving_frame_count == 0) return 0; //the 'ID3' header alone isn't going to be written with 0 existing frames
	return tag_len;
}

void APar_RenderFields(char* dest_buffer, uint32_t max_alloc, ID3v2Tag* id3_tag, ID3v2Frame* id3v2_frame, uint32_t* frame_header_len, uint32_t* frame_length) {
	if (id3v2_frame->ID3v2_Frame_Fields == NULL) {
		*frame_header_len = 0;
		*frame_length = 0;
		return;
	}
	
	for (uint8_t fld_idx = 0; fld_idx < id3v2_frame->ID3v2_FieldCount; fld_idx++) {
		ID3v2Fields* this_field = id3v2_frame->ID3v2_Frame_Fields+fld_idx;
		//fprintf(stdout, "Total Fields for %s: %u (this is %u, %u)\n", id3v2_frame->ID3v2_Frame_Namestr, id3v2_frame->ID3v2_FieldCount, fld_idx, this_field->ID3v2_Field_Type);
		switch(this_field->ID3v2_Field_Type) {
			
			//these are raw data fields of variable/fixed length and are not NULL terminated
			case ID3_UNKNOWN_FIELD : 
			case ID3_PIC_TYPE_FIELD :
			case ID3_GROUPSYMBOL_FIELD :
			case ID3_TEXT_ENCODING_FIELD :
			case ID3_LANGUAGE_FIELD :
			case ID3_COUNTER_FIELD :
			case ID3_IMAGEFORMAT_FIELD :
			case ID3_URL_FIELD :
			case ID3_BINARY_DATA_FIELD : {
				APar_LimitBufferRange(max_alloc, *frame_header_len + *frame_length);
				if (this_field->field_string != NULL) {
					memcpy(dest_buffer + *frame_length, this_field->field_string, this_field->field_length);
					*frame_length += this_field->field_length;
					//fprintf(stdout, "Field idx %u(%d) is now %u bytes long (+%u)\n", fld_idx, this_field->ID3v2_Field_Type, *frame_length, this_field->field_length);
				}
				break;
			}
			
			//these fields are subject to NULL byte termination - based on what the text encoding field says the encoding of this string is
			case ID3_TEXT_FIELD :
			case ID3_FILENAME_FIELD :
			case ID3_DESCRIPTION_FIELD : {
				if (this_field->field_string == NULL) {
					*frame_header_len = 0;
					*frame_length = 0;
					return;
				} else {
					APar_LimitBufferRange(max_alloc, *frame_header_len + *frame_length +2); //+2 for a possible extra NULLs
					uint8_t encoding_val = id3v2_frame->ID3v2_Frame_Fields->field_string[0]; //ID3_TEXT_ENCODING_FIELD is always the first field, and should have an encoding
					if ( (id3_tag->ID3v2Tag_MajorVersion == 4 && encoding_val == TE_UTF8) || encoding_val == TE_LATIN1 ) {
						if (this_field->ID3v2_Field_Type != ID3_TEXT_FIELD) APar_ValidateNULLTermination8bit(this_field);
						
						memcpy(dest_buffer + *frame_length, this_field->field_string, this_field->field_length);
						*frame_length += this_field->field_length;

					} else if ((id3_tag->ID3v2Tag_MajorVersion == 4 && encoding_val == TE_UTF16LE_WITH_BOM) || encoding_val == TE_UTF16BE_NO_BOM) {
						APar_ValidateNULLTermination16bit(this_field, encoding_val);
						
						memcpy(dest_buffer + *frame_length, this_field->field_string, this_field->field_length);
						*frame_length += this_field->field_length;
					
					} else { //well, AP didn't set this frame, so just duplicate it.
						memcpy(dest_buffer + *frame_length, this_field->field_string, this_field->field_length);
						*frame_length += this_field->field_length;
					}
				}
				//fprintf(stdout, "Field idx %u(%d) is now %u bytes long\n", fld_idx, this_field->ID3v2_Field_Type, *frame_length);
				break;
			}
			
			//these are iso 8859-1 encoded with a single NULL terminator
			//a 'LINK' url would also come here and be seperately enumerated (because it has a terminating NULL); but in 3gp assets, external references aren't allowed
			//an 'OWNE'/'COMR' price field would also be here because of single byte NULL termination
			case ID3_OWNER_FIELD :
			case ID3_MIME_TYPE_FIELD : {
				if (this_field->field_string == NULL) {
					*frame_header_len = 0;
					*frame_length = 0;
					return;
				} else {
					APar_LimitBufferRange(max_alloc, *frame_header_len + *frame_length +1); //+2 for a possible extra NULLs

					APar_ValidateNULLTermination8bit(this_field);
					memcpy(dest_buffer + *frame_length, this_field->field_string, this_field->field_length);
					*frame_length += this_field->field_length;

				}
				//fprintf(stdout, "Field idx %u(%d) is now %u bytes long\n", fld_idx, this_field->ID3v2_Field_Type, *frame_length);
				break;
			}
			default : {
				//fprintf(stdout, "I was unable to determine the field class. I was provided with %u (i.e. text field: %u, text encoding: %u\n", this_field->ID3v2_Field_Type, ID3_TEXT_FIELD, ID3_TEXT_ENCODING_FIELD);
				break;
			}
		
		} //end switch
	}
	return;
}

uint32_t APar_Render_ID32_Tag(AtomicInfo* id32_atom, uint32_t max_alloc) {
	bool contains_rendered_frames = false;
	APar_FrameFilter(id32_atom);
	
	UInt16_TO_String2(id32_atom->AtomicLanguage, id32_atom->AtomicData); //parsedAtoms[atom_idx].AtomicLanguage
	uint32_t tag_offset = 2; //those first 2 bytes will hold the language
	uint32_t frame_length, frame_header_len; //the length in bytes this frame consumes in AtomicData as rendered
	uint32_t frame_length_pos, frame_compressed_length_pos;
	
	memcpy(id32_atom->AtomicData + tag_offset, "ID3", 3);
	tag_offset+=3;
	
	id32_atom->AtomicData[tag_offset] = id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion; //should be 4
	id32_atom->AtomicData[tag_offset+1] = id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion; //should be 0
	id32_atom->AtomicData[tag_offset+2] = id32_atom->ID32_TagInfo->ID3v2Tag_Flags;
	tag_offset+=3;
	
	//unknown full length; fill in later
	if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3 || id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
		tag_offset+= 4;
		if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_EXTENDEDHEADER)) { //currently unimplemented
			tag_offset+=10;
		}
	} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 2) {
		tag_offset+= 3;
	}
	
	id32_atom->ID32_TagInfo->ID3v2Tag_Length = tag_offset-2;
	
	ID3v2Frame* thisframe = id32_atom->ID32_TagInfo->ID3v2_FirstFrame;
	while (thisframe != NULL) {
		frame_header_len = 0;
		frame_length_pos = 0;
		frame_compressed_length_pos = 0;
		
		if (thisframe->eliminate_frame == true) {
			thisframe = thisframe->ID3v2_NextFrame;
			continue;
		}
		
		contains_rendered_frames = true;
		//this won't be able to convert from 1 tag version to another because it doesn't look up the frame id strings for the change
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3 || id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
			memcpy(id32_atom->AtomicData + tag_offset, thisframe->ID3v2_Frame_Namestr, 4);
			frame_header_len += 4;
			
			//the frame length won't be determined until the end of rendering this frame fully; for now just remember where its supposed to be:
			frame_length_pos = tag_offset + frame_header_len;
			frame_header_len+=4;
			
		} else if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 2) {
			memcpy(id32_atom->AtomicData + tag_offset, thisframe->ID3v2_Frame_Namestr, 3);
			frame_header_len += 3;
			
			//the frame length won't be determined until the end of rendering this frame fully; for now just remember where its supposed to be:
			frame_length_pos = tag_offset + frame_header_len;
			frame_header_len+=3;
		}
		
		//render frame flags //TODO: compression & group symbol are the only ones that can possibly be set here
		if (id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 3 || id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion == 4) {
			UInt16_TO_String2(thisframe->ID3v2_Frame_Flags, id32_atom->AtomicData + tag_offset + frame_header_len);
			frame_header_len+=2;
		}
		
		//grouping flag? 1 byte; technically, its outside the header and before the fields begin
		if (ID3v2_TestFrameFlag(thisframe->ID3v2_Frame_Flags, ID32_FRAMEFLAG_GROUPING)) {
			id32_atom->AtomicData[tag_offset + frame_header_len] = thisframe->ID3v2_Frame_GroupingSymbol;
			frame_header_len++;
		}
		
		//compression flag? 4bytes; technically, its outside the header and before the fields begin
		if (ID3v2_TestFrameFlag(thisframe->ID3v2_Frame_Flags, ID32_FRAMEFLAG_COMPRESSED)) {
			frame_compressed_length_pos = tag_offset + frame_header_len; //fill in later; remember where it is supposed to go
			frame_header_len+=4;
		}
		
		frame_length = 0;
		
		APar_RenderFields(id32_atom->AtomicData + tag_offset+frame_header_len, max_alloc-tag_offset, id32_atom->ID32_TagInfo, thisframe, &frame_header_len, &frame_length);
		
#if defined HAVE_ZLIB_H
		//and now that we have rendered the frame, its time to turn to compression, if set
		if (ID3v2_TestFrameFlag(thisframe->ID3v2_Frame_Flags, ID32_FRAMEFLAG_COMPRESSED) ) {
			uint32_t compressed_len = 0;
			char* compress_buffer = (char*)calloc(1, sizeof(char)*frame_length + 20);
			
			compressed_len = APar_zlib_deflate(id32_atom->AtomicData + tag_offset+frame_header_len, frame_length, compress_buffer, frame_length + 20);
			
			if (compressed_len > 0) {
				memcpy(id32_atom->AtomicData + tag_offset+frame_header_len, compress_buffer, compressed_len + 1);
				convert_to_syncsafe32(frame_length, id32_atom->AtomicData + frame_compressed_length_pos);
				frame_length = compressed_len;

				//WriteZlibData(id32_atom->AtomicData + tag_offset+frame_header_len, compressed_len);
			}
		}
#endif
		
		convert_to_syncsafe32(frame_length, id32_atom->AtomicData + frame_length_pos);
		tag_offset += frame_header_len + frame_length; //advance
		id32_atom->ID32_TagInfo->ID3v2Tag_Length += frame_header_len + frame_length;
		thisframe = thisframe->ID3v2_NextFrame;
		
	}
	convert_to_syncsafe32(id32_atom->ID32_TagInfo->ID3v2Tag_Length - 10, id32_atom->AtomicData + 8); //-10 for a v2.4 tag with no extended header
	
	if (!contains_rendered_frames) id32_atom->ID32_TagInfo->ID3v2Tag_Length = 0;

	return id32_atom->ID32_TagInfo->ID3v2Tag_Length;
}

///////////////////////////////////////////////////////////////////////////////////////
//                       id3 initializing functions                                  //
///////////////////////////////////////////////////////////////////////////////////////

void APar_FieldInit(ID3v2Frame* aFrame, uint8_t a_field, uint8_t frame_comp_list, char* frame_payload) {
	uint32_t byte_allocation = 0;
	ID3v2Fields* this_field = NULL;
	int field_type = FrameTypeConstructionList[frame_comp_list].ID3_FieldComponents[a_field];
	
	switch(field_type) {
		//case ID3_UNKNOWN_FIELD will not be handled
		
		//these are all 1 to less than 16 bytes.
		case ID3_GROUPSYMBOL_FIELD :
		case ID3_COUNTER_FIELD :
		case ID3_PIC_TYPE_FIELD :
		case ID3_LANGUAGE_FIELD :
		case ID3_IMAGEFORMAT_FIELD : //PIC in v2.2
		case ID3_TEXT_ENCODING_FIELD : {
			byte_allocation = 16;
			break;
		}
		
		//between 16 & 100 bytes.
		case ID3_MIME_TYPE_FIELD : {
			byte_allocation = 100;
			break;
		}
		
		//these are allocated with 2000 bytes
		case ID3_FILENAME_FIELD :
		case ID3_OWNER_FIELD :
		case ID3_DESCRIPTION_FIELD :
		case ID3_URL_FIELD :
		case ID3_TEXT_FIELD : {
			uint32_t string_len = strlen(frame_payload) + 1;
			if (string_len * 2 > 2000) {
				byte_allocation = string_len * 2;
			} else {
				byte_allocation = 2000;
			}
			break;
		}
		
		case ID3_BINARY_DATA_FIELD : {
			if (aFrame->ID3v2_Frame_ID == ID3v2_EMBEDDED_PICTURE || aFrame->ID3v2_Frame_ID == ID3v2_EMBEDDED_OBJECT ) {
				//this will be left NULL because it would would probably have to be realloced, so just do it later to the right size //byte_allocation = (uint32_t)findFileSize(frame_payload) + 1; //this should be limited to max_sync_safe_uint28_t
			} else {
				byte_allocation = 2000;
			}
			break;
		}
	
		//default : {
		//	fprintf(stdout, "I am %d\n", FrameTypeConstructionList[frame_comp_list].ID3_FieldComponents[a_field]);
		//	break;
		//}
	}
	//if (byte_allocation > 0) {
		this_field = aFrame->ID3v2_Frame_Fields + a_field;
		this_field->ID3v2_Field_Type = field_type;
		if (byte_allocation > 0) {
			this_field->field_string = (char*)calloc(1, sizeof(char*)*byte_allocation);
			if (!APar_assert((this_field->field_string != NULL), 11, aFrame->ID3v2_Frame_Namestr) ) exit(11);
		} else {
			this_field->field_string = NULL;
		}
		this_field->field_length = 0;
		this_field->alloc_length = byte_allocation;
		//fprintf(stdout, "For %u field, %u bytes were allocated.\n", this_field->ID3v2_Field_Type, byte_allocation);
	//}
	return;
}

void APar_FrameInit(ID3v2Frame* aFrame, char* frame_str, uint16_t frameID, uint8_t frame_comp_list, char* frame_payload) {
	aFrame->ID3v2_FieldCount = FrameTypeConstructionList[frame_comp_list].ID3_FieldCount;
	if (aFrame->ID3v2_FieldCount > 0) {
		aFrame->ID3v2_Frame_Fields = (ID3v2Fields*)calloc(1, sizeof(ID3v2Fields)*aFrame->ID3v2_FieldCount);
		aFrame->ID3v2_Frame_ID = frameID - 1;
		aFrame->ID3v2_FrameType = FrameTypeConstructionList[frame_comp_list].ID3_FrameType;
		aFrame->ID3v2_Frame_ExpandedLength = 0;
		aFrame->ID3v2_Frame_GroupingSymbol = 0;
		aFrame->ID3v2_Frame_Flags = 0;
		aFrame->ID3v2_Frame_Length = 0;
		aFrame->eliminate_frame = false;
		memcpy(aFrame->ID3v2_Frame_Namestr, frame_str, 5);
		
		for (uint8_t fld = 0; fld < aFrame->ID3v2_FieldCount; fld++) {
			APar_FieldInit(aFrame, fld, frame_comp_list, frame_payload);
		}
		
		//fprintf(stdout, "(%u = %d) Type %d\n", frameID, KnownFrames[frameID+1].ID3v2_InternalFrameID, aFrame->ID3v2_FrameType);
	}
	//fprintf(stdout, "Retrieved frame for '%s': %s (%u fields)\n", frame_str, KnownFrames[frameID].ID3V2p4_FrameID, aFrame->ID3v2_FieldCount);
	return;
}

void APar_ID3Tag_Init(AtomicInfo* id32_atom) {
	id32_atom->ID32_TagInfo = (ID3v2Tag*)calloc(1, sizeof(ID3v2Tag));
	id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion = AtomicParsley_ID3v2Tag_MajorVersion;
	id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion = AtomicParsley_ID3v2Tag_RevisionVersion;
	id32_atom->ID32_TagInfo->ID3v2Tag_Flags = AtomicParsley_ID3v2Tag_Flags;
	id32_atom->ID32_TagInfo->ID3v2Tag_Length = 10; //this would be 9 for v2.2
	id32_atom->ID32_TagInfo->ID3v2_Tag_ExtendedHeader_Length = 0;
	id32_atom->ID32_TagInfo->ID3v2_FrameCount = 0;
	id32_atom->ID32_TagInfo->modified_tag = false; //this will have to change when a frame is added/modified/removed because this id3 header won't be written with 0 frames
	
	id32_atom->ID32_TagInfo->ID3v2_FirstFrame = NULL;
	id32_atom->ID32_TagInfo->ID3v2_FrameList = NULL;
	return;
}

void APar_realloc_memcpy(ID3v2Fields* thisField, uint32_t new_size) {
	if (new_size > thisField->alloc_length) {
		char* new_alloc = (char*)calloc(1, sizeof(char*)*new_size + 1);
		//memcpy(new_alloc, thisField->field_string, thisField->field_length);
		thisField->field_length = 0;
		free(thisField->field_string);
		thisField->field_string = new_alloc;
		thisField->alloc_length = new_size;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                    id3 frame setting/finding functions                            //
///////////////////////////////////////////////////////////////////////////////////////

uint32_t APar_TextFieldDataPut(ID3v2Fields* thisField, char* this_payload, uint8_t str_encoding, bool multistringtext = false) {
	if (multistringtext == false) thisField->field_length = 0;
	uint32_t bytes_used = 0;
	
	if (str_encoding == TE_UTF8) {
		bytes_used = strlen(this_payload); //no NULL termination is provided until render time
		if (bytes_used + thisField->field_length > thisField->alloc_length) {
			APar_realloc_memcpy(thisField, (bytes_used > 2000 ? bytes_used : 2000) );
		}
		if (thisField->field_length > 0) {
			//to accommodate id3v2.4's "multiple string" hack (NULL separators):
			//add sufficient length to thisField->field_length to accommodate a unicode NULL (1 or 2 bytes based on encoding).
			//requires testing the last few bytes to see if it contains just a BOM/NULL pair or something else
			//requires addressing the printing of such fields as well
			//...and only on v2.4 is this allowed.
		}
		memcpy(thisField->field_string + thisField->field_length, this_payload, bytes_used);
		thisField->field_length += bytes_used;
		
	} else if (str_encoding == TE_LATIN1) {
		int string_length = strlen(this_payload);
		if ((uint32_t)string_length + thisField->field_length > thisField->alloc_length) {
			APar_realloc_memcpy(thisField, (string_length > 2000 ? string_length : 2000) );
		}
		int converted_bytes = UTF8Toisolat1((unsigned char*)thisField->field_string + thisField->field_length, (int)thisField->alloc_length,
		                                   (unsigned char*)this_payload, string_length);
		if (converted_bytes > 0) {
			thisField->field_length += (uint32_t)converted_bytes;
			bytes_used = converted_bytes;
			//fprintf(stdout, "string %s, %u=%u\n", thisField->field_string, thisField->field_length, bytes_used);
		}
		
	} else if (str_encoding == TE_UTF16BE_NO_BOM) {
		int string_length = (int)utf8_length(this_payload, strlen(this_payload)) + 1;
		if (2 * (uint32_t)string_length + thisField->field_length > thisField->alloc_length) {
			APar_realloc_memcpy(thisField, (2 * (uint32_t)string_length + thisField->field_length > 2000 ? 2 * (uint32_t)string_length + thisField->field_length : 2000) );
		}
		int converted_bytes = UTF8ToUTF16BE((unsigned char*)thisField->field_string + thisField->field_length, (int)thisField->alloc_length,
		                                   (unsigned char*)this_payload, string_length);
		if (converted_bytes > 0) {
			thisField->field_length += (uint32_t)converted_bytes;
			bytes_used = converted_bytes;
		}
	
	} else if (str_encoding == TE_UTF16LE_WITH_BOM) {
		int string_length = (int)utf8_length(this_payload, strlen(this_payload)) + 1;
		uint32_t bom_offset = 0;
		
		if (2 * (uint32_t)string_length + thisField->field_length > thisField->alloc_length) { //important: realloc before BOM testing!!!
			APar_realloc_memcpy(thisField, (2 * (uint32_t)string_length + thisField->field_length > 2000 ? 2 * (uint32_t)string_length + thisField->field_length : 2000) );
		}
		if (thisField->field_length == 0) {
			memcpy(thisField->field_string, "\xFF\xFE", 2);
		}
		
		uint8_t field_encoding = TextField_TestBOM(thisField->field_string);
		if (field_encoding > 0) {
			bom_offset = 2;
		}
		int converted_bytes = UTF8ToUTF16LE((unsigned char*)thisField->field_string + thisField->field_length + bom_offset, (int)thisField->alloc_length,
		                                   (unsigned char*)this_payload, string_length);
		if (converted_bytes > 0) {
			thisField->field_length += (uint32_t)converted_bytes + bom_offset;
			bytes_used = converted_bytes;
		}
	}
	return bytes_used;
}

uint32_t APar_BinaryFieldPut(ID3v2Fields* thisField, uint32_t a_number, char* this_payload, uint32_t payload_len) {
	if (thisField->ID3v2_Field_Type == ID3_TEXT_ENCODING_FIELD || thisField->ID3v2_Field_Type == ID3_PIC_TYPE_FIELD) {
		thisField->field_string[0] = (unsigned char)a_number;
		thisField->field_length = 1;
		//fprintf(stdout, "My (TE/PT) content is 0x%02X\n", thisField->field_string[0]);
		return 1;
		
	} else if (thisField->ID3v2_Field_Type == ID3_BINARY_DATA_FIELD && payload_len == 0) { //contents of a file
		uint32_t file_length = (uint32_t)findFileSize(this_payload);
		thisField->field_string = (char*)calloc(1, sizeof(char*)*file_length+16);
		
		FILE* binfile = APar_OpenFile(this_payload, "rb");
		APar_ReadFile(thisField->field_string, binfile, file_length);
		fclose(binfile);
		
		thisField->field_length = file_length;
		thisField->alloc_length = file_length+16;
		thisField->ID3v2_Field_Type = ID3_BINARY_DATA_FIELD;
		return file_length;
	
	} else if (thisField->ID3v2_Field_Type == ID3_BINARY_DATA_FIELD) {
		thisField->field_string = (char*)calloc(1, sizeof(char*)*payload_len+16);
		memcpy(thisField->field_string, this_payload, payload_len);
		
		thisField->field_length = payload_len;
		thisField->alloc_length = payload_len+16;
		thisField->ID3v2_Field_Type = ID3_BINARY_DATA_FIELD;
		return payload_len;

	}
	return 0;
}

void APar_FrameDataPut(ID3v2Frame* thisFrame, char* frame_payload, AdjunctArgs* adjunct_payload, uint8_t str_encoding) {
	if (adjunct_payload->multistringtext == false) thisFrame->ID3v2_Frame_Length = 0;
	switch(thisFrame->ID3v2_FrameType) {
		case ID3_TEXT_FRAME : {
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, 1); //encoding
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+1, frame_payload, str_encoding, adjunct_payload->multistringtext); //text field
			//fprintf(stdout, "I am of text type %u\n", str_encoding);
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_TEXT_FRAME_USERDEF : {
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, 1); //encoding
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+1, adjunct_payload->descripArg, str_encoding); //language
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+2, frame_payload, str_encoding); //text field
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_DESCRIBED_TEXT_FRAME : {
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, 1); //encoding
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+1, adjunct_payload->targetLang, 0); //language
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+2, adjunct_payload->descripArg, str_encoding); //description
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+3, frame_payload, str_encoding, adjunct_payload->multistringtext); //text field
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_URL_FRAME : {
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields, frame_payload, TE_LATIN1); //url field
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_URL_FRAME_USERDEF : {
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, 1); //encoding
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+1, adjunct_payload->descripArg, str_encoding); //language
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+2, frame_payload, TE_LATIN1); //url field
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_UNIQUE_FILE_ID_FRAME : {
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields, frame_payload, TE_LATIN1); //owner field
			
			if (memcmp(adjunct_payload->uniqIDArg, "randomUUIDstamp", 16) == 0) {
				char uuid_binary_str[25]; memset(uuid_binary_str, 0, 25);
				APar_generate_random_uuid(uuid_binary_str);
				(thisFrame->ID3v2_Frame_Fields+1)->field_string = (char*)calloc(1, sizeof(char*)*40);
				APar_sprintf_uuid((ap_uuid_t*)uuid_binary_str, (thisFrame->ID3v2_Frame_Fields+1)->field_string);
		
				(thisFrame->ID3v2_Frame_Fields+1)->field_length = 36;
				(thisFrame->ID3v2_Frame_Fields+1)->alloc_length = 40;
				(thisFrame->ID3v2_Frame_Fields+1)->ID3v2_Field_Type = ID3_BINARY_DATA_FIELD;
				thisFrame->ID3v2_Frame_Length += 36;
			} else {
				uint8_t uniqueIDlen = strlen(adjunct_payload->uniqIDArg);
				thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, (uniqueIDlen > 64 ? 64 : uniqueIDlen)); //unique file ID
			}

			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_CD_ID_FRAME : {
			thisFrame->ID3v2_Frame_Fields->field_length = GenerateMCDIfromCD(frame_payload, thisFrame->ID3v2_Frame_Fields->field_string);
			thisFrame->ID3v2_Frame_Length = thisFrame->ID3v2_Frame_Fields->field_length;
			
			if (thisFrame->ID3v2_Frame_Length < 12) {
				free(thisFrame->ID3v2_Frame_Fields->field_string);
				thisFrame->ID3v2_Frame_Fields->field_string = NULL;
				thisFrame->ID3v2_Frame_Fields->alloc_length = 0;
				thisFrame->ID3v2_Frame_Length = 0;
			} else {
				modified_atoms = true;
				GlobalID3Tag->modified_tag = true;
				GlobalID3Tag->ID3v2_FrameCount++;
			}
			break;
		}
		case ID3_ATTACHED_PICTURE_FRAME : {
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields, str_encoding, NULL, 1); //encoding
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+1, adjunct_payload->mimeArg, TE_LATIN1); //mimetype
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields+2, adjunct_payload->pictype_uint8, NULL, 1); //picturetype
			thisFrame->ID3v2_Frame_Length += APar_TextFieldDataPut(thisFrame->ID3v2_Frame_Fields+3, adjunct_payload->descripArg, str_encoding); //description
			//(thisFrame->ID3v2_Frame_Fields+4)->ID3v2_Field_Type = ID3_BINARY_DATA_FIELD; //because it wasn't malloced, this needs to be set now
			thisFrame->ID3v2_Frame_Length += APar_BinaryFieldPut(thisFrame->ID3v2_Frame_Fields+4, 0, frame_payload, 0); //binary file (path)
			modified_atoms = true;
			GlobalID3Tag->modified_tag = true;
			GlobalID3Tag->ID3v2_FrameCount++;
			break;
		}
		case ID3_ATTACHED_OBJECT_FRAME : {
			break;
		}
		case ID3_GROUP_ID_FRAME : {
			break;
		}
		case ID3_SIGNATURE_FRAME : {
			break;
		}
		case ID3_PRIVATE_FRAME : {
			break;
		}
	} //end switch
	return;
}

void APar_EmbeddedFileTests(char* filepath, int frameType, AdjunctArgs* adjunct_payloads) {
	if (frameType == ID3_ATTACHED_PICTURE_FRAME) {
		
		//get cli imagetype
		uint8_t total_image_types = (uint8_t)(sizeof(ImageTypeList)/sizeof(*ImageTypeList));
		uint8_t img_typlen = strlen(adjunct_payloads->pictypeArg) + 1;
		char* img_comparison_str = NULL;
		
		for (uint8_t itest = 0; itest < total_image_types; itest++) {
			if (img_typlen == 5) {
				img_comparison_str = ImageTypeList[itest].hexstring;
			} else {
				img_comparison_str = ImageTypeList[itest].imagetype_str;
			}
			if (memcmp(adjunct_payloads->pictypeArg, img_comparison_str, img_typlen) == 0) {
				adjunct_payloads->pictype_uint8 = ImageTypeList[itest].hexcode;
			}
		}

		if (strlen(filepath) > 0) {
			//see if file even exists
			TestFileExistence(filepath, true);
			
			char* image_headerbytes = (char*)calloc(1, (sizeof(char)*25));
			FILE* imagefile =  APar_OpenFile(filepath, "rb");
			APar_ReadFile(image_headerbytes, imagefile, 24);
			fclose(imagefile);
			//test mimetype
			if (strlen(adjunct_payloads->mimeArg) == 0 || memcmp(adjunct_payloads->mimeArg, "-->", 3) == 0) {
				uint8_t total_image_tests = (uint8_t)(sizeof(ImageList)/sizeof(*ImageList));
				for (uint8_t itest = 0; itest < total_image_tests; itest++) {
					if (ImageList[itest].image_testbytes == 0) {
						adjunct_payloads->mimeArg = ImageList[itest].image_mimetype;
						break;
					} else if (memcmp(image_headerbytes, ImageList[itest].image_binaryheader, ImageList[itest].image_testbytes) == 0) {
						adjunct_payloads->mimeArg = ImageList[itest].image_mimetype;
						if (adjunct_payloads->pictype_uint8 == 0x01) {
							if (memcmp(image_headerbytes+16, "\x00\x00\x00\x20\x00\x00\x00\x20", 8) != 0 && itest != 2) {
								adjunct_payloads->pictype_uint8 = 0x02;
							}
						}
						break;
					}
				}
			}
			free(image_headerbytes);
			image_headerbytes = NULL;
		}
		
	} else if (frameType == ID3_ATTACHED_OBJECT_FRAME) {
		if (strlen(filepath) > 0) {
			TestFileExistence(filepath, true);			
			FILE* embedfile =  APar_OpenFile(filepath, "rb");
			fclose(embedfile);
		}
	}
	return;
}

char* APar_ConvertField_to_UTF8(ID3v2Frame* targetframe, int fieldtype) {
	char* utf8str = NULL;
	uint8_t targetfield = 0xFF;
	uint8_t textencoding = 0;
	uint32_t utf8maxalloc = 0;
	
	for (uint8_t frm_field = 0; frm_field < targetframe->ID3v2_FieldCount; frm_field++) {
		if ( (targetframe->ID3v2_Frame_Fields+frm_field)->ID3v2_Field_Type == fieldtype) {
			targetfield = frm_field;
			break;
		}
	}
	
	if (targetfield != 0xFF) {
		if (targetframe->ID3v2_Frame_Fields->ID3v2_Field_Type == ID3_TEXT_ENCODING_FIELD) {
			textencoding = targetframe->ID3v2_Frame_Fields->field_string[0];
		}
		
		if (textencoding == TE_LATIN1) {
			utf8str = (char*)calloc(1, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *2) +16);
			isolat1ToUTF8((unsigned char*)utf8str, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *2) +16,
			              (unsigned char*)((targetframe->ID3v2_Frame_Fields+targetfield)->field_string), (targetframe->ID3v2_Frame_Fields+targetfield)->field_length);

		} else if (textencoding == TE_UTF8) { //just so things can be free()'d with testing; a small price to pay
			utf8str = (char*)calloc(1, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length) +16);
			memcpy(utf8str, (targetframe->ID3v2_Frame_Fields+targetfield)->field_string, (targetframe->ID3v2_Frame_Fields+targetfield)->field_length);
			
		} else if (textencoding == TE_UTF16BE_NO_BOM) {
			utf8str = (char*)calloc(1, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *4) +16);
			UTF16BEToUTF8((unsigned char*)utf8str, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *4) +16,
			              (unsigned char*)((targetframe->ID3v2_Frame_Fields+targetfield)->field_string), (targetframe->ID3v2_Frame_Fields+targetfield)->field_length);
			
		} else if (textencoding == TE_UTF16LE_WITH_BOM) {
			utf8str = (char*)calloc(1, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *4) +16);
			if (memcmp( (targetframe->ID3v2_Frame_Fields+targetfield)->field_string, "\xFF\xFE", 2) == 0) {
				UTF16LEToUTF8((unsigned char*)utf8str, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *4) +16,
			                (unsigned char*)((targetframe->ID3v2_Frame_Fields+targetfield)->field_string+2), (targetframe->ID3v2_Frame_Fields+targetfield)->field_length-2);
			} else {
				UTF16BEToUTF8((unsigned char*)utf8str, sizeof(char*)*( (targetframe->ID3v2_Frame_Fields+targetfield)->field_length *4) +16,
			                (unsigned char*)((targetframe->ID3v2_Frame_Fields+targetfield)->field_string+2), (targetframe->ID3v2_Frame_Fields+targetfield)->field_length-2);
			}
		}
	}
	
	return utf8str;
}

ID3v2Frame* APar_FindFrame(ID3v2Tag* id3v2tag, char* frame_str, uint16_t frameID, int frametype, AdjunctArgs* adjunct_payloads, bool createframe) {
	ID3v2Frame* returnframe = NULL;
	ID3v2Frame* evalframe = id3v2tag->ID3v2_FirstFrame;
	uint8_t supplemental_matching = 0;
	
	if (createframe) { //the end of scanned/new frames
		ID3v2Frame* newframe = (ID3v2Frame*)calloc(1, sizeof(ID3v2Frame));
		newframe->ID3v2_NextFrame = NULL;
		if (id3v2tag->ID3v2_FirstFrame == NULL) id3v2tag->ID3v2_FirstFrame = newframe;
		if (id3v2tag->ID3v2_FrameList != NULL) id3v2tag->ID3v2_FrameList->ID3v2_NextFrame = newframe;
		id3v2tag->ID3v2_FrameList = newframe;
		returnframe = newframe;
		return returnframe;
	}
	
	if (APar_EvalFrame_for_Field(frametype, ID3_DESCRIPTION_FIELD)) {
		supplemental_matching = 0x01;
	}

	while (evalframe != NULL) {
		//if (trametype is a type containing a modifer like description or image type or symbol or such things
		if (supplemental_matching != 0) {
		
			//match on description + frame name
			if (supplemental_matching && 0x01 && evalframe->ID3v2_Frame_ID == frameID) {
				char* utf8_descrip = APar_ConvertField_to_UTF8(evalframe, ID3_DESCRIPTION_FIELD);
				if (utf8_descrip != NULL) {
					if (memcmp(adjunct_payloads->descripArg, utf8_descrip, strlen(adjunct_payloads->descripArg)) == 0) {
						returnframe = evalframe;
						free(utf8_descrip);
						break;
					}
					free(utf8_descrip);
				}
			}
		
		} else if (evalframe->ID3v2_Frame_ID == ID3_UNKNOWN_FRAME) {
			if (memcmp(frame_str, evalframe->ID3v2_Frame_Namestr, 4) == 0) {
				returnframe = evalframe;
				break;
			}
		
		} else {
			if (evalframe->ID3v2_Frame_ID == frameID) {
				returnframe = evalframe;
				break;
			}
		}
		evalframe = evalframe->ID3v2_NextFrame;
	}
	return returnframe;
}

void APar_ID3FrameAmmend(short id32_atom_idx, char* frame_str, char* frame_payload, AdjunctArgs* adjunct_payloads, uint8_t str_encoding) {
	ID3v2Frame* targetFrame = NULL;
	ID3v2Frame* eval_frame = NULL;
	GlobalID3Tag = parsedAtoms[id32_atom_idx].ID32_TagInfo;
	//fprintf(stdout, "frame is %s; payload is %s; %s %s\n", frame_str, frame_payload, adjunct_payloads->descripArg, adjunct_payloads->targetLang);
	if (id32_atom_idx < 1) return;
	if (memcmp(parsedAtoms[id32_atom_idx].AtomicName, "ID32", 4) != 0) return;
	
	uint16_t frameID = MatchID3FrameIDstr(frame_str, GlobalID3Tag->ID3v2Tag_MajorVersion);
	int frameType = KnownFrames[frameID].ID3v2_FrameType;
	uint8_t frameCompositionList = GetFrameCompositionDescription(frameType);
	
	if (frameType == ID3_ATTACHED_PICTURE_FRAME || frameType == ID3_ATTACHED_OBJECT_FRAME) {
		APar_EmbeddedFileTests(frame_payload, frameType, adjunct_payloads);
	}
	
	targetFrame = APar_FindFrame(parsedAtoms[id32_atom_idx].ID32_TagInfo, frame_str, frameID, frameType, adjunct_payloads, false);
	
	if (frame_payload == NULL) {
		if (targetFrame != NULL) {
			targetFrame->eliminate_frame = true;
			modified_atoms = true;
			parsedAtoms[id32_atom_idx].ID32_TagInfo->modified_tag = true;
		}
		return;
		
	} else if (strlen(frame_payload) == 0) {
		if (targetFrame != NULL) {
			targetFrame->eliminate_frame = true; //thats right, frames of empty text are removed - so be a doll and try to convey some info, eh?
			modified_atoms = true;
			parsedAtoms[id32_atom_idx].ID32_TagInfo->modified_tag = true;
		}
		return;
	
	} else {
		if (frameType == ID3_UNKNOWN_FRAME) {
			APar_assert(false, 10, frame_str);
			return;
		}
		if (targetFrame == NULL) {
			targetFrame = APar_FindFrame(parsedAtoms[id32_atom_idx].ID32_TagInfo, frame_str, frameID, frameType, adjunct_payloads, true);
			if (targetFrame == NULL) {
				fprintf(stdout, "NULL frame\n");
				exit(0);
			} else {
				APar_FrameInit(targetFrame, frame_str, frameID, frameCompositionList, frame_payload);			
			}
		}
	}
	
	if (targetFrame != NULL) {
		if (adjunct_payloads->zlibCompressed) {
			targetFrame->ID3v2_Frame_Flags |= (ID32_FRAMEFLAG_COMPRESSED + ID32_FRAMEFLAG_LENINDICATED);
		}
		
		if (targetFrame->ID3v2_Frame_ID == ID3v2_FRAME_LANGUAGE) {
			APar_FrameDataPut(targetFrame, adjunct_payloads->targetLang, adjunct_payloads, str_encoding);
		} else {
			APar_FrameDataPut(targetFrame, frame_payload, adjunct_payloads, str_encoding);
		}
		
		if (adjunct_payloads->zlibCompressed) {
			targetFrame->ID3v2_Frame_ExpandedLength = targetFrame->ID3v2_Frame_Length;
		}
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                        id3 displaying functions                                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_Print_ID3TextField(ID3v2Frame* textframe, ID3v2Fields* textfield, bool linefeed = false) {
	//this won't accommodate id3v2.4's multiple strings separated by NULLs
	if (textframe->ID3v2_Frame_Fields->field_string[0] == TE_LATIN1) { //all frames that have text encodings have the encoding as the first field
		if (textfield->field_length > 0) {
			char* conv_buffer = (char*)calloc(1, sizeof(char*)*(textfield->field_length *4) +2);
			isolat1ToUTF8((unsigned char*)conv_buffer, sizeof(char*)*(textfield->field_length *4) +2, (unsigned char*)textfield->field_string, textfield->field_length);
			fprintf(stdout, "%s", conv_buffer);
			free(conv_buffer);
			conv_buffer = NULL;
		}
		
	} else if (textframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF16LE_WITH_BOM) { //technically AP *writes* uff16LE here, but based on BOM, it could be utf16BE
		if (textfield->field_length > 2) {
			char* conv_buffer = (char*)calloc(1, sizeof(char*)*(textfield->field_length *2) +2);
			if (memcmp(textfield->field_string, "\xFF\xFE", 2) == 0) {
				UTF16LEToUTF8((unsigned char*)conv_buffer, sizeof(char*)*(textfield->field_length *4) +2, (unsigned char*)textfield->field_string+2, textfield->field_length);
				fprintf(stdout, "%s", conv_buffer);
			} else {
				UTF16BEToUTF8((unsigned char*)conv_buffer, sizeof(char*)*(textfield->field_length *4) +2, (unsigned char*)textfield->field_string+2, textfield->field_length);
				fprintf(stdout, "%s", conv_buffer);
			}			
			free(conv_buffer);
			conv_buffer = NULL;
		}

	} else if (textframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF16BE_NO_BOM) {
		if (textfield->field_length > 0) {
			char* conv_buffer = (char*)calloc(1, sizeof(char*)*(textfield->field_length *2) +2);
			UTF16BEToUTF8((unsigned char*)conv_buffer, sizeof(char*)*(textfield->field_length *4) +2, (unsigned char*)textfield->field_string, textfield->field_length);
			fprintf(stdout, "%s", conv_buffer);
			free(conv_buffer);
			conv_buffer = NULL;
		}
	} else if (textframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF8) {
		fprintf(stdout, "%s", textfield->field_string);
	} else {
		fprintf(stdout, "(unknown type: 0x%X", textframe->ID3v2_Frame_Fields->field_string[0]);
	}
	if(linefeed) fprintf(stdout, "\n");
	return;
}

char* APar_GetTextEncoding(ID3v2Frame* aframe, ID3v2Fields* textfield) {
	char* text_encoding = NULL;
	if (aframe->ID3v2_Frame_Fields->field_string[0] == TE_LATIN1) text_encoding = "latin1";
	if (aframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF16BE_NO_BOM) {
		if (memcmp(textfield->field_string, "\xFF\xFE", 2) == 0) {
			text_encoding = "utf16le";
		} else if (memcmp(textfield->field_string, "\xFE\xFF", 2) == 0) {
			text_encoding = "utf16be";
		}			
	}
	if (aframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF16LE_WITH_BOM) text_encoding = "utf16le";	
	if (aframe->ID3v2_Frame_Fields->field_string[0] == TE_UTF8) text_encoding = "utf8";
	return text_encoding;
}

void APar_Print_ID3v2_tags(AtomicInfo* id32_atom) {
	//fprintf(stdout, "Maj.Min.Rev version was 2.%u.%u\n", id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion, id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion);
	char* id32_level = (char*)calloc(1, sizeof(char*)*16);
	if (id32_atom->AtomicLevel == 2) {
		memcpy(id32_level, "file level", 10);
	} else if (id32_atom->AtomicLevel == 3) {
		memcpy(id32_level, "movie level", 11);
	} else if (id32_atom->AtomicLevel == 4) {
		sprintf(id32_level, "track #%u", 1); //unimplemented; need to pass a variable here
	}
	
	unsigned char unpacked_lang[3];
	APar_UnpackLanguage(unpacked_lang, id32_atom->AtomicLanguage);
	
	if (id32_atom->ID32_TagInfo->ID3v2_FirstFrame != NULL) {
		fprintf(stdout, "ID32 atom [lang=%s] at %s contains an ID3v2.%u.%u tag (%u tags, %u bytes):\n", unpacked_lang, id32_level, id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion, id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion, id32_atom->ID32_TagInfo->ID3v2_FrameCount, id32_atom->ID32_TagInfo->ID3v2Tag_Length);
	} else {
		fprintf(stdout, "ID32 atom [lang=%s] at %s contains an ID3v2.%u.%u tag. ", unpacked_lang, id32_level, id32_atom->ID32_TagInfo->ID3v2Tag_MajorVersion, id32_atom->ID32_TagInfo->ID3v2Tag_RevisionVersion);
		if (ID3v2_TestTagFlag(id32_atom->ID32_TagInfo->ID3v2Tag_Flags, ID32_TAGFLAG_UNSYNCRONIZATION)) {
			fprintf(stdout, "Unsyncrhonized flag set. Unsupported. No tags read. %u bytes.\n", id32_atom->ID32_TagInfo->ID3v2Tag_Length);
		}
	}
	
	ID3v2Frame* target_frameinfo = id32_atom->ID32_TagInfo->ID3v2_FirstFrame;
	while (target_frameinfo != NULL) {
		fprintf(stdout, " Tag: %s \"%s\" ", target_frameinfo->ID3v2_Frame_Namestr , KnownFrames[target_frameinfo->ID3v2_Frame_ID].ID3V2_FrameDescription );
		uint8_t frame_comp_idx = GetFrameCompositionDescription(target_frameinfo->ID3v2_FrameType);
		if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_UNKNOWN_FRAME) {
			fprintf(stdout, "(unknown frame) %u bytes\n", target_frameinfo->ID3v2_Frame_Fields->field_length);
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_TEXT_FRAME) {
			fprintf(stdout, "(%s) : ", APar_GetTextEncoding(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+1) );
			APar_Print_ID3TextField(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+1, true);
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_TEXT_FRAME_USERDEF) {
			fprintf(stdout, "(user-defined text frame) ");
			fprintf(stdout, "%u fields\n", target_frameinfo->ID3v2_FieldCount);
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_URL_FRAME) {
			fprintf(stdout, "(url frame) : %s\n", (target_frameinfo->ID3v2_Frame_Fields+1)->field_string);
			fprintf(stdout, "%u fields\n", target_frameinfo->ID3v2_FieldCount);
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_URL_FRAME_USERDEF) {
			fprintf(stdout, "(user-defined url frame) ");
			fprintf(stdout, "%u fields\n", target_frameinfo->ID3v2_FieldCount);
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_UNIQUE_FILE_ID_FRAME) {
			if (test_limited_ascii( (target_frameinfo->ID3v2_Frame_Fields+1)->field_string, (target_frameinfo->ID3v2_Frame_Fields+1)->field_length)) {
				fprintf(stdout, "(owner='%s') : %s\n", target_frameinfo->ID3v2_Frame_Fields->field_string, (target_frameinfo->ID3v2_Frame_Fields+1)->field_string);
			} else {
				fprintf(stdout, "(owner='%s') : 0x", target_frameinfo->ID3v2_Frame_Fields->field_string);
				for (uint32_t hexidx = 0; hexidx < (target_frameinfo->ID3v2_Frame_Fields+1)->field_length; hexidx++) {
					fprintf(stdout, "%02X", (uint8_t)(target_frameinfo->ID3v2_Frame_Fields+1)->field_string[hexidx]);
				}
				fprintf(stdout, "\n");
			}
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_CD_ID_FRAME) { //TODO: print hex representation
			uint8_t tracklistings = 0;
			if (target_frameinfo->ID3v2_Frame_Fields->field_length >= 16) {
				tracklistings = target_frameinfo->ID3v2_Frame_Fields->field_length / 8;
				fprintf(stdout, "(Music CD Identifier) : Entries for %u tracks + leadout track.\n   Hex: 0x", tracklistings-1);
			} else {
				fprintf(stdout, "(Music CD Identifier) : Unknown format (less then 16 bytes).\n   Hex: 0x");
			}
			for (uint16_t hexidx = 1; hexidx < target_frameinfo->ID3v2_Frame_Fields->field_length+1; hexidx++) {
				fprintf(stdout, "%02X", (uint8_t)target_frameinfo->ID3v2_Frame_Fields->field_string[hexidx-1]);
				if (hexidx % 4 == 0) fprintf(stdout, " ");
			}
			fprintf(stdout, "\n");
			
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_DESCRIBED_TEXT_FRAME) {
			fprintf(stdout, "(%s, lang=%s, desc[", APar_GetTextEncoding(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+2),
			                                  (target_frameinfo->ID3v2_Frame_Fields+1)->field_string );
			APar_Print_ID3TextField(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+2);
			fprintf(stdout, "]) : ");
			APar_Print_ID3TextField(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+3, true);
				
		} else if (FrameTypeConstructionList[frame_comp_idx].ID3_FrameType == ID3_ATTACHED_PICTURE_FRAME) {
			fprintf(stdout, "(type=0x%02X-'%s', mimetype=%s, %s, desc[", (target_frameinfo->ID3v2_Frame_Fields+2)->field_string[0],
			                 ImageTypeList[ (uint8_t)(target_frameinfo->ID3v2_Frame_Fields+2)->field_string[0] ].imagetype_str, (target_frameinfo->ID3v2_Frame_Fields+1)->field_string,
											 APar_GetTextEncoding(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+1)  );
			APar_Print_ID3TextField(target_frameinfo, target_frameinfo->ID3v2_Frame_Fields+3);
			if (ID3v2_TestFrameFlag(target_frameinfo->ID3v2_Frame_Flags, ID32_FRAMEFLAG_COMPRESSED)) {
				fprintf(stdout, "]) : %u bytes (%u compressed)\n",
				                     (target_frameinfo->ID3v2_Frame_Fields+4)->field_length, target_frameinfo->ID3v2_Frame_Length);
			} else {
				fprintf(stdout, "]) : %u bytes\n", (target_frameinfo->ID3v2_Frame_Fields+4)->field_length);
			}

		} else {
			fprintf(stdout, " [idx=%u;%d]\n", frame_comp_idx, FrameTypeConstructionList[frame_comp_idx].ID3_FrameType);
		}
		target_frameinfo = target_frameinfo->ID3v2_NextFrame;
	}
	free(id32_level);
	id32_level = NULL;
	return;
}

/*----------------------
APar_ImageExtractTest
	buffer - pointer to raw image data
	id3args - *currently unused* when testing raw image data from an image file, results like mimetype & imagetype will be placed here

    Loop through the ImageList array and see if the first few bytes in the image data in buffer match any of the known image_binaryheader types listed. If it does,
		and its png, do a further test to see if its type 0x01 which requires it to be 32x32
----------------------*/
ImageFileFormatDefinition* APar_ImageExtractTest(char* buffer, AdjunctArgs* id3args) {
	ImageFileFormatDefinition* thisImage = NULL;
	uint8_t total_image_tests = (uint8_t)(sizeof(ImageList)/sizeof(*ImageList));
	
	for (uint8_t itest = 0; itest < total_image_tests; itest++) {
		if (ImageList[itest].image_testbytes == 0) {
			if (id3args != NULL) {
				id3args->mimeArg = ImageList[itest].image_mimetype;
			}
			return &ImageList[itest];
		} else if (memcmp(buffer, ImageList[itest].image_binaryheader, ImageList[itest].image_testbytes) == 0) {
			if (id3args != NULL) {
				id3args->mimeArg = ImageList[itest].image_mimetype;
				if (id3args->pictype_uint8 == 0x01) {
					if (memcmp(buffer+16, "\x00\x00\x00\x20\x00\x00\x00\x20", 8) != 0 && itest != 2) {
						id3args->pictype_uint8 = 0x02;
					}
				}
			}
			thisImage = &ImageList[itest];
			break;
		}
	}
	return thisImage;
}

/*----------------------
APar_ID3ExtractFile
	id32_atom_idx - index to the AtomicInfo ID32 atom that contains this while ID3 tag (tag is in all the frames like APIC)
	frame_str - either APIC or GEOB
	originfile - the originating mpeg-4 file that contains the ID32 atom
	destination_folder - *currently not use* TODO: extract to this folder
	id3args - *currently not use* TODO: extract by mimetype or imagetype or description

    Extracts (all) files of a particular frame type (APIC or GEOB - GEOB is currently not implemented) out to a file next to the originating mpeg-4 file. First, match
		frame_str to get the internal frameID number for APIC/GEOB frame. Locate the .ext of the origin file, duplicate the path including the basename (excluding the
		extension. Loop through the linked list of ID3v2Frame and search for the internal frameID number.
		When an image is found, test the data that the image contains and determine file extension from the ImageFileFormatDefinition structure (containing some popular
		image format/extension definitions). In combination with the file extension, use the image description and image type to create the name of the output file.
		The image (which if was compressed on disc was expanded when read in) and simply write out its data (stored in the 5th member of the frame's field strings.
----------------------*/
void APar_ID3ExtractFile(short id32_atom_idx, char* frame_str, char* originfile, char* destination_folder, AdjunctArgs* id3args) {
	uint16_t iter = 0;
	ID3v2Frame* eval_frame = NULL;
	uint32_t basepath_len = 0;
	char* extract_filename = NULL;
	
	if (id32_atom_idx < 1) return;
	if (memcmp(parsedAtoms[id32_atom_idx].AtomicName, "ID32", 4) != 0) return;
	
	uint16_t frameID = MatchID3FrameIDstr(frame_str, parsedAtoms[id32_atom_idx].ID32_TagInfo->ID3v2Tag_MajorVersion);
	int frameType = KnownFrames[frameID].ID3v2_FrameType;
	uint8_t frameCompositionList = GetFrameCompositionDescription(frameType);
	
	if (destination_folder == NULL) {
		basepath_len = (uint32_t)(strrchr(originfile, '.') - originfile);
	}
	
	if (frameType == ID3_ATTACHED_PICTURE_FRAME || frameType == ID3_ATTACHED_OBJECT_FRAME) {
		if (parsedAtoms[id32_atom_idx].ID32_TagInfo->ID3v2_FirstFrame == NULL) return;
		
		eval_frame = parsedAtoms[id32_atom_idx].ID32_TagInfo->ID3v2_FirstFrame;
		extract_filename = (char*)malloc(sizeof(char*)*MAXPATHLEN+1);
		
		while (eval_frame != NULL) {
			if (frameType == eval_frame->ID3v2_FrameType) {
				memset(extract_filename, 0, sizeof(char*)*MAXPATHLEN+1);
				ImageFileFormatDefinition* thisimage = APar_ImageExtractTest((eval_frame->ID3v2_Frame_Fields+4)->field_string, NULL);
				memcpy(extract_filename, originfile, basepath_len);
				iter++;
				
				char* img_description = APar_ConvertField_to_UTF8(eval_frame, ID3_DESCRIPTION_FIELD);
				sprintf(extract_filename+basepath_len, "-img#%u-(desc=%s)-0x%02X%s", 
				                                     iter, img_description, (uint8_t)((eval_frame->ID3v2_Frame_Fields+2)->field_string[0]), thisimage->image_fileextn);
				
				if (img_description != NULL) {
					free(img_description);
					img_description = NULL;
				}
				
				FILE *imgfile = APar_OpenFile(extract_filename, "wb");
				if (imgfile != NULL) {
					fwrite((eval_frame->ID3v2_Frame_Fields+4)->field_string, (size_t)((eval_frame->ID3v2_Frame_Fields+4)->field_length), 1, imgfile);
					fclose(imgfile);
					fprintf(stdout, "Extracted artwork to file: %s\n", extract_filename);
				}

			}
			eval_frame = eval_frame->ID3v2_NextFrame;
		}
	}
	if (extract_filename != NULL) {
		free(extract_filename);
		extract_filename = NULL;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//                           id3 cleanup function                                    //
///////////////////////////////////////////////////////////////////////////////////////

/*----------------------
APar_FreeID32Memory

    free all the little bits of allocated memory. Follow the ID3v2Frame pointers by each frame's ID3v2_NextFrame. Each frame has ID3v2_FieldCount number of field
		strings (char*) that were malloced.
----------------------*/
void APar_FreeID32Memory(ID3v2Tag* id32tag) {
	ID3v2Frame* aframe = id32tag->ID3v2_FirstFrame;
	while (aframe != NULL) {
		
#if defined(DEBUG_V)
		fprintf(stdout, "freeing frame %s of %u fields\n", aframe->ID3v2_Frame_Namestr, aframe->ID3v2_FieldCount);
#endif
		for(uint8_t id3fld = 0; id3fld < aframe->ID3v2_FieldCount; id3fld++) {
#if defined(DEBUG_V)
			fprintf(stdout, "freeing field %s ; %u of %u fields\n", (aframe->ID3v2_Frame_Fields+id3fld)->field_string, id3fld+1, aframe->ID3v2_FieldCount);
#endif
			if ( (aframe->ID3v2_Frame_Fields+id3fld)->field_string != NULL ) {
				free( (aframe->ID3v2_Frame_Fields+id3fld)->field_string );
				(aframe->ID3v2_Frame_Fields+id3fld)->field_string = NULL;
			}
		}
		free( aframe->ID3v2_Frame_Fields );
		aframe->ID3v2_Frame_Fields = NULL;
		free(aframe);	
		aframe = aframe->ID3v2_NextFrame;
	}
	return;
}
