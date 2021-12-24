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

typedef enum {
	OPUS_GAIN_REFERENCE_ABSOLUTE,
	OPUS_GAIN_REFERENCE_R128,
} OpusGainReference;

typedef struct {
	bool vorbisgain_compat;
	OpusGainReference opus_gain_reference;
	double offset;
	bool is_track;
} OpusTagInfo;

double clamp_rg(double x);
void clamp_gain_data(struct gain_data *gd);

int set_rg_info(char const *filename, char const *extension,
    struct gain_data *gd, OpusTagInfo const *opus_tag_info);

bool has_rg_info(char const *filename, char const *extension,
    OpusTagInfo const *opus_tag_info);

#ifdef __cplusplus
}
#endif

#endif /* RGTAG_H_ */
