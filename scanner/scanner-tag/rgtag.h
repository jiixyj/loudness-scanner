/* See COPYING file for copyright and license details. */

#ifndef RGTAG_H_
#define RGTAG_H_

#include <stdbool.h>

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

void clamp_gain_data(struct gain_data *gd);

int set_rg_info(char const *filename, char const *extension,
    struct gain_data *gd, int opus_compat);

bool has_rg_info(char const *filename, char const *extension, int opus_compat);

#ifdef __cplusplus
}
#endif

#endif /* RGTAG_H_ */
