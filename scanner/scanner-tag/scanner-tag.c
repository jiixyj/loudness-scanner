/* See COPYING file for copyright and license details. */

#include "scanner-tag.h"

#include <stdio.h>
#include <stdlib.h>

#include "nproc.h"
#include "parse_args.h"
#include "rgtag.h"
#include "scanner-common.h"

static struct file_data empty;

extern gboolean verbose;
extern gboolean histogram;
static gboolean track = FALSE;
static gboolean dry_run = FALSE;
static gboolean incremental_tagging = FALSE;
static gboolean force_as_album = FALSE;
static gboolean opus_vorbisgain_compat = FALSE;
extern gchar *decode_to_file;

static GOptionEntry entries[] = {
	{ "track", 't', 0, G_OPTION_ARG_NONE, /**/
	    &track, NULL, NULL },
	{ "dry-run", 'n', 0, G_OPTION_ARG_NONE, /**/
	    &dry_run, NULL, NULL },
	{ "incremental", 0, 0, G_OPTION_ARG_NONE, /**/
	    &incremental_tagging, NULL, NULL },
	{ "force-as-album", 0, 0, G_OPTION_ARG_NONE, /**/
	    &force_as_album, NULL, NULL },
	{ "opus-vorbisgain-compat", 0, 0, G_OPTION_ARG_NONE, /**/
	    &opus_vorbisgain_compat, NULL, NULL },
	{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 },
};

static void
fill_album_data(struct filename_list_node *fln, double const *album_data)
{
	struct file_data *fd = (struct file_data *)fln->d;

	fd->gain_album = album_data[0];
	fd->peak_album = album_data[1];
}

static gchar *current_dir;
static GSList *files_in_current_dir;

static void
calculate_album_gain_and_peak_last_dir(void)
{
	double album_data[] = { 0.0, 0.0 };
	GPtrArray *states = g_ptr_array_new();
	struct file_data result;
	memcpy(&result, &empty, sizeof empty);

	files_in_current_dir = g_slist_reverse(files_in_current_dir);
	g_slist_foreach(files_in_current_dir, (GFunc)get_state, states);
	ebur128_loudness_global_multiple((ebur128_state **)states->pdata,
	    states->len, &album_data[0]);
	album_data[0] = RG_REFERENCE_LEVEL - album_data[0];
	g_slist_foreach(files_in_current_dir, (GFunc)get_max_peaks, &result);
	album_data[1] = result.peak;
	g_slist_foreach(files_in_current_dir, (GFunc)fill_album_data,
	    album_data);

	g_ptr_array_free(states, TRUE);

	g_free(current_dir);
	current_dir = NULL;
	g_slist_free(files_in_current_dir);
	files_in_current_dir = NULL;
}

static void
calculate_album_gain_and_peak(struct filename_list_node *fln, gpointer unused)
{
	gchar *dirname;

	(void)unused;
	dirname = g_path_get_dirname(fln->fr->raw);
	if (!current_dir) {
		current_dir = g_strdup(dirname);
	}
	if (!strcmp(current_dir, dirname)) {
		files_in_current_dir = g_slist_prepend(files_in_current_dir,
		    fln);
	} else {
		calculate_album_gain_and_peak_last_dir();
		current_dir = g_strdup(dirname);
		files_in_current_dir = g_slist_prepend(files_in_current_dir,
		    fln);
	}
	g_free(dirname);
}

/* must g_free basename and filename */
static void
get_filename_and_extension(struct filename_list_node *fln, char **basename,
    char **extension, char **filename)
{
	*basename = g_path_get_basename(fln->fr->raw);
	*extension = strrchr(*basename, '.');
	if (*extension) {
		++*extension;
	} else {
		*extension = "";
	}
#ifdef G_OS_WIN32
	*filename = (char *)g_utf8_to_utf16(fln->fr->raw, -1, NULL, NULL, NULL);
#else
	*filename = g_strdup(fln->fr->raw);
#endif
}

static void
print_file_data(struct filename_list_node *fln, gpointer unused)
{
	struct file_data *fd = (struct file_data *)fln->d;

	(void)unused;
	if (fd->scanned) {
		struct gain_data gd = {
			RG_REFERENCE_LEVEL - fd->loudness,
			fd->peak,
			!track,
			fd->gain_album,
			fd->peak_album,
		};

		char *basename;
		char *extension;
		char *filename;
		get_filename_and_extension(fln, &basename, &extension,
		    &filename);

		clamp_gain_data(&gd);

		g_free(basename);
		g_free(filename);

		if (!track) {
			g_print("%7.2f dB, %7.2f dB, %10.6f, %10.6f",
			    gd.album_gain, gd.track_gain, gd.album_peak,
			    gd.track_peak);
		} else {
			g_print("%7.2f dB, %10.6f", gd.track_gain,
			    gd.track_peak);
		}
		if (fln->fr->display[0]) {
			g_print(", ");
			print_utf8_string(fln->fr->display);
		}
		putchar('\n');
	}
}

static int tag_output_state = 0;
void
tag_file(struct filename_list_node *fln, int *ret)
{
	struct file_data *fd = (struct file_data *)fln->d;
	if (fd->scanned) {
		int error;

		struct gain_data gd = {
			RG_REFERENCE_LEVEL - fd->loudness,
			fd->peak,
			!track,
			fd->gain_album,
			fd->peak_album,
		};

		char *basename;
		char *extension;
		char *filename;
		get_filename_and_extension(fln, &basename, &extension,
		    &filename);

		error = set_rg_info(filename, extension, &gd,
		    opus_vorbisgain_compat);
		if (error) {
			if (tag_output_state == 0) {
				fflush(stderr);
				fputc('\n', stderr);
				tag_output_state = 1;
			}
			g_message("Error tagging %s", fln->fr->display);
			*ret = EXIT_FAILURE;
		} else {
			fputc('.', stderr);
			tag_output_state = 0;
		}

		g_free(basename);
		g_free(filename);
	}
}

int
scan_files(GSList *files)
{
	struct scan_opts opts = { FALSE, "sample", histogram, TRUE,
		decode_to_file };
	int do_scan = 0;

	g_slist_foreach(files, (GFunc)init_and_get_number_of_frames, &do_scan);
	if (do_scan) {

		process_files(files, &opts);

		if (!track) {
			if (force_as_album) {
				files_in_current_dir = g_slist_copy(files);
			} else {
				g_slist_foreach(files,
				    (GFunc)calculate_album_gain_and_peak, NULL);
			}
			calculate_album_gain_and_peak_last_dir();
		}

		clear_line();
		if (!track) {
			fprintf(stderr,
			    "Album gain, Track gain, Album peak, Track peak\n");
		} else {
			fprintf(stderr, "Track gain, Track peak\n");
		}
		g_slist_foreach(files, (GFunc)print_file_data, NULL);
	}
	g_slist_foreach(files, (GFunc)destroy_state, NULL);
	scanner_reset_common();

	return do_scan;
}

int
tag_files(GSList *files)
{
	int ret = 0;

	fprintf(stderr, "Tagging");
	g_slist_foreach(files, (GFunc)tag_file, &ret);
	if (!ret) {
		fprintf(stderr, " Success!");
	}
	fputc('\n', stderr);

	return ret;
}

static void
append_to_untagged_list(struct filename_list_node *fln, GSList **ret)
{
	char *basename;
	char *extension;
	char *filename;
	get_filename_and_extension(fln, &basename, &extension, &filename);

	if (!has_rg_info(filename, extension, opus_vorbisgain_compat)) {
		*ret = g_slist_prepend(*ret, fln);
	}

	g_free(basename);
	g_free(filename);
}

int
loudness_tag(GSList *files)
{
	if (incremental_tagging) {
		GSList *untagged_files = NULL;
		g_slist_foreach(files, (GFunc)append_to_untagged_list,
		    &untagged_files);
		untagged_files = g_slist_reverse(untagged_files);

		files = untagged_files;
	}

	if (scan_files(files) && !dry_run) {
		return tag_files(files);
	}
	return 0;
}

gboolean
loudness_tag_parse(int *argc, char **argv[])
{
	gboolean success = parse_mode_args(argc, argv, entries);
	if (!success) {
		if (*argc == 1) {
			fprintf(stderr, "Missing arguments\n");
		}
		return FALSE;
	}
	return TRUE;
}
