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
typedef struct jpeg_decompress_struct *j_decompress_ptr;
typedef enum {JPEG_DUMMY1} J_COLOR_SPACE;
typedef enum {JPEG_DUMMY2} J_DCT_METHOD;
typedef enum {JPEG_DUMMY3} J_DITHER_MODE;
typedef unsigned int JDIMENSION;

#define JPOOL_PERMANENT 0

#define JPEG_EOI	0xD9  // EOI marker code

#define JMSG_STR_PARM_MAX  80

#define DCTSIZE2 64
#define NUM_QUANT_TBLS 4
#define NUM_HUFF_TBLS 4
#define NUM_ARITH_TBLS 16
#define MAX_COMPS_IN_SCAN 4
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


/*
=================================================================

  DarkPlaces definitions

=================================================================
*/

// Functions exported from libjpeg
static void (*qjpeg_CreateDecompress) (j_decompress_ptr cinfo, int version, size_t structsize);
static void (*qjpeg_destroy_decompress) (j_decompress_ptr cinfo);
static jboolean (*qjpeg_finish_decompress) (j_decompress_ptr cinfo);
static jboolean (*qjpeg_resync_to_restart) (j_decompress_ptr cinfo, int desired);
static int (*qjpeg_read_header) (j_decompress_ptr cinfo, jboolean require_image);
static JDIMENSION (*qjpeg_read_scanlines) (j_decompress_ptr cinfo, qbyte** scanlines, JDIMENSION max_lines);
static jboolean (*qjpeg_start_decompress) (j_decompress_ptr cinfo);
static struct jpeg_error_mgr* (*qjpeg_std_error) (struct jpeg_error_mgr *err);

static dllfunction_t jpegfuncs[] =
{
	{"jpeg_CreateDecompress",	(void **) &qjpeg_CreateDecompress},
	{"jpeg_destroy_decompress",	(void **) &qjpeg_destroy_decompress},
	{"jpeg_finish_decompress",	(void **) &qjpeg_finish_decompress},
	{"jpeg_resync_to_restart",	(void **) &qjpeg_resync_to_restart},
	{"jpeg_read_header",		(void **) &qjpeg_read_header},
	{"jpeg_read_scanlines",		(void **) &qjpeg_read_scanlines},
	{"jpeg_start_decompress",	(void **) &qjpeg_start_decompress},
	{"jpeg_std_error",			(void **) &qjpeg_std_error},
	{NULL, NULL}
};

// Handle for JPEG DLL
static dllhandle_t jpeg_dll = NULL;


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
	const char* dllname;
	const dllfunction_t *func;

	// Already loaded?
	if (jpeg_dll)
		return true;

#ifdef WIN32
	dllname = "libjpeg.dll";
#else
	dllname = "libjpeg.so.62";
#endif

	// Initializations
	for (func = jpegfuncs; func && func->name != NULL; func++)
		*func->funcvariable = NULL;

	// Load the DLL
	if (! (jpeg_dll = Sys_LoadLibrary (dllname)))
	{
		Con_Printf("Can't find %s. JPEG support disabled\n", dllname);
		return false;
	}

	// Get the function adresses
	for (func = jpegfuncs; func && func->name != NULL; func++)
		if (!(*func->funcvariable = (void *) Sys_GetProcAddress (jpeg_dll, func->name)))
		{
			Con_Printf("missing function \"%s\" - broken JPEG library!\n", func->name);
			JPEG_CloseLibrary ();
			return false;
		}

	Con_Printf("%s loaded. JPEG support enabled\n", dllname);
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
	if (!jpeg_dll)
		return;

	Sys_UnloadLibrary (jpeg_dll);
	jpeg_dll = NULL;
}


/*
=================================================================

  Functions for handling JPEG images

=================================================================
*/

static qbyte jpeg_eoi_marker [2] = {0xFF, JPEG_EOI};

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

static void JPEG_MemSrc (j_decompress_ptr cinfo, qbyte *buffer)
{
	cinfo->src = (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof (struct jpeg_source_mgr));

	cinfo->src->next_input_byte = buffer;
	cinfo->src->bytes_in_buffer = fs_filesize;

	cinfo->src->init_source = JPEG_Noop;
	cinfo->src->fill_input_buffer = JPEG_FillInputBuffer;
	cinfo->src->skip_input_data = JPEG_SkipInputData;
	cinfo->src->resync_to_restart = qjpeg_resync_to_restart; // use the default method
	cinfo->src->term_source = JPEG_Noop;
}


/*
====================
JPEG_LoadImage

Load a JPEG image into a RGBA buffer
====================
*/
qbyte* JPEG_LoadImage (qbyte *f, int matchwidth, int matchheight)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	qbyte *image_rgba, *scanline;
	unsigned int line;

	// No DLL = no JPEGs
	if (!jpeg_dll)
		return NULL;

	cinfo.err = qjpeg_std_error (&jerr);
	qjpeg_CreateDecompress (&cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));
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

	image_rgba = Mem_Alloc(tempmempool, image_width * image_height * 4);
	scanline = Mem_Alloc(tempmempool, image_width * cinfo.output_components);
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
