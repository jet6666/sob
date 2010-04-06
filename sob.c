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

#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "ignorsym.h"
#include "swf.h"
#include "options.h"
#include "ob_abc.h"

static int retcode = 0;
static int quiet = 0;
static char* outfile = NULL;

static int obfuscate_abc(swf_t* swf, unsigned char* abc, size_t length, int skipflagsname, unsigned char** out_data, size_t* out_length)
{
	int base = 0;
	if (skipflagsname) { /* tag 82 has extra fields we don't care about */
		base += 4;
		while (abc[base] != 0 && base < length) base++;
		base++;
	}
	if (!ob_abc_obfuscate(swf, abc + base, length - base, out_data, out_length)) {
		fprintf(stderr, "sod: failed to obfuscate the code (invalid ABC data?)\n");
		retcode = 1;
		return 0;
	}
	if (skipflagsname && base > 0) { /* put back the prefix in the out_data */
		size_t new_length = base + (*out_length);
		unsigned char* new_data = malloc(new_length);
		memcpy(new_data, abc, base);
		memcpy(new_data + base, *out_data, *out_length);
		free(*out_data);
		*out_length = new_length;
		*out_data = new_data;
	}
	return 1;
}

static void begin_obfuscation(swf_t* swf)
{
	swf_tag_t* tag = swf->first_tag;
	int code_tag_found = 0;
	/* scan tags for known code definition tags and call the appropriate 
	 * obfuscation function (note: this is for future expansion when at some
	 * point sob supports AS2 scripts) */
	while (tag) {
		unsigned char* data;
		size_t length;
		
		/* check for either tag 72 or 82 */
		if (tag->type == 82 || tag->type == 72) {
			code_tag_found = 1;
			if (obfuscate_abc(swf, tag->data, tag->length, tag->type == 82, &data, &length)) {
				swf_replace_tag_data(tag, data, length);
				free(data);
			}
			break;
		}
		tag = tag->next;
	}
	if (!code_tag_found) {
		fprintf(stderr, "sob: swf file does not contain a tag with ABC code\n");
		retcode = 1;
	}
}

static void dump_swf_tags(swf_t* swf, const char* prefix)
{
	swf_tag_t* tag;
	int idx = 0;
	for (tag = swf->first_tag; tag; tag = tag->next) {
		char* fname = malloc(strlen(prefix) + 256);
		FILE* f;
		sprintf(fname, "%s-%04i-%03i.bin", prefix, idx, tag->type);
		f = fopen(fname, "wb");
		if (f) {
			if (!quiet) printf("sob: dumping #%i tag with code %i, data %i bytes\n", idx, tag->type, (int)tag->length);
			fwrite(tag->data, 1, tag->length, f);
			fclose(f);
		} else fprintf(stderr, "sob: failed to create '%s'\n", fname);
		free(fname);
		idx++;
	}
	if (!quiet) printf("sob: dumped %i tags\n", idx);
}

static void load_default_symbols(const char* argv0)
{
	char* xpath = NULL;
	char* symbols_filename;
	/* if the program's execution contain a path, use this path to search for
	 * the symbols file */
	if (strchr(argv0, '/') || strchr(argv0, '\\')) {
		int i;
		xpath = strclone(argv0);
		for (i = strlen(argv0) - 1; i>=0; i--) {
			if (xpath[i] == '/' || xpath[i] == '\\') {
				xpath[i] = 0;
				break;
			}
		}
	} else { /* ...otherwise scan the PATH environment variable for the program */
		char* path = getenv("PATH");
		char* entry = malloc(strlen(path) + 1);
		char* tmpfn;
		int h = 0, el = 0;
		#ifdef WIN32
		/* under win32 a program can be executed with or without the extension
		 * so we must add the extension if is missing */
		char* exefile = strclone(argv0);
		for (h=0; exefile[h]; h++) exefile[h] = tolower(exefile[h]);
		if (!strstr(exefile, ".exe")) {
			char* tmp = malloc(strlen(exefile) + 5);
			sprintf(tmp, "%s.exe", exefile);
			free(exefile);
			exefile = tmp;
		}
		#else
		char* exefile = strclone(argv0);
		#endif
		/* scan the path */
		tmpfn = malloc(strlen(path) + strlen(exefile) + 2);
		while (xpath == NULL && path[h]) {
			#ifdef WIN32
			if (path[h] == ';') {
			#else
			if (path[h] == ':') {
			#endif
				entry[el] = 0;
				if (el > 0 && entry[el-1] == '/') entry[el-1] = 0;
				sprintf(tmpfn, "%s/%s", entry, exefile);
				if (file_exists(tmpfn)) {
					xpath = strclone(entry);
				}
				el = 0;
			} else if (path[h] != '"') entry[el++] = path[h]=='\\'?'/':path[h];
			h++;
		}
		if (xpath == NULL && el > 0) {
			entry[el] = 0;
			sprintf(tmpfn, "%s/%s", entry, exefile);
			if (file_exists(tmpfn)) {
				xpath = entry;
			}
		}
		free(entry);
		free(tmpfn);
		free(exefile);
		/* if no file in the path was found, use the current directory */
		if (xpath == NULL) xpath = strclone(".");
	}
	/* load the symbols file */
	symbols_filename = malloc(strlen(xpath) + 13);
	sprintf(symbols_filename, "%s/symbols.txt", xpath);
	free(xpath);
	if (!file_exists(symbols_filename)) {
		fprintf(stderr, "sob: warning: default symbols file was not found in '%s', all names will be obfuscated!!!\n", symbols_filename);
	} else {
		printf("sob: loading default symbols file '%s': ", symbols_filename);
		ignore_load_file(symbols_filename);
		printf("ok\n");
	}
	free(symbols_filename);
}

static void show_help(void)
{
	printf("sob, the swf obfuscator\n");
	printf("version 0.2 beta Copyright (C) 2008 Kostas Michalopoulos\n\n");
	printf("usage: sob [options] <file1.swf> [file2.swf [...]]\n\n");
	printf("options are:\n");
	printf("--help                      this screen\n");
	printf("--version or -V             version info\n");
	printf("--skip-symbols-file         do not load the default symbols file\n");
	printf("--ignore-file or -I <file>  load the given ignore file (see below)\n");
	printf("--ignore or -i <name>       ignore (do not obfuscate) the given name\n");
	printf("--deignore or -d <name>     remove the given name from the ignore list\n");
	printf("--quiet or -q               do not produce output in stdout\n");
	printf("--output or -o <filename>   use the given filename for output for the next file\n");
	printf("--dont-save                 do everything but dont save the output file\n");
	printf("--dump-tags <prefix>        dump the swf file tags in separate files using the\n");
	printf("                            filename format <prefix>-<index>-<tagcode>.bin\n");
	printf("--obfuscate-mask <mask>	    set the obfuscation mask. <mask> is a combination of\n");
	printf("                            one or more of the following letters:\n");
	printf("                              q - obfuscate qualified names\n");
	printf("                              r - obfuscate runtime qualified names\n");
	printf("                              R - obfuscate runtime multinames\n");
	printf("                              n - obfuscate namespace names\n");
	printf("                            by default sob will obfuscate all of the above\n");
	printf("--show-renames              show the renamed symbols\n");
	printf("\nIgnore files:\n");
	printf("Ignore files are text files with one name per line. The names will be\n");
	printf("ignored by the obfuscator. If a line begins with + then the name is removed\n");
	printf("from the ignore list. Use this to obfuscate names defined in the default\n");
	printf("symbols file (make sure you know what you're doing).\n");
	printf("\nVisit http://www.badsectoracula.com/projects/sob/ for the latest version\n");
}

static void parse_arguments(int argn, char** argv)
{
	int i, produced = 0, skip_symbols = 0;
	int dump_tags = 0, dontsave = 0;
	int showrenames = 0;
	char* dump_tag_prefix = "";
	
	for (i=1; i<argn; i++) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--help")) {
				show_help();
				exit(0);
			} else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
				printf("sob version 0.2 beta Copyright (C) 2008 Kostas Michalopoulos\n");
				printf("Visit http://www.badsectoracula.com/projects/sob/ for the latest version\n");
				exit(0);
			} else if (!strcmp(argv[i], "--skip-symbols-file")) {
				skip_symbols = 1;
				if (!quiet) printf("sob: warning: skipping default symbols file\n");
			} else if (!strcmp(argv[i], "--ignore-file") || !strcmp(argv[i], "-I")) {
				ignore_load_file(argv[++i]);
			} else if (!strcmp(argv[i], "--ignore") || !strcmp(argv[i], "-i")) {
				ignore_add(argv[++i]);
			} else if (!strcmp(argv[i], "--deignore") || !strcmp(argv[i], "-d")) {
				ignore_remove(argv[++i]);
			} else if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q")) {
				quiet = 1;
			} else if (!strcmp(argv[i], "--output") || !strcmp(argv[i], "-o")) {
				outfile = argv[++i];
			} else if (!strcmp(argv[i], "--dont-save")) {
				dontsave = 1;
			} else if (!strcmp(argv[i], "--show-renames")) {
				showrenames = 1;
			} else if (!strcmp(argv[i], "--obfuscate-mask")) {
				opt_parse_obfuscate_mask_string(argv[++i]);
			} else if (!strcmp(argv[i], "--dump-tags")) {
				dump_tags = 1;
				dump_tag_prefix = argv[++i];
			} else {
				fprintf(stderr, "sob: unknown argument '%s'\n", argv[i]);
			}
		} else {
			char* swffile = outfile;
			swf_t* swf;
			if (!produced && !skip_symbols) {
				load_default_symbols(argv[0]);
			}
			produced = 1;
			swf = swf_read(argv[i]);
			if (!swf) {
				fprintf(stderr, "sob: failed to read swf file '%s'\n", argv[i]);
				exit(1);
			}
			if (!quiet) {
				printf("sob: processing %s swf '%s', version %i, body length %i\n",
					swf->compressed?"compressed":"uncompressed",
					swf->filename,
					swf->version,
					(int)swf->body_length);
				printf("sob: movie area=%i,%i -> %i,%i ",
					(int)(swf->rect.xmin/20), (int)(swf->rect.ymin/20),
					(int)(swf->rect.xmax/20), (int)(swf->rect.ymax/20));
				printf("rate=%i.%i frames=%i tags=%i\n", (int)(swf->rate>>8), (int)(swf->rate&0xFF),
					(int)swf->frame_count, (int)swf->tag_count);
			}
			
			if (dump_tags) {
				dump_swf_tags(swf, dump_tag_prefix);
				swf_free(swf);
				exit(0);
			}
			
			begin_obfuscation(swf);
			
			if (showrenames) {
				int i;
				for (i=0; i<swf->renames; i++) {
					printf("sob: rename: '%s' => '%s'\n", swf->rename[i].old_name, swf->rename[i].new_name);
				}
				printf("sob: %i renames total\n", swf->renames);
			}
			
			if (!swffile) {
				swffile = malloc(strlen(argv[i]) + 15);
				sprintf(swffile, "obfuscated_%s", argv[i]);
			}
			
			if (!dontsave) {
				if (!swf_write(swf, swffile)) {
					fprintf(stderr, "sob: failed to write swf file '%s'\n", swffile);
				} else {
					if (!quiet) printf("sob: wrote swf file '%s' with body length %i\n", swffile, (int)swf->body_length);
				}
			} else {
				if (!quiet) printf("sob: pretending i wrote swf file '%s' with body length %i\n", swffile, (int)swf->body_length);
			}
				
			
			if (!outfile) free(swffile);
			outfile = NULL;
			
			swf_free(swf);
		}
	}
	if (!produced) {
		printf("sob: no files specified. Use --help for usage information.\n");
	}
}

int main(int argn, char** argv)
{
	ignore_init();
	parse_arguments(argn, argv);
	
	ignore_shutdown();
	return retcode;
}

