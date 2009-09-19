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

#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "swf.h"


/***** SWF READING CODE *******************************************************/

static int swf_read_body(swf_t* swf)
{
	if (swf->compressed) {
		Bytef* data;
		int ret;
		size_t csize = 0;
		Bytef* cdata = NULL;
		uLongf length = swf->length;
		/* read the rest of the swf file in memory */
		while (!feof(swf->fh)) {
			char chunk[1024];
			size_t rb = fread(chunk, 1, 1024, swf->fh);
			if (rb < 1) break;
			cdata = realloc(cdata, csize + rb);
			memcpy(cdata + csize, chunk, rb);
			csize += rb;
		}
		
		/* decompress */
		data = malloc(swf->length);
		ret = uncompress(data, &length, cdata, csize);
		free(cdata);
		swf->body = (unsigned char*)data;
		swf->body_length = (size_t)length;
	} else {
		swf->body_length = swf->length - 8;
		swf->body = malloc(swf->body_length);
		if (fread(swf->body, 1, swf->body_length, swf->fh) != swf->body_length) {
			return 0;
		}
	}
	swf->body_pos = 0;
	swf->bit = 8; /* reset bitvalue counter */
	return 1;
}

static unsigned short swf_read_ui16(swf_t* swf)
{
	unsigned short r = swf->body[swf->body_pos] | (swf->body[swf->body_pos + 1] << 8);
	swf->body_pos += 2;
	return r;
}

static unsigned long swf_read_ui32(swf_t* swf)
{
	unsigned long r = swf->body[swf->body_pos] |
					   (swf->body[swf->body_pos + 1] << 8) |
					   (swf->body[swf->body_pos + 2] << 16) |
					   (swf->body[swf->body_pos + 3] << 24);
	swf->body_pos += 4;
	return r;
}

static unsigned long swf_read_ub(swf_t* swf, int bits)
{
	unsigned long r = 0;
	while (bits > 0) {
		if (swf->bit == 8) {
			swf->bit = 0;
			swf->bb = swf->body[swf->body_pos++];
		}
		bits--;
		if (swf->bb & (1 << (7 - swf->bit))) {
			r += 1 << bits;
		}
		swf->bit++;
	}
	return r;
}

static long swf_read_sb(swf_t* swf, int bits)
{
	long r = 0, w = 0, lb = 0;
	while (bits > 0) {
		if (swf->bit == 8) {
			swf->bit = 0;
			swf->bb = swf->body[swf->body_pos++];
		}
		bits--;
		if (swf->bb & (1 << (7 - swf->bit))) {
			r += 1 << bits;
			lb = 1;
		} else lb = 0;
		swf->bit++;
		w++;
	}
	if (lb == 1) {
		while (w < 32) {
			r += 1 << w;
			w++;
		}
	}
	
	return r;
}

static void swf_read_tag(swf_t* swf)
{
	unsigned short codelen = swf_read_ui16(swf);
	unsigned short code = codelen >> 6;
	unsigned long len = codelen & 0x3F;
	swf_tag_t* tag;
	if (len == 0x3F) len = swf_read_ui32(swf);
	
	tag = malloc_zero(sizeof(swf_tag_t));
	tag->type = code;
	tag->pos = swf->body_pos;
	tag->length = (size_t)len;
	tag->data = swf->body + swf->body_pos;
	tag->swf = swf;
	tag->prev = swf->last_tag;
	if (swf->last_tag) swf->last_tag->next = tag;
	else swf->first_tag = tag;
	swf->last_tag = tag;
	
	swf->body_pos += tag->length;
}

static int swf_read_header(swf_t* swf)
{
	unsigned char id[3], ver, la, lb, lc, ld;
	/* read three-byte signature*/
	fread(&id[0], 1, 1, swf->fh);
	fread(&id[1], 1, 1, swf->fh);
	fread(&id[2], 1, 1, swf->fh);
	if (id[1] != 0x57 || id[2] != 0x53) return 0;
	/* see if the file is compressed */
	if (id[0] == 0x43) {
		swf->compressed = 1;
	} else {
		swf->compressed = 0;
	}
	/* read version */
	fread(&ver, 1, 1, swf->fh);
	swf->version = ver;
	/* read length */
	fread(&la, 1, 1, swf->fh);
	fread(&lb, 1, 1, swf->fh);
	fread(&lc, 1, 1, swf->fh);
	fread(&ld, 1, 1, swf->fh);
	swf->length = la | (lb << 8) | (lc << 16) | (ld << 24);
	return 1;
}

swf_t* swf_read(const char* filename)
{
	int bits;
	swf_t* swf = malloc_zero(sizeof(swf_t));
	swf->fh = fopen(filename, "rb");
	if (!swf->fh) {
		free(swf);
		return NULL;
	}
	swf->filename = strclone(filename);
	/* read header */
	if (!swf_read_header(swf)) {
		swf_free(swf);
		return NULL;
	}
	/* read body */
	if (!swf_read_body(swf)) {
		swf_free(swf);
		return NULL;
	}
	/* read movie rect area */
	bits = swf_read_ub(swf, 5);
	swf->rect.xmin = swf_read_sb(swf, bits);
	swf->rect.xmax = swf_read_sb(swf, bits);
	swf->rect.ymin = swf_read_sb(swf, bits);
	swf->rect.ymax = swf_read_sb(swf, bits);
	/* read movie info */
	swf->rate = swf_read_ui16(swf);
	swf->frame_count = swf_read_ui16(swf);
	/* read tags */
	swf->tag_count = 0;
	while (swf->body_pos < swf->body_length) {
		swf_read_tag(swf);
		swf->tag_count++;
	}
	/* close file */
	fclose(swf->fh);
	swf->fh = NULL;
	return swf;
}


/***** SWF WRITING CODE *******************************************************/

static void swf_write_header(swf_t* swf)
{
	unsigned char id[3], ver, la, lb, lc, ld;
	/* write three-byte signature*/
	id[0] = 0x43; /* always compress */
	id[1] = 0x57;
	id[2] = 0x53;
	fwrite(&id[0], 1, 1, swf->fh);
	fwrite(&id[1], 1, 1, swf->fh);
	fwrite(&id[2], 1, 1, swf->fh);
	/* write version */
	ver = (unsigned char)swf->version;
	fwrite(&ver, 1, 1, swf->fh);
	/* read length */
	la = swf->length & 0xFF;
	lb = (swf->length >> 8) & 0xFF;
	lc = (swf->length >> 16) & 0xFF;
	ld = (swf->length >> 24) & 0xFF;
	fwrite(&la, 1, 1, swf->fh);
	fwrite(&lb, 1, 1, swf->fh);
	fwrite(&lc, 1, 1, swf->fh);
	fwrite(&ld, 1, 1, swf->fh);
}

static void swf_write_body(swf_t* swf)
{
	uLongf destlen = swf->body_length*2;
	Bytef* dest = malloc(destlen);
	if (compress2(dest, &destlen, swf->body, swf->body_length, 9) == Z_OK) {
		fwrite(dest, 1, destlen, swf->fh);
	} else {
		char b = 0x46;
		fprintf(stderr, "sob: error: failed to compress the swf body data.\n");
		fprintf(stderr, "sob: will try to store uncompressed data.\n");
		fwrite(swf->body, 1, swf->body_length, swf->fh);
		fseek(swf->fh, 0, SEEK_SET);
		fwrite(&b, 1, 1, swf->fh);
		fseek(swf->fh, 0, SEEK_END);
	}
	free(dest);
}

int swf_write(swf_t* swf, const char* filename)
{
	swf->fh = fopen(filename, "wb");
	if (!swf->fh) {
		return 0;
	}
	swf_write_header(swf);
	swf_write_body(swf);
	fclose(swf->fh);
	swf->fh = NULL;
	
	return 1;
}


/***** COMMON SWF CODE ********************************************************/

void swf_free(swf_t* swf)
{
	if (swf->body) free(swf->body);
	if (swf->filename) free(swf->filename);
	if (swf->fh) fclose(swf->fh);
	if (swf->rename) {
		size_t i;
		for (i=0; i<swf->renames; i++) {
			free(swf->rename[i].old_name);
			free(swf->rename[i].new_name);
		}
		free(swf->rename);
	}
	while (swf->first_tag) {
		swf->last_tag = swf->first_tag->next;
		free(swf->first_tag);
		swf->first_tag = swf->last_tag;
	}
	free(swf);
}

void swf_replace_tag_data(swf_tag_t* tag, const unsigned char* data, size_t length)
{
	swf_t* swf = tag->swf;
	size_t len_diff = length - tag->length;
	size_t before_tag_body_length = tag->pos;
	size_t after_tag_body_length = swf->body_length - tag->pos - tag->length;
	swf_tag_t* otag;
	unsigned char* before_tag_body = malloc(before_tag_body_length);
	unsigned char* after_tag_body = malloc(after_tag_body_length);
	
	/* replace tag data in body */
	memcpy(before_tag_body, swf->body, before_tag_body_length);
	memcpy(after_tag_body, swf->body + tag->pos + tag->length, after_tag_body_length);
	free(swf->body);
	swf->body_length = before_tag_body_length + after_tag_body_length + length;
	swf->length = swf->body_length + 8;
	swf->body = malloc(before_tag_body_length + after_tag_body_length + length);
	memcpy(swf->body, before_tag_body, before_tag_body_length);
	memcpy(swf->body + before_tag_body_length, data, length);
	memcpy(swf->body + before_tag_body_length + length, after_tag_body, after_tag_body_length);
	tag->length = length;
	free(after_tag_body);
	free(before_tag_body);
	
	/* update next tags position */
	for (otag = tag->next; otag; otag = otag->next)	otag->pos += len_diff;

	/* update all tags data pointer */
	for (otag = swf->first_tag; otag; otag = otag->next) otag->data = swf->body + otag->pos;

	/* update tag's length in body (TODO: modify the code to shift the bytes if tag header differs in size) */
	swf->body[tag->pos - 4] = tag->length & 0xFF;
	swf->body[tag->pos - 3] = (tag->length >> 8) & 0xFF;
	swf->body[tag->pos - 2] = (tag->length >> 16) & 0xFF;
	swf->body[tag->pos - 1] = (tag->length >> 24) & 0xFF;
}

void swf_add_rename(swf_t* swf, const char* old_name, const char* new_name)
{
	swf->rename = realloc(swf->rename, sizeof(rename_t)*(swf->renames + 1));
	swf->rename[swf->renames].old_name = strclone(old_name);
	swf->rename[swf->renames].new_name = strclone(new_name);
	swf->renames++;
}

char* swf_check_rename(swf_t* swf, const char* old_name, const char* new_name)
{
	int i;
	for (i=0; i<swf->renames; i++) {
		if (!strcmp(swf->rename[i].old_name, old_name)) {
			return strclone(swf->rename[i].new_name);
		}
	}
	swf_add_rename(swf, old_name, new_name);
	return strclone(new_name);
}

