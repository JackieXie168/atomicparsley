//==================================================================//
/*
    AtomicParsley - AP_NSImage.mm

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

#import <Cocoa/Cocoa.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#include "AP_NSImage.h"
#include "AtomicParsley.h"

bool isJPEG=false;
bool isPNG=false;

off_t pic_file_size;

void findPicFileSize(const char *path) {
	struct stat picfileStats;
	stat(path, &picfileStats);
	pic_file_size=picfileStats.st_size;
	
	return;
}

void DetermineType(const char *picfilePath) {
	char* picHeader=(char *)malloc(sizeof(char)*8);;
	u_int64_t r;
	
	FILE *pic_file = NULL;
	pic_file = fopen(picfilePath, "rb");
  r = fread(picHeader, 8, 1, pic_file);
  fclose(pic_file);
	
	if (strncmp(picHeader, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) { //casts uchar* to char* (2)
				isPNG=true;
				isJPEG=false;
	} else if (strncmp(picHeader, "\xFF\xD8\xFF\xE0", 4) == 0) {//casts uchar* to char* (2)
				isJPEG=true;
				isPNG=false;
	}
	return;
}

char* DeriveNewPath(const char *filePath, PicPrefs myPicPrefs) {
	char* suffix = strrchr(filePath, '.');
	//fprintf(stdout, "strsep %s\n", suffix);
	
	size_t filepath_len = strlen(filePath);
	char* newpath = (char *)malloc(sizeof(char) * filepath_len + 30);
	size_t base_len = filepath_len-strlen(suffix);
	strncpy(newpath, filePath, base_len);
	
	char* appendage = "-resized-";
	for (size_t i=0; i <= strlen(appendage); i++) {
		newpath[base_len+i] = appendage[i];
	}
	
	char randstring[5];
	srand((int) time(NULL)); //Seeds rand()
	int randNum = rand()%10000;
	sprintf(randstring, "%i", randNum);
	strcat(newpath, randstring);
	
	if (myPicPrefs.allJPEG) {
		strcat(newpath, ".jpg");
	} else if (myPicPrefs.allPNG) {
		strcat(newpath, ".png");
	} else {
		strcat(newpath, suffix);
	}
	
	if ( (strncmp(suffix,".jpg",4) == 0) || (strncmp(suffix,".jpeg",5) == 0) || (strncmp(suffix,".JPG",4) == 0) || (strncmp(suffix,".JPEG",5) == 0) ) {
		isJPEG=true;
	} else if ((strncmp(suffix,".png",4) == 0) || (strncmp(suffix,".PNG",4) == 0)) {
		isPNG=true;
	}
	return newpath;
}

char* ResizeGivenImage(const char* filePath, PicPrefs myPicPrefs) {
	char* new_path=(char*)malloc(sizeof(char)*1024);
	BOOL resize = false;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	char* aFile = strdup(filePath);	
	NSString *inFile;
	inFile = [NSString stringWithUTF8String: aFile];
	free(aFile); //<-----NEWNEWNEW
	aFile=NULL; //<-----NEWNEWNEW
	
	NSImage* source = [ [NSImage alloc] initWithContentsOfFile: inFile ];
	[source setScalesWhenResized: YES];
	if ( source == nil ) {
		fprintf( stderr, "Image '%s' could not be loaded.\n", filePath );
    exit (1);
  }
	
	NSSize sourceSize = [source size];
	float hmax, vmax, aspect;
	hmax = sourceSize.width;
	vmax = sourceSize.height;
	aspect = sourceSize.height / sourceSize.width;
	//fprintf(stdout, "aspect %f2.4\n", aspect);
	if (myPicPrefs.max_dimension != 0) {
		if ( ( (int)sourceSize.width > myPicPrefs.max_dimension) || ( (int)sourceSize.height > myPicPrefs.max_dimension) ) {
			resize = true; //only if dimensions are LARGER than our max do we resize
			if (hmax > vmax) {
				hmax = myPicPrefs.max_dimension;
				vmax = myPicPrefs.max_dimension * aspect;
			} else {
				hmax = myPicPrefs.max_dimension / aspect;
				vmax = myPicPrefs.max_dimension;
			}
		}
	}
	
	///// determine dpi/ppi
	float hres, vres, hdpi, vdpi;
	NSImageRep *myRep = [[source representations] objectAtIndex:0];
	hres = [myRep pixelsWide]; //native pixel dimensions
	vres = [myRep pixelsHigh];
	hdpi = hres/sourceSize.width; //in native resolution (multiply by 72 to get native dpi)
	vdpi = vres/sourceSize.height;
	
	if ( ( (int)hdpi != 1 ) || ( (int)vdpi != 1) ) {
		resize = true;
		hmax = hres;
		vmax = vres;
		if (myPicPrefs.max_dimension != 0) {
			//we also need to recheck we don't go over our max dimensions (again)
			if ( ( (int)hres > myPicPrefs.max_dimension) || ( (int)vres > myPicPrefs.max_dimension) ) {
				if (hmax > vmax) {
					hmax = myPicPrefs.max_dimension;
					vmax = myPicPrefs.max_dimension * aspect;
				} else {
					hmax = myPicPrefs.max_dimension / aspect;
					vmax = myPicPrefs.max_dimension;
				}
			}
		}
	}
	
	if (myPicPrefs.squareUp) {
		if (myPicPrefs.max_dimension != 0) {
			vmax = myPicPrefs.max_dimension;
			hmax = myPicPrefs.max_dimension;
			resize = true;
		} else {
		//this will stretch the image to the largest dimension. Hope you don't try to scale a 160x1200 image... it could get ugly
			if (hmax > vmax) {
				vmax = hmax;
				resize = true;
			} else if (vmax > hmax) {
				hmax = vmax;
				resize = true;
			}
		}
	}
	
	findPicFileSize(filePath);
	if ( ( (int)pic_file_size > myPicPrefs.max_Kbytes) && ( myPicPrefs.max_Kbytes != 0) ) {
		resize = true;
	}
	
	DetermineType(filePath);
	if ( (isJPEG && myPicPrefs.allPNG) || (isPNG && myPicPrefs.allJPEG) ) { //handle jpeg->png & png->jpg conversion
		resize = true;
		
	}
	
	NSRect destinationRect = NSMakeRect( 0, 0, hmax, vmax );
	NSSize size = NSMakeSize( hmax, vmax );
		
	if (resize) {
		[NSApplication sharedApplication];
		[[NSGraphicsContext currentContext] setImageInterpolation: NSImageInterpolationHigh];
	
		[source setSize: size];
        
		NSImage* image = [[NSImage alloc] initWithSize:size];
		[image lockFocus];
    
		NSEraseRect( destinationRect );
		[source drawInRect: destinationRect
							fromRect: destinationRect
							operation: NSCompositeCopy fraction: 1.0];
        
		NSBitmapImageRep* bitmap = [ [NSBitmapImageRep alloc]
																	initWithFocusedViewRect: destinationRect ];
		_NSBitmapImageFileType filetype;
		NSDictionary *props;
		
		if ( (isPNG && !myPicPrefs.allJPEG) || myPicPrefs.allPNG) {
				filetype = NSPNGFileType;
				props = nil;
			
		} else {
			filetype = NSJPEGFileType;
			props = [  NSDictionary dictionaryWithObject:
																		  [NSNumber numberWithFloat: 0.7] forKey: NSImageCompressionFactor];
		}
		NSData* data = [bitmap representationUsingType:filetype properties:props];
		
		unsigned dataLength = [data length]; //holds the file length
		
		int iter = 0;
		float compression = 0.65;
		if ( (myPicPrefs.max_Kbytes != 0) && (filetype == NSJPEGFileType) ) {
			while ( (dataLength > (unsigned)myPicPrefs.max_Kbytes) && (iter < 10) ) {
				props = [  NSDictionary dictionaryWithObject:
																		  [NSNumber numberWithFloat: compression] forKey: NSImageCompressionFactor];
				data = [bitmap representationUsingType:filetype properties:props];
				dataLength = [data length];
				compression = compression - 0.05;
				iter++;
			}
		}
		
		[bitmap release];
		//new_path = DeriveNewPath(filePath, myPicPrefs);
		//const char* new_path = "/Users/walking/talking/rectum.jpg";
		//NSString *outFile= [NSString stringWithUTF8String: new_path];
		NSString *outFile= [NSString stringWithUTF8String: DeriveNewPath(filePath, myPicPrefs)];
		//NSLog(outFile);
		[[NSFileManager defaultManager]
          createFileAtPath: outFile
          contents: data
          attributes: nil ];
        
		[image unlockFocus];
		[image release];
		isJPEG=false;
		isPNG=false;
		//new_path = (char*)[outFile cStringUsingEncoding: [NSString NSUTF8StringEncoding] ];
		//const char* stringToBeHashed = [outFile cStringUsingEncoding: NSNonLossyASCIIStringEncoding];
		//fprintf(stdout, "lasting path %s\n", stringToBeHashed);
		new_path = (char*)[outFile cStringUsingEncoding: NSNonLossyASCIIStringEncoding];
	}
	[pool release];
	return new_path;
}
