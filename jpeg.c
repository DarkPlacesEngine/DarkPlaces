/*
	Copyright (C) 2002  Mathieu Olivier

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#include "quakedef.h"
#include "image.h"
#include "jpeg.h"


/*
=================================================================

  Minimal set of definitions from the JPEG lib

  WARNING: for a matter of simplicity, several pointer types are
  casted to "void*", and most enumerated values are not included

=================================================================
*/

// jboolean is qbyte instead of int on Win32
#ifdef WIN32
typedef qbyte jboolean;
#else
typedef int jboolean;
#endif

#define JPEG_LIB_VERSION  62  // Version 6b

typedef void *j_common_ptr;
typedef struct jpeg_compress_struct *j_compress_ptr;
typedef struct jpeg_decompress_struct *j_decompress_ptr;
typedef enum
{
	JCS_UNKNOWN,
	JCS_GRAYSCALE,
	JCS_RGB,
	JCS_YCbCr,
	JCS_CMYK,
	JCS_YCCK
} J_COLOR_SPACE;
typedef enum {JPEG_DUMMY1} J_DCT_METHOD;
typedef enum {JPEG_DUMMY2} J_DITHER_MODE;
typedef unsigned int JDIMENSION;

#define JPOOL_PERMANENT	0	// lasts until master record is destroyed
#define JPOOL_IMAGE		1	// lasts until done with image/datastream

#define JPEG_EOI	0xD9  // EOI marker code

#define JMSG_STR_PARM_MAX  80

#define DCTSIZE2 64
#define NUM_QUANT_TBLS 4
#define NUM_HUFF_TBLS 4
#define NUM_ARITH_TBLS 16
#define MAX_COMPS_IN_SCAN 4
#define C_MAX_BLOCKS_IN_MCU 10
#define D_MAX_BLOCKS_IN_MCU 10

struct jpeg_memory_mgr
{
  void* (*alloc_small) (j_common_ptr cinfo, int pool_id, size_t sizeofobject);
  void (*alloc_large) ();
  void (*alloc_sarray) ();
  void (*alloc_barray) ();
  void (*request_virt_sarray) ();
  void (*request_virt_barray) ();
  void (*realize_virt_arrays) ();
  void (*access_virt_sarray) ();
  void (*access_virt_barray) ();
  void (*free_pool) ();
  void (*self_destruct) ();

  long max_memory_to_use;
  long max_alloc_chunk;
};

struct jpeg_error_mgr
{
	void (*error_exit) (j_common_ptr cinfo);
	void (*emit_message) (j_common_ptr cinfo, int msg_level);
	void (*output_message) (j_common_ptr cinfo);
	void (*format_message) (j_common_ptr cinfo, char * buffer);
	void (*reset_error_mgr) (j_common_ptr cinfo);
	int msg_code;
	union {
		int i[8];
		char s[JMSG_STR_PARM_MAX];
	} msg_parm;
	int trace_level;
	long num_warnings;
	const char * const * jpeg_message_table;
	int last_jpeg_message;
	const char * const * addon_message_table;
	int first_addon_message;
	int last_addon_message;
};

struct jpeg_source_mgr
{
	const qbyte *next_input_byte;
	size_t bytes_in_buffer;

	void (*init_source) (j_decompress_ptr cinfo);
	jboolean (*fill_input_buffer) (j_decompress_ptr cinfo);
	void (*skip_input_data) (j_decompress_ptr cinfo, long num_bytes);
	jboolean (*resync_to_restart) (j_decompress_ptr cinfo, int desired);
	void (*term_source) (j_decompress_ptr cinfo);
};

struct jpeg_decompress_struct
{
	struct jpeg_error_mgr *err;		// USED
	struct jpeg_memory_mgr *mem;	// USED

	void *progress;
	void *client_data;
	jboolean is_decompressor;
	int global_state;

	struct jpeg_source_mgr *src;	// USED
	JDIMENSION image_width;			// USED
	JDIMENSION image_height;		// USED

	int num_components;
	J_COLOR_SPACE jpeg_color_space;
	J_COLOR_SPACE out_color_space;
	unsigned int scale_num, scale_denom;
	double output_gamma;
	jboolean buffered_image;
	jboolean raw_data_out;
	J_DCT_METHOD dct_method;
	jboolean do_fancy_upsampling;
	jboolean do_block_smoothing;
	jboolean quantize_colors;
	J_DITHER_MODE dither_mode;
	jboolean two_pass_quantize;
	int desired_number_of_colors;
	jboolean enable_1pass_quant;
	jboolean enable_external_quant;
	jboolean enable_2pass_quant;
	JDIMENSION output_width;

	JDIMENSION output_height;	// USED

	int out_color_components;

	int output_components;		// USED

	int rec_outbuf_height;
	int actual_number_of_colors;
	void *colormap;

	JDIMENSION output_scanline;	// USED

	int input_scan_number;
	JDIMENSION input_iMCU_row;
	int output_scan_number;
	JDIMENSION output_iMCU_row;
	int (*coef_bits)[DCTSIZE2];
	void *quant_tbl_ptrs[NUM_QUANT_TBLS];
	void *dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
	void *ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
	int data_precision;
	void *comp_info;
	jboolean progressive_mode;
	jboolean arith_code;
	qbyte arith_dc_L[NUM_ARITH_TBLS];
	qbyte arith_dc_U[NUM_ARITH_TBLS];
	qbyte arith_ac_K[NUM_ARITH_TBLS];
	unsigned int restart_interval;
	jboolean saw_JFIF_marker;
	qbyte JFIF_major_version;
	qbyte JFIF_minor_version;
	qbyte density_unit;
	unsigned short X_density;
	unsigned short Y_density;
	jboolean saw_Adobe_marker;
	qbyte Adobe_transform;
	jboolean CCIR601_sampling;
	void *marker_list;
	int max_h_samp_factor;
	int max_v_samp_factor;
	int min_DCT_scaled_size;
	JDIMENSION total_iMCU_rows;
	void *sample_range_limit;
	int comps_in_scan;
	void *cur_comp_info[MAX_COMPS_IN_SCAN];
	JDIMENSION MCUs_per_row;
	JDIMENSION MCU_rows_in_scan;
	int blocks_in_MCU;
	int MCU_membership[D_MAX_BLOCKS_IN_MCU];
	int Ss, Se, Ah, Al;
	int unread_marker;
	void *master;
	void *main;
	void *coef;
	void *post;
	void *inputctl;
	void *marker;
	void *entropy;
	void *idct;
	void *upsample;
	void *cconvert;
	void *cquantize;
};


struct jpeg_compress_struct
{
	struct jpeg_error_mgr *err;
	struct jpeg_memory_mgr *mem;
	void *progress;
	void *client_data;
	jboolean is_decompressor;
	int global_state;

	void *dest;
	JDIMENSION image_width;
	JDIMENSION image_height;
	int input_components;
	J_COLOR_SPACE in_color_space;
	double input_gamma;
	int data_precision;

	int num_components;
	J_COLOR_SPACE jpeg_color_space;
	void *comp_info;
	void *quant_tbl_ptrs[NUM_QUANT_TBLS];
	void *dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
	void *ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
	qbyte arith_dc_L[NUM_ARITH_TBLS];
	qbyte arith_dc_U[NUM_ARITH_TBLS];
	qbyte arith_ac_K[NUM_ARITH_TBLS];

	int num_scans;
	const void *scan_info;
	jboolean raw_data_in;
	jboolean arith_code;
	jboolean optimize_coding;
	jboolean CCIR601_sampling;
	int smoothing_factor;
	J_DCT_METHOD dct_method;

	unsigned int restart_interval;
	int restart_in_rows;

	jboolean write_JFIF_header;
	qbyte JFIF_major_version;
	qbyte JFIF_minor_version;
	qbyte density_unit;
	unsigned short X_density;
	unsigned short Y_density;
	jboolean write_Adobe_marker;
	JDIMENSION next_scanline;

	jboolean progressive_mode;
	int max_h_samp_factor;
	int max_v_samp_factor;
	JDIMENSION total_iMCU_rows;
	int comps_in_scan;
	void *cur_comp_info[MAX_COMPS_IN_SCAN];
	JDIMENSION MCUs_per_row;
	JDIMENSION MCU_rows_in_scan;
	int blocks_in_MCU;
	int MCU_membership[C_MAX_BLOCKS_IN_MCU];
	int Ss, Se, Ah, Al;

	void *master;
	void *main;
	void *prep;
	void *coef;
	void *marker;
	void *cconvert;
	void *downsample;
	void *fdct;
	void *entropy;
	void *script_space;
	int script_space_size;
};

struct jpeg_destination_mgr
{
	qbyte* next_output_byte;
	size_t free_in_buffer;

	void (*init_destination) (j_compress_ptr cinfo);
	jboolean (*empty_output_buffer) (j_compress_ptr cinfo);
	void (*term_destination) (j_compress_ptr cinfo);
};


/*
=================================================================

  DarkPlaces definitions

=================================================================
*/

// Functions exported from libjpeg
#define qjpeg_create_compress(cinfo) \
	qjpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct))
#define qjpeg_create_decompress(cinfo) \
	qjpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct))

static void (*qjpeg_CreateCompress) (j_compress_ptr cinfo, int version, size_t structsize);
static void (*qjpeg_CreateDecompress) (j_decompress_ptr cinfo, int version, size_t structsize);
static void (*qjpeg_destroy_compress) (j_compress_ptr cinfo);
static void (*qjpeg_destroy_decompress) (j_decompress_ptr cinfo);
static void (*qjpeg_finish_compress) (j_compress_ptr cinfo);
static jboolean (*qjpeg_finish_decompress) (j_decompress_ptr cinfo);
static jboolean (*qjpeg_resync_to_restart) (j_decompress_ptr cinfo, int desired);
static int (*qjpeg_read_header) (j_decompress_ptr cinfo, jboolean require_image);
static JDIMENSION (*qjpeg_read_scanlines) (j_decompress_ptr cinfo, qbyte** scanlines, JDIMENSION max_lines);
static void (*qjpeg_set_defaults) (j_compress_ptr cinfo);
static void (*qjpeg_set_quality) (j_compress_ptr cinfo, int quality, jboolean force_baseline);
static jboolean (*qjpeg_start_compress) (j_compress_ptr cinfo, jboolean write_all_tables);
static jboolean (*qjpeg_start_decompress) (j_decompress_ptr cinfo);
static struct jpeg_error_mgr* (*qjpeg_std_error) (struct jpeg_error_mgr *err);
static JDIMENSION (*qjpeg_write_scanlines) (j_compress_ptr cinfo, qbyte** scanlines, JDIMENSION num_lines);

static dllfunction_t jpegfuncs[] =
{
	{"jpeg_CreateCompress",		(void **) &qjpeg_CreateCompress},
	{"jpeg_CreateDecompress",	(void **) &qjpeg_CreateDecompress},
	{"jpeg_destroy_compress",	(void **) &qjpeg_destroy_compress},
	{"jpeg_destroy_decompress",	(void **) &qjpeg_destroy_decompress},
	{"jpeg_finish_compress",	(void **) &qjpeg_finish_compress},
	{"jpeg_finish_decompress",	(void **) &qjpeg_finish_decompress},
	{"jpeg_resync_to_restart",	(void **) &qjpeg_resync_to_restart},
	{"jpeg_read_header",		(void **) &qjpeg_read_header},
	{"jpeg_read_scanlines",		(void **) &qjpeg_read_scanlines},
	{"jpeg_set_defaults",		(void **) &qjpeg_set_defaults},
	{"jpeg_set_quality",		(void **) &qjpeg_set_quality},
	{"jpeg_start_compress",		(void **) &qjpeg_start_compress},
	{"jpeg_start_decompress",	(void **) &qjpeg_start_decompress},
	{"jpeg_std_error",			(void **) &qjpeg_std_error},
	{"jpeg_write_scanlines",	(void **) &qjpeg_write_scanlines},
	{NULL, NULL}
};

// Handle for JPEG DLL
dllhandle_t jpeg_dll = NULL;

static qbyte jpeg_eoi_marker [2] = {0xFF, JPEG_EOI};
static qboolean error_in_jpeg;

// Our own output manager for JPEG compression
typedef struct
{
	struct jpeg_destination_mgr pub;

	qfile_t* outfile;
	qbyte* buffer;
} my_destination_mgr;
typedef my_destination_mgr* my_dest_ptr;


/*
=================================================================

  DLL load & unload

=================================================================
*/

/*
====================
JPEG_OpenLibrary

Try to load the JPEG DLL
====================
*/
qboolean JPEG_OpenLibrary (void)
{
	const char* dllnames [] =
	{
#if defined(WIN64)
		"libjpeg64.dll",
#elif defined(WIN32)
		"libjpeg.dll",
#elif defined(MACOSX)
		"libjpeg.62.dylib",
#else
		"libjpeg.so.62",
		"libjpeg.so",
#endif
		NULL
	};

	// Already loaded?
	if (jpeg_dll)
		return true;

	// Load the DLL
	if (! Sys_LoadLibrary (dllnames, &jpeg_dll, jpegfuncs))
	{
		Con_Printf ("JPEG support disabled\n");
		return false;
	}

	Con_Printf ("JPEG support enabled\n");
	return true;
}


/*
====================
JPEG_CloseLibrary

Unload the JPEG DLL
====================
*/
void JPEG_CloseLibrary (void)
{
	Sys_UnloadLibrary (&jpeg_dll);
}


/*
=================================================================

	JPEG decompression

=================================================================
*/

static void JPEG_Noop (j_decompress_ptr cinfo) {}

static jboolean JPEG_FillInputBuffer (j_decompress_ptr cinfo)
{
    // Insert a fake EOI marker
    cinfo->src->next_input_byte = jpeg_eoi_marker;
    cinfo->src->bytes_in_buffer = 2;

	return TRUE;
}

static void JPEG_SkipInputData (j_decompress_ptr cinfo, long num_bytes)
{
    if (cinfo->src->bytes_in_buffer <= (unsigned long)num_bytes)
	{
		cinfo->src->bytes_in_buffer = 0;
		return;
	}

    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}

static void JPEG_MemSrc (j_decompress_ptr cinfo, const qbyte *buffer)
{
	cinfo->src = (struct jpeg_source_mgr *)cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof (struct jpeg_source_mgr));

	cinfo->src->next_input_byte = buffer;
	cinfo->src->bytes_in_buffer = fs_filesize;

	cinfo->src->init_source = JPEG_Noop;
	cinfo->src->fill_input_buffer = JPEG_FillInputBuffer;
	cinfo->src->skip_input_data = JPEG_SkipInputData;
	cinfo->src->resync_to_restart = qjpeg_resync_to_restart; // use the default method
	cinfo->src->term_source = JPEG_Noop;
}

static void JPEG_ErrorExit (j_common_ptr cinfo)
{
	((struct jpeg_decompress_struct*)cinfo)->err->output_message (cinfo);
	error_in_jpeg = true;
}


/*
====================
JPEG_LoadImage

Load a JPEG image into a RGBA buffer
====================
*/
qbyte* JPEG_LoadImage (const qbyte *f, int matchwidth, int matchheight)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	qbyte *image_rgba, *scanline;
	unsigned int line;

	// No DLL = no JPEGs
	if (!jpeg_dll)
		return NULL;

	cinfo.err = qjpeg_std_error (&jerr);
	qjpeg_create_decompress (&cinfo);
	JPEG_MemSrc (&cinfo, f);
	qjpeg_read_header (&cinfo, TRUE);
	qjpeg_start_decompress (&cinfo);

	image_width = cinfo.image_width;
	image_height = cinfo.image_height;

	if ((matchwidth && image_width != matchwidth) || (matchheight && image_height != matchheight))
	{
		qjpeg_finish_decompress (&cinfo);
		qjpeg_destroy_decompress (&cinfo);
		return NULL;
	}
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Con_Printf("JPEG_LoadImage: invalid image size %ix%i\n", image_width, image_height);
		return NULL;
	}

	image_rgba = (qbyte *)Mem_Alloc(tempmempool, image_width * image_height * 4);
	scanline = (qbyte *)Mem_Alloc(tempmempool, image_width * cinfo.output_components);
	if (!image_rgba || !scanline)
	{
		if (!image_rgba)
			Mem_Free (image_rgba);

		Con_Printf("JPEG_LoadImage: not enough memory for %i by %i image\n", image_width, image_height);
		qjpeg_finish_decompress (&cinfo);
		qjpeg_destroy_decompress (&cinfo);
		return NULL;
	}

	// Decompress the image, line by line
	line = 0;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		qbyte *buffer_ptr;
		int ind;

		qjpeg_read_scanlines (&cinfo, &scanline, 1);

		// Convert the image to RGBA
		switch (cinfo.output_components)
		{
			// RGB images
			case 3:
				buffer_ptr = &image_rgba[image_width * line * 4];
				for (ind = 0; ind < image_width * 3; ind += 3, buffer_ptr += 4)
				{
					buffer_ptr[0] = scanline[ind];
					buffer_ptr[1] = scanline[ind + 1];
					buffer_ptr[2] = scanline[ind + 2];
					buffer_ptr[3] = 255;
				}
				break;

			// Greyscale images (default to it, just in case)
			case 1:
			default:
				buffer_ptr = &image_rgba[image_width * line * 4];
				for (ind = 0; ind < image_width; ind++, buffer_ptr += 4)
				{
					buffer_ptr[0] = scanline[ind];
					buffer_ptr[1] = scanline[ind];
					buffer_ptr[2] = scanline[ind];
					buffer_ptr[3] = 255;
				}
		}

		line++;
	}
	Mem_Free (scanline);

	qjpeg_finish_decompress (&cinfo);
	qjpeg_destroy_decompress (&cinfo);

	return image_rgba;
}


/*
=================================================================

  JPEG compression

=================================================================
*/

#define JPEG_OUTPUT_BUF_SIZE 4096
static void JPEG_InitDestination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr)cinfo->dest;
	dest->buffer = (qbyte*)cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_IMAGE, JPEG_OUTPUT_BUF_SIZE * sizeof(qbyte));
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
}

static jboolean JPEG_EmptyOutputBuffer (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr)cinfo->dest;

	if (FS_Write (dest->outfile, dest->buffer, JPEG_OUTPUT_BUF_SIZE) != (size_t) JPEG_OUTPUT_BUF_SIZE)
	{
		error_in_jpeg = true;
		return false;
	}

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
	return true;
}

static void JPEG_TermDestination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr)cinfo->dest;
	size_t datacount = JPEG_OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	// Write any data remaining in the buffer
	if (datacount > 0)
		if (FS_Write (dest->outfile, dest->buffer, datacount) != (fs_offset_t)datacount)
			error_in_jpeg = true;
}

static void JPEG_MemDest (j_compress_ptr cinfo, qfile_t* outfile)
{
	my_dest_ptr dest;

	// First time for this JPEG object?
	if (cinfo->dest == NULL)
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_destination_mgr));

	dest = (my_dest_ptr)cinfo->dest;
	dest->pub.init_destination = JPEG_InitDestination;
	dest->pub.empty_output_buffer = JPEG_EmptyOutputBuffer;
	dest->pub.term_destination = JPEG_TermDestination;
	dest->outfile = outfile;
}


/*
====================
JPEG_SaveImage_preflipped

Save a preflipped JPEG image to a file
====================
*/
qboolean JPEG_SaveImage_preflipped (const char *filename, int width, int height, qbyte *data)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	qbyte *scanline;
	unsigned int offset, linesize;
	qfile_t* file;

	// No DLL = no JPEGs
	if (!jpeg_dll)
	{
		Con_Print("You need the libjpeg library to save JPEG images\n");
		return false;
	}

	// Open the file
	file = FS_Open (filename, "wb", true, false);
	if (!file)
		return false;

	cinfo.err = qjpeg_std_error (&jerr);
	cinfo.err->error_exit = JPEG_ErrorExit;
	error_in_jpeg = false;

	qjpeg_create_compress (&cinfo);
	JPEG_MemDest (&cinfo, file);

	// Set the parameters for compression
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	qjpeg_set_defaults (&cinfo);
	qjpeg_set_quality (&cinfo, scr_screenshot_jpeg_quality.value * 100, TRUE);
	qjpeg_start_compress (&cinfo, true);

	// Compress each scanline
	linesize = cinfo.image_width * 3;
	offset = linesize * (cinfo.image_height - 1);
	while (cinfo.next_scanline < cinfo.image_height)
	{
		scanline = &data[offset - cinfo.next_scanline * linesize];

		qjpeg_write_scanlines (&cinfo, &scanline, 1);
		if (error_in_jpeg)
			break;
	}

	qjpeg_finish_compress (&cinfo);
	qjpeg_destroy_compress (&cinfo);

	FS_Close (file);
	return true;
}
