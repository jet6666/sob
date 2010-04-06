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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

void* malloc_zero(size_t size)
{
	void* ptr = malloc(size);
	if (ptr != NULL) {
		memset(ptr, 0, size);
	}
	return ptr;
}

char* strclone(const char* str)
{
	size_t len = strlen(str) + 1;
	char* clone = malloc(len);
	memcpy(clone, str, len);
	return clone;
}

int file_exists(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
}

char* int2str(long i, int base)
{
	static char buff[32] = {0};
	int idx = 30;
	for (; i&&idx;--idx,i/=base) buff[idx] = "0123456789abcdefghijklmnopqrstuvwxyz"[i%base];
	return buff + idx + 1;
}

