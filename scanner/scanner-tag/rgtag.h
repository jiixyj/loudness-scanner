/* See COPYING file for copyright and license details. */

#ifndef RGTAG_H_
#define RGTAG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct gain_data {
  double track_gain;
  double track_peak;
  int album_mode;
  double album_gain;
  double album_peak;
};

void clamp_gain_data(struct gain_data* gd);

int set_rg_info(const char* filename,
                const char* extension,
                struct gain_data* gd,
                int opus_compat);

int has_rg_info(const char* filename,
                const char* extension);

void adjust_with_file_gain(struct gain_data* gd,
                           const char* filename,
                           const char* extension);

#ifdef __cplusplus
}
#endif

#endif  /* RGTAG_H_ */
