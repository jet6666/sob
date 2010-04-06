/*
 * SOB - Swf OBfuscator
 * Copyright (C) 2008-2010 Kostas Michalopoulos
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

#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "ignorsym.h"
#include "swf.h"
#include "options.h"
#include "ob_abc.h"

typedef struct _context_t
{
	swf_t* swf; /* the swf file this context is based on */
	unsigned char* data; /* ABC data */
	size_t length; /* length */
	size_t pos; /* current position for read_*** functions */
	int error; /* if 1 some error was found */
	char** str_table; /* string table */
	size_t str_table_size; /* string table size */
	size_t str_table_start; /* position in ABC where the string table begins */
	size_t str_table_end; /* position in ABC where the string table ends */
	long* name_table; /* a table with indices in str_table for names */
	size_t name_table_size;
} context_t;

static unsigned short read_ui16(context_t* ctx)
{
	unsigned short r = ctx->data[ctx->pos] | (ctx->data[ctx->pos + 1] << 8);
	ctx->pos += 2;
	return r;
}

static unsigned long read_encoded_int(context_t* ctx)
{
	unsigned long res = ctx->data[ctx->pos++];
	if (!(res & 0x00000080)) return res;
	res = (res & 0x0000007F) | (ctx->data[ctx->pos++] << 7);
	if (!(res & 0x00004000)) return res;
	res = (res & 0x00003FFF) | (ctx->data[ctx->pos++] << 14);
	if (!(res & 0x00200000)) return res;
	res = (res & 0x001FFFFF) | (ctx->data[ctx->pos++] << 21);
	if (!(res & 0x10000000)) return res;
	return (res & 0x0FFFFFFF) | (ctx->data[ctx->pos++] << 28);
}

static char* read_string_info(context_t* ctx)
{
	long size = read_encoded_int(ctx);
	char* data = malloc(size + 1);
	memcpy(data, ctx->data + ctx->pos, size);
	data[size] = 0;
	ctx->pos += size;
	return data;
}

static void add_name(context_t* ctx, long name)
{
	size_t i;
	if (name == 0 || name >= ctx->str_table_size || !ctx->str_table[name][0]) return;
	for (i=0; i<ctx->name_table_size; i++) {
		if (name == ctx->name_table[i]) return;
	}
	ctx->name_table = realloc(ctx->name_table, sizeof(long)*(ctx->name_table_size + 1));
	ctx->name_table[ctx->name_table_size++] = name;
}

static void parse_constant_pool(context_t* ctx)
{
	long count, idx;
	/* skip integers */
	count = read_encoded_int(ctx) - 1;
	while (count > 0) { read_encoded_int(ctx); count--; }
	/* skip unsigned integers */
	count = read_encoded_int(ctx) - 1;
	while (count > 0) { read_encoded_int(ctx); count--; }
	/* skip doubles */
	count = read_encoded_int(ctx) - 1;
	ctx->pos += count*8;
	/* read strings */
	count = read_encoded_int(ctx);
	ctx->str_table_start = ctx->pos;
	ctx->str_table_size = count<1?0:count;
	if (ctx->str_table_size > 0) {
		ctx->str_table = malloc(sizeof(char*)*count);
		ctx->str_table[0] = strclone("");
		for (idx=1; idx<count; idx++) {
			ctx->str_table[idx] = read_string_info(ctx);
		}
	}
	ctx->str_table_end = ctx->pos;
	/* process namespaces */
	count = read_encoded_int(ctx) - 1;
	while (count > 0) { 
		ctx->pos++; /* skip kind */
		if (opt_obfuscate_namespaces) add_name(ctx, read_encoded_int(ctx));
		else read_encoded_int(ctx);
		count--;
	}
	/* skip namespace sets */
	count = read_encoded_int(ctx) - 1;
	while (count > 0) { 
		long cnt = read_encoded_int(ctx);
		for (;cnt>0;cnt--) read_encoded_int(ctx);
		count--;
	}
	/* process multinames */
	count = read_encoded_int(ctx) - 1;
	while (count > 0) { 
		int kind = ctx->data[ctx->pos++];
		switch (kind) {
		case 0x07: /* QName, QNameA */
		case 0x0D:
			read_encoded_int(ctx); /* skip namespace id */
			if (opt_obfuscate_qnames) add_name(ctx, read_encoded_int(ctx));
			else read_encoded_int(ctx);
			break;
		case 0x0F: /* RTQName, RTQNameA */
		case 0x10:
			if (opt_obfuscate_rtqnames) add_name(ctx, read_encoded_int(ctx));
			else read_encoded_int(ctx);
			break;
		case 0x09: /* RTMultiname, RTMultinameA */
		case 0x0E:
			read_encoded_int(ctx);
			if (opt_obfuscate_rtmultinames) add_name(ctx, read_encoded_int(ctx));
			else read_encoded_int(ctx);
			break;
		case 0x1B: /* RTMultinameL, RTMultinameLA */
		case 0x1C:
			read_encoded_int(ctx); /* skip namespace set id */
			break;
		default: ; /* RTQNameL, RTQNameLA do not contain extra data */
		}
		count--;
	}
}

static char* generate_new_name(context_t* ctx, const char* name, int idx)
{
	char nn[512];
	char c = "jiolmn7y6tefx0kf"[idx&15];
	sprintf(nn, "_%cs%s%c_", c, int2str(idx, 36), c);
	return swf_check_rename(ctx->swf, name, nn);
}

static void rebuild_names(context_t* ctx)
{
	size_t i, nid = 0;
	/* rebuild name table */
	for (i=0; i<ctx->name_table_size; i++) {
		long idx = ctx->name_table[i];
		char* old_name = ctx->str_table[idx];
		char* new_name;
		/* if the name is in the ignore table, ignore it */
		if (ignore_check(old_name)) continue;
		new_name = generate_new_name(ctx, old_name, nid++);
		free(old_name);
		ctx->str_table[idx] = new_name;
	}
}

static void rebuild_string_table(context_t* ctx)
{
	unsigned char* table;
	size_t table_len = 0, pos = 0;
	size_t before_table_data_size;
	size_t after_table_data_size;
	unsigned char* before_table_data;
	unsigned char* after_table_data;
	int i;
	/* build the table data */
	for (i=1; i<ctx->str_table_size; i++) {
		size_t size = strlen(ctx->str_table[i]);
		table_len += strlen(ctx->str_table[i]) + 1;
		if (size > 0x7F) table_len++;
		if (size > 0x3FFF) table_len++;
		if (size > 0x1FFFFF) table_len++;
	}
	table = malloc(table_len);
	for (i=1; i<ctx->str_table_size; i++) {
		size_t size = strlen(ctx->str_table[i]);
		table[pos++] = size&0x7F;
		if (size > 0x7F) { table[pos - 1] |= 0x80; table[pos++] = (size >> 7)&0x7F; }
		if (size > 0x3FFF) { table[pos - 1] |= 0x80; table[pos++] = (size >> 14)&0x7F; }
		if (size > 0x1FFFFF) { table[pos - 1] |= 0x80; table[pos++] = (size >> 21)&0x7F; }
		memcpy(table + pos, ctx->str_table[i], size);
		pos += size;
	}
	
	/* replace the table in the context's data */
	before_table_data_size = ctx->str_table_start;
	after_table_data_size = ctx->length - ctx->str_table_end;
	before_table_data = malloc(before_table_data_size);
	after_table_data = malloc(after_table_data_size);
	memcpy(before_table_data, ctx->data, before_table_data_size);
	memcpy(after_table_data, ctx->data + ctx->str_table_end, after_table_data_size);
	ctx->length = before_table_data_size + after_table_data_size + table_len;
	ctx->data = malloc(ctx->length);
	memcpy(ctx->data, before_table_data, before_table_data_size);
	memcpy(ctx->data + before_table_data_size, table, table_len);
	memcpy(ctx->data + before_table_data_size + table_len, after_table_data, after_table_data_size);
	free(after_table_data);
	free(before_table_data);
	free(table);
}

static void process(context_t* ctx)
{
	int ver_minor = read_ui16(ctx);
	int ver_major = read_ui16(ctx);
	/* check ABC version */
	if (ver_major != 46) {
		ctx->error = 1;
		return;
	}
	if (ver_minor); /* to avoid getting an unused variable name */
	parse_constant_pool(ctx);
	rebuild_names(ctx);
	rebuild_string_table(ctx);
}

static void release_context(context_t* ctx)
{
	if (ctx->str_table) {
		size_t i;
		for (i=0; i<ctx->str_table_size; i++) free(ctx->str_table[i]);
		free(ctx->str_table);
	}
	if (ctx->name_table) free(ctx->name_table);
}

int ob_abc_obfuscate(swf_t* swf, unsigned char* abc, size_t abclen, unsigned char** out_abc, size_t* out_len)
{
	context_t ctx;
	memset(&ctx, 0, sizeof(context_t));
	ctx.swf = swf;
	ctx.data = abc;
	ctx.length = abclen;
	ctx.pos = 0;
	ctx.error = 0;
	ctx.name_table = NULL;
	ctx.name_table_size = 0;
	process(&ctx);
	*out_abc = ctx.data;
	*out_len = ctx.length;
	release_context(&ctx);
	return !ctx.error;
}

