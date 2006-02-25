//==================================================================//
/*
    AtomicParsley - AP_AtomExtracts.cpp

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
#include <time.h>
#include "AP_AtomExtracts.h"
#include "AtomicParsley.h"

///////////////////////////////////////////////////////////////////////////////////////
//                             File reading routines                                 //
///////////////////////////////////////////////////////////////////////////////////////

uint8_t APar_read8(FILE* m4afile, uint32_t pos) {
	uint8_t a_byte = 0;
	fseek(m4afile, pos, SEEK_SET);
	fread(&a_byte, 1, 1, m4afile);
	return a_byte;
}

uint16_t APar_read16(char* &buffer, FILE* m4afile, uint32_t pos) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 2, m4afile);
	return UInt16FromBigEndian(buffer);
}

uint32_t APar_read32(char* &buffer, FILE* m4afile, uint32_t pos) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, 4, m4afile);
	return UInt32FromBigEndian(buffer);
}

void APar_readX(char* &buffer, FILE* m4afile, uint32_t pos, uint32_t length) {
	fseek(m4afile, pos, SEEK_SET);
	fread(buffer, 1, length, m4afile);
	return;
}

uint32_t APar_FindValueInAtom(char* &uint32_buffer, FILE* m4afile, short an_atom, uint32_t start_position, uint32_t eval_number) {
	uint32_t current_pos = start_position;
	memset(uint32_buffer, 0, 5);
	while (current_pos <= parsedAtoms[an_atom].AtomicLength) {
		current_pos ++;
		if (eval_number > 65535) {
			//current_pos +=4;
			if (APar_read32(uint32_buffer, m4afile, parsedAtoms[an_atom].AtomicStart + current_pos) == eval_number) {
				break;
			}
		} else {
			//current_pos +=2;
			if (APar_read16(uint32_buffer, m4afile, parsedAtoms[an_atom].AtomicStart + current_pos) == (uint16_t)eval_number) {
				break;
			}
		}
		if (current_pos >= parsedAtoms[an_atom].AtomicLength) {
			current_pos =  0;
			break;
		}
	}
	return current_pos;
}

uint8_t APar_skip_filler(FILE* m4afile, uint32_t start_position) {
	uint8_t skip_bytes = 0;
	
	while (true) {
		uint8_t eval_byte = APar_read8(m4afile, start_position + skip_bytes);
		
		if (eval_byte == 0x80 || eval_byte == 0xFE) {
			skip_bytes++;
		} else {
			break;
		}
	}
	return skip_bytes;
}

///////////////////////////////////////////////////////////////////////////////////////
//                       Time / Language / Channel specifics                         //
///////////////////////////////////////////////////////////////////////////////////////

void determine_MonthDay(int literal_day, int &month, int &day) {
	if (literal_day <= 31) {
		month = 1; day = literal_day;
	
	} else if (literal_day <= 59) {
		month = 2; day = literal_day - 31;
	
	} else if (literal_day <= 90) {
		month = 3; day = literal_day - 59;
	
	} else if (literal_day <= 120) {
		month = 4; day = literal_day - 90;
	
	} else if (literal_day <= 151) {
		month = 5; day = literal_day - 120;
	
	} else if (literal_day <= 181) {
		month = 6; day = literal_day - 151;
	
	} else if (literal_day <= 212) {
		month = 7; day = literal_day - 181;
		
	} else if (literal_day <= 243) {
		month = 8; day = literal_day - 212;
	
	} else if (literal_day <= 273) {
		month = 9; day = literal_day - 243;
	
	} else if (literal_day <= 304) {
		month = 10; day = literal_day - 273;
	
	} else if (literal_day <= 334) {
		month = 11; day = literal_day - 304;
	
	} else if (literal_day <= 365) {
		month = 12; day = literal_day - 334;
	}
	return;
}

char* APar_ExtractUTC(uint32_t total_secs) {
	//this will probably be off between Jan 1 & Feb 28 on a leap year by a day.... I'm somehow cope & deal.
	struct tm timeinfo = {0,0,0,0,0};

	int offset_year = (int)( (double)total_secs / 31536000 ); //60 * 60 * 24 * 365 (ordinary year in seconds; doesn't account for leap year)
	int literal_year = 1904 + offset_year;
	int literal_days_into_year = ((total_secs % 31536000) / 86400) - (offset_year / 4); //accounts for the leap year
	
	uint32_t literal_seconds_into_day = total_secs % 86400;
	
	int month =  0;
	int days = 0;
	
	determine_MonthDay(literal_days_into_year, month, days);
	
	if (literal_days_into_year < 0 ) {
		literal_year -=1;
		literal_days_into_year = 31 +literal_days_into_year;
		month = 12;
		days = literal_days_into_year;
	}
	
	int hours = literal_seconds_into_day / 3600;
	
	timeinfo.tm_year = literal_year - 1900;
	timeinfo.tm_yday = literal_days_into_year;
	timeinfo.tm_mon = month - 1;
	timeinfo.tm_mday = days;
	timeinfo.tm_wday = (((total_secs / 86400) - (offset_year / 4)) - 5 ) % 7;
	
	timeinfo.tm_hour = hours;
	timeinfo.tm_min = (literal_seconds_into_day - (hours * 3600)) / 60;
	timeinfo.tm_sec = (int)(literal_seconds_into_day % 60);
		
	
	return asctime(&timeinfo);
}

void APar_UnpackLanguage(unsigned char lang_code[], uint16_t packed_language) {
	lang_code[3] = 0;
	lang_code[2] = (packed_language &0x1F) + 0x60;
	lang_code[1] = ((packed_language >> 5) &0x1F) + 0x60;
	lang_code[0] = ((packed_language >> 10) &0x1F) + 0x60;
	return;
}

uint8_t APar_ExtractChannelInfo(FILE* m4afile, uint32_t pos) {
	uint8_t packed_channels = APar_read8(m4afile, pos);
	uint8_t unpacked_channels = (packed_channels << 1); //just shift the first bit off the table
	unpacked_channels = (unpacked_channels >> 4); //and slide it on over back on the uint8_t
	return unpacked_channels;
}

esds_AudioInfo* APar_Extract_audio_esds_Info(char* &uint32_buffer, FILE* m4afile, short track_level_atom) {
	static esds_AudioInfo this_esds_info;
	esds_AudioInfo* esds_ptr = &this_esds_info;
	memset ( &this_esds_info, 0, sizeof (esds_AudioInfo) );	
	uint32_t offset_into_stsd = 0;
	
	while (offset_into_stsd < parsedAtoms[track_level_atom].AtomicLength) {
		offset_into_stsd ++;
		if ( APar_read32(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + offset_into_stsd) == 0x65736473 ) {
			this_esds_info.contains_esds = true;
		
			uint32_t esds_start = parsedAtoms[track_level_atom].AtomicStart + offset_into_stsd - 4;
			uint32_t esds_length = APar_read32(uint32_buffer, m4afile, esds_start);
			uint32_t offset_into_esds = 12; //4bytes length + 4 bytes name + 4bytes null
						
			if ( APar_read8(m4afile, esds_start + offset_into_esds) == 0x03 ) {
				offset_into_esds++;
				offset_into_esds += APar_skip_filler(m4afile, esds_start + offset_into_esds);
			}

			uint8_t section3_length = APar_read8(m4afile, esds_start + offset_into_esds);
			if ( section3_length <= esds_length && section3_length != 0) {
				this_esds_info.section3_length = section3_length;
			} else {
				break;
			}
			
			offset_into_esds+= 4; //1 bytes section 0x03 length + 2 bytes + 1 byte
			
			if ( APar_read8(m4afile, esds_start + offset_into_esds) == 0x04 ) {
				offset_into_esds++;
				offset_into_esds += APar_skip_filler(m4afile, esds_start + offset_into_esds);
			}
			
			uint8_t section4_length = APar_read8(m4afile, esds_start + offset_into_esds);
			if ( section4_length <= section3_length && section4_length != 0) {
				this_esds_info.section4_length = section4_length;
				
				offset_into_esds++;
				this_esds_info.descriptor_object_typeID = APar_read8(m4afile, esds_start + offset_into_esds);
				
				offset_into_esds+= 5;
				this_esds_info.max_bitrate = APar_read32(uint32_buffer, m4afile, esds_start + offset_into_esds);
				offset_into_esds+= 4;
				this_esds_info.avg_bitrate = APar_read32(uint32_buffer, m4afile, esds_start + offset_into_esds);
				offset_into_esds+= 4;
			} else {
				break;
			}
			
			if ( APar_read8(m4afile, esds_start + offset_into_esds) == 0x05 ) {
				offset_into_esds++;
				offset_into_esds += APar_skip_filler(m4afile, esds_start + offset_into_esds);
			}
			
			uint8_t section5_length = APar_read8(m4afile, esds_start + offset_into_esds);
			if ( section5_length <= section4_length && section5_length != 0) {
				this_esds_info.section5_length = section5_length;
				offset_into_esds+=2;
				this_esds_info.channels = APar_ExtractChannelInfo(m4afile, esds_start + offset_into_esds);
				//fprintf(stdout, "channs = %u", this_esds_info.channels);
			}
			break; //uh, I've extracted the pertinent info
		
		}
		if (offset_into_stsd > parsedAtoms[track_level_atom].AtomicLength) {
			break;
		}
	
	}

	return esds_ptr;
}

///////////////////////////////////////////////////////////////////////////////////////
//                            Track Level Atom Info                                  //
//              (suspiciously like the original in AtomicParsley.cpp)                //
///////////////////////////////////////////////////////////////////////////////////////

void APar_TrackLevelInfo(uint8_t &total_tracks, uint8_t &track_num, short &codec_atom, char* extraction_atom) {
	uint8_t track_tally = 0;
	short iter = 0;
	
	while (parsedAtoms[iter].NextAtomNumber != 0) {
	
		if ( strncmp(parsedAtoms[iter].AtomicName, "trak", 4) == 0) {
			track_tally += 1;
			if (track_num == 0) {
				total_tracks += 1;
				
			} else if (track_num == track_tally) {
				
				short next_atom = parsedAtoms[iter].NextAtomNumber;
				while (parsedAtoms[next_atom].AtomicLevel > parsedAtoms[iter].AtomicLevel) {
					
					if (strncmp(parsedAtoms[next_atom].AtomicName, extraction_atom, 4) == 0) {
					
						codec_atom = parsedAtoms[next_atom].AtomicNumber;
						return;
					} else {
						next_atom = parsedAtoms[next_atom].NextAtomNumber;
					}
					if (parsedAtoms[next_atom].AtomicLevel == parsedAtoms[iter].AtomicLevel) {
						codec_atom = 0;
					}
				}
			}
		}
		iter=parsedAtoms[iter].NextAtomNumber;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////////////
//             Completely ugly way to get at some track-level info                   //
///////////////////////////////////////////////////////////////////////////////////////

void APar_ExtractDetails(FILE* m4afile) {
	uint8_t total_tracks = 0;
	uint8_t track_num = 0;
	short track_level_atom = 0;
	char* uint32_buffer=(char*)malloc( sizeof(char)*5 );
	uint32_t four_bytes = 0;
	uint32_t _offset = 0;
		
	//With track_num set to 0, it will return the total trak atom into total_tracks here.
	APar_TrackLevelInfo(total_tracks, track_num, track_level_atom, NULL);

	fprintf(stdout, "Low-level details. Total tracks: %u \n", total_tracks);
	
	if (total_tracks > 0) {
		while (total_tracks > track_num) {
			bool is_sound_file = false;
			track_num+= 1;
			fprintf(stdout, "  Track %u\n", track_num);
			
			//type/codec/encoder section
			
			APar_TrackLevelInfo(total_tracks, track_num, track_level_atom, "hdlr");
			memset(uint32_buffer, 0, 5);
			four_bytes = APar_read32(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + 16);
			char4TOuint32(four_bytes, uint32_buffer);
			fprintf(stdout, "    Type: %s", uint32_buffer);
			
			if (strncmp(uint32_buffer, "soun", 4) == 0) {
				is_sound_file = true;
			}
			
			if ( parsedAtoms[track_level_atom].AtomicLength > 34) {
				char* trackname = (char*)malloc( sizeof(char)*parsedAtoms[track_level_atom].AtomicLength - 32 );
				memset(trackname, 0, parsedAtoms[track_level_atom].AtomicLength - 32);
				
				APar_readX(trackname, m4afile, parsedAtoms[track_level_atom].AtomicStart + 32, parsedAtoms[track_level_atom].AtomicLength - 32);
				fprintf(stdout, "    Name: %s", trackname);
				free(trackname);
				trackname = NULL;
			}
			
			APar_TrackLevelInfo(total_tracks, track_num, track_level_atom, "stsd");
			memset(uint32_buffer, 0, 5);
			four_bytes = APar_read32(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + 20);
			char4TOuint32(four_bytes, uint32_buffer);
			fprintf(stdout, "    Kind/Codec: %s", uint32_buffer);
			
			//number of channels
			
			if (is_sound_file) {
			
				esds_AudioInfo* this_info = APar_Extract_audio_esds_Info(uint32_buffer, m4afile, track_level_atom);
				if (this_info->contains_esds) {
					fprintf(stdout, "    Channels: [%u]", this_info->channels);
					
				} else { //alac files don't have esds; channels aren't bitpacked either; moot since Apple Lossless doesn't appear able to convert to 5.1
					this_info->channels = APar_read8(m4afile, parsedAtoms[track_level_atom].AtomicStart + 41);
					fprintf(stdout, "    Channels: (%u)", this_info->channels );
				}
			}
			
			//Encoder string; occasionally, it appears under stsd for a video track; it is typcally preceded by ' ²' (1st char is unprintable) or 0x01B2
			
			_offset = APar_FindValueInAtom(uint32_buffer, m4afile, track_level_atom, 24, 0x01B2);
			if (_offset > 0 && _offset < parsedAtoms[track_level_atom].AtomicLength) {
				_offset +=2;
				char* encoder_name = (char*)malloc( sizeof(char)*parsedAtoms[track_level_atom].AtomicLength - _offset );
				memset(encoder_name, 0, parsedAtoms[track_level_atom].AtomicLength - _offset);
				
				APar_readX(encoder_name, m4afile, parsedAtoms[track_level_atom].AtomicStart + _offset, parsedAtoms[track_level_atom].AtomicLength - _offset);
				fprintf(stdout, "    Encoder: %s", encoder_name); // a single Nero file had "em4v 4.1.6.5" - but others didn't
			}
			
			fprintf(stdout, "\n");
			
			//language code
			
			APar_TrackLevelInfo(total_tracks, track_num, track_level_atom, "mdhd");
			memset(uint32_buffer, 0, 5);
			uint16_t packed_language = APar_read16(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + 28);
			unsigned char unpacked_lang[3];
			APar_UnpackLanguage(unpacked_lang, packed_language);
			fprintf(stdout, "    Language code: %s\n", unpacked_lang); //http://www.w3.org/WAI/ER/IG/ert/iso639.htm
			
			//dates section
			
			APar_TrackLevelInfo(total_tracks, track_num, track_level_atom, "tkhd");
			if ( APar_read8(m4afile, parsedAtoms[track_level_atom].AtomicStart + 8) == 0) {
				four_bytes = APar_read32(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + 12);
				fprintf(stdout, "    Creation Date (UTC):      %s", APar_ExtractUTC(four_bytes));
				four_bytes = APar_read32(uint32_buffer, m4afile, parsedAtoms[track_level_atom].AtomicStart + 16);
				fprintf(stdout, "    Modification Date (UTC):  %s\n", APar_ExtractUTC(four_bytes));
				
			}
		}
	}

	return;
}
