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
#include <ctype.h>
#include "utils.h"
#include "ignorsym.h"

static char** names = NULL;
static size_t namec = 0;

void ignore_init(void)
{
}

void ignore_shutdown(void)
{
	if (names) {
		int i;
		for (i=0; i<namec; i++) free(names[i]);
		free(names);
	}
	names = NULL;
	namec = 0;
}

void ignore_load_file(const char* filename)
{
	char line[1024];
	FILE* f = fopen(filename, "rt");
	while ((fgets(line, sizeof(line), f))) {
		while (line[0] > 0 && isspace(line[strlen(line)-1])) line[strlen(line)-1] = 0;
		if (line[0] && line[0] != '#') {
			if (line[0] == '+')
				ignore_remove(line + 1);
			else
				ignore_add(line);
		}
	}
	fclose(f);
}

void ignore_add(const char* name)
{
	names = realloc(names, sizeof(char*)*(namec + 1));
	names[namec++] = strclone(name);
}

void ignore_remove(const char* name)
{
	int i;
	for (i=0; i<namec; i++) if (!strcmp(name, names[i])) {
		int j;
		free(names[i]);
		for (j=i; j<namec-1; j++) names[j] = names[j + 1];
		namec--;
		names = realloc(names, sizeof(char*)*namec);
	}
}

int ignore_check(const char* name)
{
	int i;
	if (!name[0] || !name[1]) return 0;
	for (i=0; i<namec; i++) {
		char* star = strchr(names[i], '*');
		if (star) {
			int star_pos = star - names[i];
			if (!strncmp(name, names[i], star_pos)) return 1;
		} else {
			if (!strcmp(name, names[i])) return 1;
		}
	}
	return 0;
}

