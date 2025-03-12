
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// ------------------------------------------------------------------------------------------------
// Constants

// wing it.
#define PATH_BUFFER_SIZE 300

// ------------------------------------------------------------------------------------------------
// Helpers

bool streq(const char* a, const char* b) {
	return strcmp(a, b) == 0;
}

void fatal(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

#define CHECKED_FREAD(f, ptr, filename)\
	if(fread((ptr), sizeof(*(ptr)), 1, (f)) == 0) {\
		fatal("failed to read from %s.", (filename));\
	}

#define CHECKED_FWRITE(f, ptr, filename)\
	if(fwrite((ptr), sizeof(*(ptr)), 1, (f)) == 0) {\
		fatal("failed to write to %s.", (filename));\
	}

#define CHECKED_FREAD_SIZE(f, ptr, size, filename)\
	if(fread((ptr), (size), 1, (f)) == 0) {\
		fatal("failed to read from %s.", (filename));\
	}

#define CHECKED_FWRITE_SIZE(f, ptr, size, filename)\
	if(fwrite((ptr), (size), 1, (f)) == 0) {\
		fatal("failed to write to %s.", (filename));\
	}

//add padding if bmp.padding is not zero
void padding_helper(FILE* in_file, uint32_t padding, FILE* out_file){	
	fseek(in_file, padding, SEEK_CUR);
	uint8_t zeros[] = {0,0,0,0};
	CHECKED_FWRITE_SIZE(out_file, &zeros, padding, "output");

}

//change csrgb to clinear
double csrgb_to_clinear(const double csrgb){
	double clinear;
	if(csrgb <= 0.04045){
		clinear = csrgb/12.92;
		return clinear;
	}
	clinear = pow((csrgb + 0.055) / 1.055, 2.4);
	return clinear;
}

//change ylinear to ysrgb
double ylinear_to_ysrgb(const double ylinear){
	double ysrgb;
	if(ylinear <= 0.0031308){
		ysrgb = ylinear * 12.92;
		return ysrgb;
	}
	ysrgb = pow(ylinear, 1/2.4) * 1.055 -0.055;
	return ysrgb;
}
// ------------------------------------------------------------------------------------------------
// OpenBMP
typedef struct {
	FILE*    in;
	FILE*    out;
	uint32_t pixelStart;
	uint32_t width;
	uint32_t height;
	uint32_t padding;
} OpenBMP;

void bmp_close(OpenBMP* bmp){
	//close infile or outfile
	if(bmp->in != NULL){
		fclose(bmp->in);
		bmp->in = NULL;
	}
	if(bmp->out != NULL){
		fclose(bmp->out);
		bmp->out = NULL;
	}
}

void bmp_open(OpenBMP* bmp, const char* in_filename){
	//open file
	bmp->in = fopen(in_filename, "rb");
	if(bmp->in == NULL){
		fatal("could not open %s for reading.", in_filename);
	}
	//look for magic number
	char magic[2] = "";
 	CHECKED_FREAD(bmp->in, &magic, in_filename);
	if(strncmp(magic, "BM", 2) !=0){
		fatal("%s does not appear to be a valid BMP file (bad magic).", in_filename);
	}
	//check the length of the file
	uint32_t file_len;
	CHECKED_FREAD(bmp->in, &file_len, in_filename);
	fseek(bmp->in, 0, SEEK_END);
	long actual_len = ftell(bmp->in);
	if(actual_len!=file_len){
		fatal("%s does not appear to be a valid BMP file (bad length).", in_filename);
	}
	//read bmp->pixelStart
	fseek(bmp->in, 10, SEEK_SET);
	CHECKED_FREAD(bmp->in, &bmp->pixelStart, in_filename);
	//read DIB header size, if not 40 print error
	fseek(bmp->in, 14, SEEK_SET);
	uint32_t header;
	CHECKED_FREAD(bmp->in, &header, in_filename);
	if(header != 40){
		fatal("%s is an unsupported version of BMP.", in_filename);
	}
	//read width and height
	CHECKED_FREAD(bmp->in, &bmp->width, in_filename);
	CHECKED_FREAD(bmp->in, &bmp->height, in_filename);
	//read the bits and if not 24 print error
	bmp->padding = bmp->width % 4;
	fseek(bmp->in, 28, SEEK_SET);
	uint16_t bpp;
	CHECKED_FREAD(bmp->in, &bpp, in_filename);
	if(bpp!=24){
		fatal("%s is %dbpp which is unsupported.", in_filename, bpp);
	}
	fseek(bmp->in, bmp->pixelStart, SEEK_SET);	
}

void bmp_open_output(OpenBMP* bmp, const char* in_filename, const char* out_prefix){
	char out_filename[PATH_BUFFER_SIZE];
	//print the filename
	snprintf(out_filename, sizeof(out_filename), "%s_%s", out_prefix, in_filename);
	bmp->out = fopen(out_filename, "wb");
	//if null print the error
	if(bmp->out == NULL){
		fatal("could not open %s for writing.", out_filename);
	}
	//seek to the beginning 
	fseek(bmp->in, 0, SEEK_SET);
	char vla[bmp->pixelStart];
	//read the beginning infle and write to the outfile
	CHECKED_FREAD(bmp->in, &vla, in_filename);
	CHECKED_FWRITE(bmp->out, &vla, out_filename);
}
// ------------------------------------------------------------------------------------------------
// Pixel
typedef struct {
	uint8_t b;
	uint8_t g;
	uint8_t r;
} Pixel;


//invert the pixel
void pixel_invert(Pixel* p){
	p->b = ~p->b;
	p->g = ~p->g;
	p->r = ~p->r;
}

//swap the pixel
void pixel_swap(Pixel* p, Pixel* p2){
	Pixel temp = *p;
	*p = *p2;
	*p2 = temp;
}

// ------------------------------------------------------------------------------------------------
// Functions!!!!!

void print_info(const char* in_filename) {
	//open file and print the information
	OpenBMP bmp = {};
	bmp_open(&bmp, in_filename);
	printf("Size: %d x %d\n", bmp.width, bmp.height);
	printf("Padding between rows: %d\n", bmp.padding);
	printf("Pixel data start offset: %d\n", bmp.pixelStart);
	bmp_close(&bmp);
}

void invert_image(const char* in_filename) {
	//open the file and check prefix
	OpenBMP bmp = {};
	bmp_open(&bmp, in_filename);
	bmp_open_output(&bmp, in_filename, "inv");
	//nested loop for rows and columns
	for(int i = 0; i < bmp.height;i++){
		for(int j = 0; j < bmp.width; j++){
			//read the pixel, invert it and write it back
			Pixel pixelVar;
			CHECKED_FREAD(bmp.in, &pixelVar, in_filename);
			pixel_invert(&pixelVar);
			CHECKED_FWRITE(bmp.out, &pixelVar, "output");
		}
		//check for padding
		if(bmp.padding !=0 ){
			padding_helper(bmp.in, bmp.padding, bmp.out);
		}
	}
	//close file
	bmp_close(&bmp);
}

void grayscale_image(const char* in_filename) {
	//open file
	OpenBMP bmp = {};
	bmp_open(&bmp, in_filename);
	bmp_open_output(&bmp, in_filename, "gray");
	for(int i = 0; i < bmp.height;i++){
		for(int j = 0; j < bmp.width; j++){
			//read the pixel
			Pixel pixelVar;
			CHECKED_FREAD(bmp.in, &pixelVar, in_filename);
			//divide the colors by 255.0
			double b_srgb = pixelVar.b/255.0;
			double g_srgb = pixelVar.g/255.0;
			double r_srgb = pixelVar.r/255.0;
			//convert them to linear variables
			b_srgb = csrgb_to_clinear(b_srgb);
			g_srgb = csrgb_to_clinear(g_srgb);
			r_srgb = csrgb_to_clinear(r_srgb);
			//create a ylinear variable
			double Y_linear = (b_srgb*0.0722) + (g_srgb*0.7152) + (r_srgb*0.2126);
			//convert  the ylinear to ysrgb
			double Y_srgb = ylinear_to_ysrgb(Y_linear);
			//multiply the ysrgb and cast as uint8 then write it back
			uint8_t Y_uint8 = (uint8_t)(Y_srgb * 255);
			Pixel grayscale_pixel = {Y_uint8, Y_uint8, Y_uint8};
			CHECKED_FWRITE(bmp.out, &grayscale_pixel, "output");
		}
		//check for padding
		if(bmp.padding !=0 ){
			padding_helper(bmp.in, bmp.padding, bmp.out);
		}
	}
	//close file
	bmp_close(&bmp);
}

void hflip_image(const char* in_filename) {
	//open file
	OpenBMP bmp = {};
	bmp_open(&bmp, in_filename);
	bmp_open_output(&bmp, in_filename, "hflip");
	//create a pixel with width items long
	Pixel row[bmp.width];
	for(int i = 0; i < bmp.height;i++){
		//read entire row into the pixel array
		CHECKED_FREAD(bmp.in, &row, in_filename);
		for(int j = 0; j < bmp.width/2; j++){
			//swap the pixel from x to width-1-x
			pixel_swap(&row[j], &row[bmp.width - 1- j]);
		}
		//write it back 
		CHECKED_FWRITE(bmp.out, &row, "output");
		//check for padding
		if(bmp.padding !=0 ){
			padding_helper(bmp.in, bmp.padding, bmp.out);
		}
	}
	//close file
	bmp_close(&bmp);
}

// ------------------------------------------------------------------------------------------------
// main

int main(int argc, char** argv) {
	if(argc == 3 && streq(argv[1], "info")) {
		print_info(argv[2]);
	} else if(argc == 3 && streq(argv[1], "invert")) {
		invert_image(argv[2]);
	} else if(argc == 3 && streq(argv[1], "grayscale")) {
		grayscale_image(argv[2]);
	} else if(argc == 3 && streq(argv[1], "hflip")) {
		hflip_image(argv[2]);
	} else {
		printf("Usage:\n");
		printf("  %s info filename.bmp\n", argv[0]);
		printf("     Checks that filename.bmp is a BMP file and prints some info about it.\n");
		printf("  %s invert filename.bmp\n", argv[0]);
		printf("     Produces inv_filename.bmp with the colors inverted.\n");
		printf("  %s grayscale filename.bmp\n", argv[0]);
		printf("     Produces gray_filename.bmp with the colors converted to grayscale.\n");
		printf("  %s hflip filename.bmp\n", argv[0]);
		printf("     Produces hflip_filename.bmp with the pixels flipped horizontally.\n");
		return 1;
	}

	return 0;
}