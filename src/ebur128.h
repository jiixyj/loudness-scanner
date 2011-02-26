/* See LICENSE file for copyright and license details. */
#ifndef _EBUR128_H_
#define _EBUR128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

/* This can be replaced by any BSD-like queue implementation */
#include "queue.h"

SLIST_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  SLIST_ENTRY(ebur128_dq_entry) entries;
};

/* Use these values when setting the channel map */
enum channels {
  EBUR128_UNUSED = 0,
  EBUR128_LEFT,
  EBUR128_RIGHT,
  EBUR128_CENTER,
  EBUR128_LEFT_SURROUND,
  EBUR128_RIGHT_SURROUND
};

/* Use these values in ebur128_init (xor'ed) */
enum mode {
  EBUR128_MODE_M         =  1, /* can call ebur128_loudness_momentary */
  EBUR128_MODE_S         =  3, /* can call ebur128_loudness_shortterm */
  EBUR128_MODE_I         =  5, /* can call ebur128_gated_loudness_*   */
  EBUR128_MODE_LRA       = 11  /* can call ebur128_loudness_range     */
};

typedef struct {
  int mode;
  double* audio_data;
  size_t audio_data_frames;
  size_t audio_data_index;
  size_t needed_frames;
  size_t channels;
  int* channel_map;
  size_t samplerate;
  size_t samples_in_200ms;
  double* a;
  double* b;
  double** v;
  struct ebur128_double_queue block_list;
  struct ebur128_double_queue short_term_block_list;
  size_t short_term_frame_counter;
} ebur128_state;

ebur128_state* ebur128_init(size_t channels, size_t samplerate, int mode);
int ebur128_destroy(ebur128_state** st);

/* Set channel "channel_number" (zero based index) to value, where value is
 * from the "channels" enum. Returns 1 if channel_number is invalid.
 * The default is: 0 - EBUR128_LEFT
 *                 1 - EBUR128_RIGHT
 *                 2 - EBUR128_CENTER
 *                 3 - EBUR128_UNUSED
 *                 4 - EBUR128_LEFT_SURROUND
 *                 5 - EBUR128_RIGHT_SURROUND  */
int ebur128_set_channel(ebur128_state* st, size_t channel_number, int value);
/* Note that the channel map will be reset when setting a different number of
 * channels. The current unfinished block will be lost.
 * Returns 1 on memory allocation error. The state will be invalid then.
 * Returns 2 if channels and samplerate were not changed. */
int ebur128_change_parameters(ebur128_state* st,
                              size_t channels,
                              size_t samplerate);

int ebur128_add_frames_short(ebur128_state* st,
                             const short* src,
                             size_t frames);
int ebur128_add_frames_int(ebur128_state* st,
                             const int* src,
                             size_t frames);
int ebur128_add_frames_float(ebur128_state* st,
                             const float* src,
                             size_t frames);
int ebur128_add_frames_double(ebur128_state* st,
                             const double* src,
                             size_t frames);

/* Get integrated loudness of the whole programme. Returns NaN if mode does not
 * contain EBUR128_MODE_I. */
double ebur128_loudness_global(ebur128_state* st);
/* Get integrated loudness of the whole programme across multiple instances.
   Returns NaN if mode does not contain EBUR128_MODE_I. */
double ebur128_loudness_global_multiple(ebur128_state** sts, size_t size);

/* Get momentary loudness (last 400ms) */
double ebur128_loudness_momentary(ebur128_state* st);
/* Get short-term loudness (last 3s). Will return NaN if mode does not contain
 * EBUR128_MODE_S. */
double ebur128_loudness_shortterm(ebur128_state* st);

/* Returns LRA according to EBU 3342. Will return NaN if memory allocation
 * fails or if mode does not contain EBUR128_MODE_LRA. */
double ebur128_loudness_range(ebur128_state* st);
/* Get LRA of the whole programme across multiple instances.
 * Returns NaN if memory allocation fails or mode does not contain
 * EBUR128_MODE_LRA. */
double ebur128_loudness_range_multiple(ebur128_state** sts, size_t size);

#ifdef __cplusplus
}
#endif

#endif  /* _EBUR128_H_ */
