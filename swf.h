/*
 * SOB - Swf OBfuscator
 * Copyright (C) 2008-2009 Kostas Michalopoulos
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Kostas Michalopoulos <badsector@runtimeterror.com>
 */

#ifndef __SWF_H_INCLUDED__
#define __SWF_H_INCLUDED__

#include <stdlib.h>
#include <stdio.h>
#include "zlib.h"

typedef struct _rename_t
{
	char* old_name;
	char* new_name;
} rename_t;

typedef struct _swf_rect
{
	long xmin;
	long xmax;
	long ymin;
	long ymax;
} swf_rect;

typedef struct _swf_tag_t
{
	struct _swf_t* swf; /* the swf object this swf tag belongs to */
	int type; /* tag type */
	size_t pos; /* tag data position in swf->body */
	size_t length; /* tag data length */
	unsigned char* data; /* tag data (pointer inside swf->body) */
	struct _swf_tag_t* next; /* next tag in swf tags list */
	struct _swf_tag_t* prev; /* previous tag in swf tags list */
} swf_tag_t;

typedef struct _swf_t
{
	FILE* fh; /* file handle */
	char* filename; /* the filename used with swf_read or swf_write */
	int compressed; /* if 1 the file is read from a zlib stream */
	int version; /* swf file version */
	size_t length; /* swf file length (as recorded in the swf file) */
	swf_rect rect; /* swf movie area in twips */
	unsigned long rate; /* delay between frames in 8.8 format */
	unsigned long frame_count; /* frame count */
	unsigned char* body; /* swf uncompressed "body" */
	size_t body_length; /* length of the body in bytes */
	size_t body_pos; /* header position in body for virtual i/o functions */
	unsigned char bb; /* used for reading/writing bit values */
	int bit; /* used for reading/writing bit values */
	swf_tag_t* first_tag; /* the first swf tag */
	swf_tag_t* last_tag; /* the last swf tag */
	size_t tag_count; /* number of tags defined in this swf */
	rename_t* rename; /* a dynamic array containing the renames done by the obfuscators so far */
	int renames; /* size of the rename dynamic array */
} swf_t;

swf_t* swf_read(const char* filename);
int swf_write(swf_t* swf, const char* filename);
void swf_free(swf_t* swf);
void swf_replace_tag_data(swf_tag_t* tag, const unsigned char* data, size_t length);
void swf_add_rename(swf_t* swf, const char* old_name, const char* new_name);
char* swf_check_rename(swf_t* swf, const char* old_name, const char* new_name);

#endif

