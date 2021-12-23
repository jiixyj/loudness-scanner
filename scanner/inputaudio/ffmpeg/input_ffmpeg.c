/* See COPYING file for copyright and license details. */

#include <libavformat/avformat.h>
#if LIBAVCODEC_VERSION_MAJOR >= 53
#if LIBAVUTIL_VERSION_MAJOR > 52
#include <libavutil/channel_layout.h>
#else
#include <libavutil/audioconvert.h>
#endif
#endif
#include <gmodule.h>

#include "ebur128.h"
#include "input.h"

#ifndef AV_INPUT_BUFFER_PADDING_SIZE
#define AV_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
#endif

#define BUFFER_SIZE (192000 + AV_INPUT_BUFFER_PADDING_SIZE)

static GStaticMutex ffmpeg_mutex = G_STATIC_MUTEX_INIT;

struct input_handle {
	AVFormatContext *format_context;
	AVCodecContext *codec_context;
	AVCodec *codec;
	AVFrame *frame;
	AVPacket packet;
	AVPacket orig_packet;
	int audio_stream;
	int flushing;
	int got_frame;
	int packet_left;
	float buffer[BUFFER_SIZE / 2 + 1];
};

static unsigned
ffmpeg_get_channels(struct input_handle *ih)
{
	return (unsigned)ih->codec_context->channels;
}

static unsigned long
ffmpeg_get_samplerate(struct input_handle *ih)
{
	return (unsigned long)ih->codec_context->sample_rate;
}

static float *
ffmpeg_get_buffer(struct input_handle *ih)
{
	return ih->buffer;
}

static struct input_handle *
ffmpeg_handle_init()
{
	struct input_handle *ret;
	ret = malloc(sizeof(struct input_handle));

	return ret;
}

static void
ffmpeg_handle_destroy(struct input_handle **ih)
{
	free(*ih);
	*ih = NULL;
}


static int
ffmpeg_open_file(struct input_handle *ih, char const *filename)
{
	size_t j;

	g_static_mutex_lock(&ffmpeg_mutex);
	ih->format_context = NULL;

	if (avformat_open_input(&ih->format_context, filename, NULL, NULL) !=
	    0) {
		fprintf(stderr, "Could not open input file!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		return 1;
	}
	if (avformat_find_stream_info(ih->format_context, 0) < 0) {
		fprintf(stderr, "Could not find stream info!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		goto close_file;
	}
	// av_dump_format(ih->format_context, 0, "blub", 0);

	// Find the first audio stream
	ih->audio_stream = -1;
	for (j = 0; j < ih->format_context->nb_streams; ++j) {
		if (ih->format_context->streams[j]->codec->codec_type ==
		    AVMEDIA_TYPE_AUDIO) {
			ih->audio_stream = (int)j;
			break;
		}
	}
	if (ih->audio_stream == -1) {
		fprintf(stderr, "Could not find an audio stream in file!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		goto close_file;
	}
	// Get a pointer to the codec context for the audio stream
	ih->codec_context =
	    ih->format_context->streams[ih->audio_stream]->codec;

	ih->codec_context->request_sample_fmt = AV_SAMPLE_FMT_FLT;

	// Ignore Opus gain when decoding.
	if (ih->codec_context->codec_id == AV_CODEC_ID_OPUS &&
	    ih->codec_context->extradata_size >= 18) {
		ih->codec_context->extradata[16] =
		    ih->codec_context->extradata[17] = 0;
	}

	ih->codec = avcodec_find_decoder(ih->codec_context->codec_id);
	if (ih->codec == NULL) {
		fprintf(stderr,
		    "Could not find a decoder for the audio format!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		goto close_file;
	}

	char *float_codec = g_malloc(
	    strlen(ih->codec->name) + sizeof("float") + 1);
	sprintf(float_codec, "%sfloat", ih->codec->name);
	AVCodec *possible_float_codec = avcodec_find_decoder_by_name(
	    float_codec);
	if (possible_float_codec)
		ih->codec = possible_float_codec;
	g_free(float_codec);

	// Open codec
	if (avcodec_open2(ih->codec_context, ih->codec, NULL) < 0) {
		fprintf(stderr, "Could not open the codec!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		goto close_file;
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 28, 1)
	ih->frame = av_frame_alloc();
#else
	ih->frame = avcodec_alloc_frame();
#endif
	if (!ih->frame) {
		fprintf(stderr, "Could not allocate frame!\n");
		g_static_mutex_unlock(&ffmpeg_mutex);
		goto close_file;
	}

	av_init_packet(&ih->packet);
	ih->packet.data = NULL;
	ih->orig_packet.size = 0;

	g_static_mutex_unlock(&ffmpeg_mutex);

	ih->flushing = 0;
	ih->got_frame = 0;
	ih->packet_left = 0;

	return 0;

close_file:
	g_static_mutex_lock(&ffmpeg_mutex);
	avformat_close_input(&ih->format_context);
	g_static_mutex_unlock(&ffmpeg_mutex);
	return 1;
}

static int
ffmpeg_set_channel_map(struct input_handle *ih, int *st)
{
	if (ih->codec_context->channel_layout) {
		unsigned int channel_map_index = 0;
		int bit_counter = 0;
		while (
		    channel_map_index < (unsigned)ih->codec_context->channels) {
			if (ih->codec_context->channel_layout &
			    (1 << bit_counter)) {
				switch (1 << bit_counter) {
				case AV_CH_FRONT_LEFT:
					st[channel_map_index] = EBUR128_LEFT;
					break;
				case AV_CH_FRONT_RIGHT:
					st[channel_map_index] = EBUR128_RIGHT;
					break;
				case AV_CH_FRONT_CENTER:
					st[channel_map_index] = EBUR128_CENTER;
					break;
				case AV_CH_BACK_LEFT:
					st[channel_map_index] =
					    EBUR128_LEFT_SURROUND;
					break;
				case AV_CH_BACK_RIGHT:
					st[channel_map_index] =
					    EBUR128_RIGHT_SURROUND;
					break;
				default:
					st[channel_map_index] = EBUR128_UNUSED;
					break;
				}
				++channel_map_index;
			}
			++bit_counter;
		}
		return 0;
	} else {
		return 1;
	}
}

static int
ffmpeg_allocate_buffer(struct input_handle *ih)
{
	(void)ih;
	return 0;
}

static size_t
ffmpeg_get_total_frames(struct input_handle *ih)
{
	double tmp =
	    (double)ih->format_context->streams[ih->audio_stream]->duration *
	    (double)ih->format_context->streams[ih->audio_stream]
		->time_base.num /
	    (double)ih->format_context->streams[ih->audio_stream]
		->time_base.den *
	    (double)ih->codec_context->sample_rate;

	if (tmp <= 0.0) {
		return 0;
	} else {
		return (size_t)(tmp + 0.5);
	}
}

static int
decode_packet(struct input_handle *ih)
{
	int ret = 0;
	ret = avcodec_decode_audio4(ih->codec_context, ih->frame,
	    &ih->got_frame, &ih->packet);
	if (ret < 0) {
		fprintf(stderr, "Error in decoder!\n");
		return ret;
	}

	return FFMIN(ret, ih->packet.size);
}

static size_t
ffmpeg_read_one_packet(struct input_handle *ih)
{
start:
	if (ih->flushing) {
		ih->packet.data = NULL;
		ih->packet.size = 0;
		decode_packet(ih);
		if (!ih->got_frame) {
			return 0;
		} else {
			goto write_to_buffer;
		}
	}

	if (ih->packet_left)
		goto packet_left;

next_frame:
	for (;;) {
		if (av_read_frame(ih->format_context, &ih->packet) < 0) {
			ih->flushing = 1;
			goto start;
		}
		if (ih->packet.stream_index != ih->audio_stream) {
			av_free_packet(&ih->packet);
			continue;
		} else {
			break;
		}
	}

	int ret;
	ih->orig_packet = ih->packet;
packet_left:
	ret = decode_packet(ih);
	if (ret < 0)
		goto free_packet;
	ih->packet.data += ret;
	ih->packet.size -= ret;
	if (ih->packet.size > 0) {
		ih->packet_left = 1;
	} else {
	free_packet:
		av_free_packet(&ih->orig_packet);
		ih->packet_left = 0;
	}

	if (!ih->got_frame)
		goto start;

write_to_buffer:;
	int nr_frames_read = ih->frame->nb_samples;
	uint8_t **ed = ih->frame->extended_data;
	uint8_t *data = ed[0];

	int16_t *data_short = (int16_t *)data;
	int32_t *data_int = (int32_t *)data;
	float *data_float = (float *)data;
	double *data_double = (double *)data;

	/* TODO: fix this */
	int channels = ih->codec_context->channels;
	// channels = ih->frame->channels;

	if (ih->frame->nb_samples * channels > sizeof ih->buffer) {
		fprintf(stderr, "buffer too small!\n");
		return 0;
	}

	int i;
	switch (ih->frame->format) {
	case AV_SAMPLE_FMT_U8:
		fprintf(stderr, "8 bit audio not supported by libebur128!\n");
		nr_frames_read = 0;
		goto out;
	case AV_SAMPLE_FMT_S16:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			ih->buffer[i] = ((float)data_short[i]) /
			    MAX(-(float)SHRT_MIN, (float)SHRT_MAX);
		}
		break;
	case AV_SAMPLE_FMT_S32:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			ih->buffer[i] = ((float)data_int[i]) /
			    MAX(-(float)INT_MIN, (float)INT_MAX);
		}
		break;
	case AV_SAMPLE_FMT_FLT:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			ih->buffer[i] = data_float[i];
		}
		break;
	case AV_SAMPLE_FMT_DBL:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			ih->buffer[i] = (float)data_double[i];
		}
		break;
	case AV_SAMPLE_FMT_S16P:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			int current_channel = i / ih->frame->nb_samples;
			int current_sample = i % ih->frame->nb_samples;
			ih->buffer[current_sample * channels +
			    current_channel] =
			    ((float)((int16_t *)
				    ed[current_channel])[current_sample]) /
			    MAX(-(float)SHRT_MIN, (float)SHRT_MAX);
		}
		break;
	case AV_SAMPLE_FMT_S32P:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			int current_channel = i / ih->frame->nb_samples;
			int current_sample = i % ih->frame->nb_samples;
			ih->buffer[current_sample * channels +
			    current_channel] =
			    ((float)((int32_t *)
				    ed[current_channel])[current_sample]) /
			    MAX(-(float)INT_MIN, (float)INT_MAX);
		}
		break;
	case AV_SAMPLE_FMT_FLTP:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			int current_channel = i / ih->frame->nb_samples;
			int current_sample = i % ih->frame->nb_samples;
			ih->buffer[current_sample * channels +
			    current_channel] =
			    ((float *)ed[current_channel])[current_sample];
		}
		break;
	case AV_SAMPLE_FMT_DBLP:
		for (i = 0; i < ih->frame->nb_samples * channels; ++i) {
			int current_channel = i / ih->frame->nb_samples;
			int current_sample = i % ih->frame->nb_samples;
			ih->buffer[current_sample * channels +
			    current_channel] =
			    ((double *)ed[current_channel])[current_sample];
		}
		break;
	default:
		fprintf(stderr, "Unknown sample format!\n");
		nr_frames_read = 0;
		goto out;
	}
out:
	return nr_frames_read;
}

static size_t
ffmpeg_read_frames(struct input_handle *ih)
{
	return ffmpeg_read_one_packet(ih);
}

static void
ffmpeg_free_buffer(struct input_handle *ih)
{
	(void)ih;
}

static void
ffmpeg_close_file(struct input_handle *ih)
{
	g_static_mutex_lock(&ffmpeg_mutex);
	avcodec_close(ih->codec_context);
	avformat_close_input(&ih->format_context);
	g_static_mutex_unlock(&ffmpeg_mutex);
}

static int
ffmpeg_init_library(void)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	// Register all formats and codecs
	av_register_all();
#endif
	av_log_set_level(AV_LOG_ERROR);
	return 0;
}

static void
ffmpeg_exit_library(void)
{
}

G_MODULE_EXPORT struct input_ops ip_ops = { ffmpeg_get_channels,
	ffmpeg_get_samplerate, ffmpeg_get_buffer, ffmpeg_handle_init,
	ffmpeg_handle_destroy, ffmpeg_open_file, ffmpeg_set_channel_map,
	ffmpeg_allocate_buffer, ffmpeg_get_total_frames, ffmpeg_read_frames,
	ffmpeg_free_buffer, ffmpeg_close_file, ffmpeg_init_library,
	ffmpeg_exit_library };

G_MODULE_EXPORT char const *ip_exts[] = { "wav", "flac", "ogg", "oga", "mp3",
	"mp2", "mpc", "ac3", "wv", "mpg", "avi", "mkv", "m4a", "mp4", "aac",
	"mov", "mxf", "opus", "w64", NULL };
