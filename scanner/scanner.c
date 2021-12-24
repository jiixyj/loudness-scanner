/* See COPYING file for copyright and license details. */

#ifdef G_OS_WIN32
#include <windows.h>
#endif
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "filetree.h"
#include "input.h"
#include "parse_args.h"

#include "scanner-scan.h"
#ifdef USE_TAGLIB
#include "scanner-tag.h"
#endif
#include "scanner-common.h"
#include "scanner-dump.h"

/* knobs: USE_TAGLIB, USE_SNDFILE */


static void
print_help(void)
{
	printf(
	    "Usage: loudness scan|tag|dump|--version [OPTION...] [FILE|DIRECTORY]...\n");
	printf("\n");
	printf(
	    "`loudness' scans audio files according to the EBU R128 standard. It can output\n");
#ifdef USE_TAGLIB
	printf(
	    "loudness and peak information, write it to ReplayGain conformant tags, or dump\n");
	printf(
	    "momentary/shortterm/integrated loudness in fixed intervals to the console.\n");
#else
	printf(
	    "loudness and peak information, or dump momentary/shortterm/integrated loudness\n");
	printf("in fixed intervals to the console.\n");
#endif
	printf("\n");
	printf("Examples:\n");
	printf(
	    "  loudness scan foo.wav       # Scans foo.wav and writes information to stdout.\n");
#ifdef USE_TAGLIB
	printf(
	    "  loudness tag -r bar/        # Tag all files in foo as one album per subfolder.\n");
#endif
	printf(
	    "  loudness dump -m 1.0 a.wav  # Each second, write momentary loudness to stdout.\n");
	printf(
	    "  loudness --version          # Write library and scanner version to stdout.\n");
	printf("\n");
	printf(" Main operation mode:\n");
	printf(
	    "  scan                       output loudness and peak information\n");
#ifdef USE_TAGLIB
	printf(
	    "  tag                        tag files with ReplayGain conformant tags\n");
#endif
	printf(
	    "  dump                       output momentary/shortterm/integrated loudness\n");
	printf("                             in fixed intervals\n");
	printf("\n");
	printf(" Global options:\n");
	printf(
	    "  -r, --recursive            recursively scan files in subdirectories\n");
	printf(
	    "  -L, --follow-symlinks      follow symbolic links (*nix only)\n");
	printf("  -v, --verbose              verbose error output\n");
	printf(
	    "  --histogram                use histogram loudness algorithm (needs less RAM)\n");
	printf(
	    "  --no-sort                  do not sort command line arguments alphabetically\n");
	printf(
	    "  --force-plugin=PLUGIN      force input plugin; PLUGIN is one of:\n");
	printf(/**/
	    "                             sndfile, ffmpeg\n");
#ifdef USE_SNDFILE
	printf(
	    "  --decode=FILE              decode one input to FILE (32 bit float WAV,\n");
	printf(
	    "                             only available in scan and tag mode)\n");
#endif
	printf("\n");
	printf(" Scan options:\n");
	printf(
	    "  -l, --lra                  calculate loudness range in LRA\n");
	printf(
	    "  -p, --peak=sample|true|dbtp|all  -p sample: sample peak (float value)\n");
	printf(
	    "                                   -p true:   true peak (float value)\n");
	printf(
	    "                                   -p dbtp:   true peak (dB True Peak)\n");
	printf(
	    "                                   -p all:    show all peak values\n");
	printf("\n");
#ifdef USE_TAGLIB
	printf(" Tag options:\n");
	printf(
	    "  -t, --track                write only track gain (album gain is default)\n");
	printf(
	    "  -n, --dry-run              perform a trial run with no changes made\n");
	printf(
	    "  --incremental              skip files that are already tagged\n");
	printf(
	    "  --force-as-album           treat all given files as one album\n");
	printf(
	    "  --opus-vorbisgain-compat   for compatibility with older software,\n");
	printf(
	    "                             write 'REPLAYGAIN_*' tags to Opus files\n");
	printf(/**/
	    "                             in addition to 'R128_*' tags\n");
	printf(
	    "  --opus-header-gain=ABS_DB|r128[,track][,offset=DB]|rg[,track][,offset=DB]\n");
	printf(/**/
	    "                             write specific values into Opus header gain field,\n");
	printf(/**/
	    "                             adjusting 'R128_*'/'REPLAYGAIN_*' tag values\n");
	printf(/**/
	    "                             as needed\n");
	printf(/**/
	    "                               examples:\n");
	printf(/**/
	    "                                   --opus-header-gain=r128\n");
	printf(/**/
	    "                                       R128 album/track gain (depending\n");
	printf(/**/
	    "                                       on '-t' parameter) in gain field,\n");
	printf(/**/
	    "                                       this setting is the default\n");
	printf(/**/
	    "                                   --opus-header-gain=r128,track\n");
	printf(/**/
	    "                                       R128 track gain in gain field\n");
	printf(/**/
	    "                                   --opus-header-gain=rg\n");
	printf(/**/
	    "                                       ReplayGain compatible album/track gain\n");
	printf(/**/
	    "                                       (depending on '-t' parameter) in\n");
	printf(/**/
	    "                                       gain field\n");
	printf(/**/
	    "                                   --opus-header-gain=rg,offset=3\n");
	printf(/**/
	    "                                       ReplayGain compatible album/track gain\n");
	printf(/**/
	    "                                       (depending on '-t' parameter) in\n");
	printf(/**/
	    "                                       gain field, plus 3dB\n");
	printf(/**/
	    "                                   --opus-header-gain=0:   0dB in gain field\n");
	printf(/**/
	    "                                   --opus-header-gain=2:   2dB in gain field\n");
	printf(/**/
	    "                                   --opus-header-gain=-3:  -3dB in gain field\n");
	printf("\n");
#endif
	printf(" Dump options:\n");
	printf(
	    "  -m, --momentary=INTERVAL   print momentary loudness every INTERVAL seconds\n");
	printf(
	    "  -s, --shortterm=INTERVAL   print shortterm loudness every INTERVAL seconds\n");
	printf(
	    "  -i, --integrated=INTERVAL  print integrated loudness every INTERVAL seconds\n");
}

static gboolean recursive = FALSE;
static gboolean follow_symlinks = FALSE;
static gboolean no_sort = FALSE;
gboolean verbose = FALSE;
gboolean histogram = FALSE;
static gchar *forced_plugin = NULL;
gchar *decode_to_file = NULL;
static gboolean help = FALSE;

static GOptionEntry entries[] = { { "recursive", 'r', 0, G_OPTION_ARG_NONE,
				      &recursive, NULL, NULL },
	{ "follow-symlinks", 'L', 0, G_OPTION_ARG_NONE, &follow_symlinks, NULL,
	    NULL },
	{ "no-sort", 0, 0, G_OPTION_ARG_NONE, &no_sort, NULL, NULL },
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, NULL, NULL },
	{ "histogram", 0, 0, G_OPTION_ARG_NONE, &histogram, NULL, NULL },
	{ "force-plugin", 0, 0, G_OPTION_ARG_STRING, &forced_plugin, NULL,
	    NULL },
#ifdef USE_SNDFILE
	{ "decode", 0, 0, G_OPTION_ARG_STRING, &decode_to_file, NULL, NULL },
#endif
	{ "help", 'h', 0, G_OPTION_ARG_NONE, &help, NULL, NULL },
	{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 } };

enum modes { LOUDNESS_MODE_SCAN, LOUDNESS_MODE_TAG, LOUDNESS_MODE_DUMP };

int
main(int argc, char *argv[])
{
	GSList *errors = NULL;
	GSList *files = NULL;
	Filetree tree;
	int mode = 0;
	int mode_parsed = FALSE;
	int ret = 0;

	if (parse_global_args(&argc, &argv, entries, TRUE) || argc < 2 ||
	    help) {
		print_help();
		exit(EXIT_FAILURE);
	}
	if (!strcmp(argv[1], "scan")) {
		mode = LOUDNESS_MODE_SCAN;
		mode_parsed = loudness_scan_parse(&argc, &argv);
#ifdef USE_TAGLIB
	} else if (!strcmp(argv[1], "tag")) {
		mode = LOUDNESS_MODE_TAG;
		mode_parsed = loudness_tag_parse(&argc, &argv);
#endif
	} else if (!strcmp(argv[1], "dump")) {
		mode = LOUDNESS_MODE_DUMP;
		mode_parsed = loudness_dump_parse(&argc, &argv);
	} else if (!strcmp(argv[1], "--version")) {
		print_version();
		exit(EXIT_SUCCESS);
	} else {
		fprintf(stderr, "Unknown mode '%s'\n", argv[1]);
	}
	if (!mode_parsed) {
		exit(EXIT_FAILURE);
	}
	if (decode_to_file && argc - 1 != 1) {
		fprintf(stderr, "Cannot decode more than one file\n");
		exit(EXIT_FAILURE);
	}

	input_init(argv[0], forced_plugin);
	scanner_init_common();

	setlocale(LC_COLLATE, "");
	setlocale(LC_CTYPE, "");
	tree = filetree_init(&argv[1], (size_t)(argc - 1), recursive,
	    follow_symlinks, no_sort, &errors);

	g_slist_foreach(errors, filetree_print_error, &verbose);
	g_slist_foreach(errors, filetree_free_error, NULL);
	g_slist_free(errors);

	filetree_file_list(tree, &files);
	filetree_remove_common_prefix(files);

	switch (mode) {
	case LOUDNESS_MODE_SCAN:
		loudness_scan(files);
		break;
#ifdef USE_TAGLIB
	case LOUDNESS_MODE_TAG:
		ret = loudness_tag(files);
		break;
#endif
	case LOUDNESS_MODE_DUMP:
		ret = loudness_dump(files);
		break;
	}

	g_slist_foreach(files, filetree_free_list_entry, NULL);
	g_slist_free(files);

	filetree_destroy(tree);
	input_deinit();
	g_free(forced_plugin);

	return ret;
}
