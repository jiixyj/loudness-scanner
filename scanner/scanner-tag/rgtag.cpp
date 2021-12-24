/* See COPYING file for copyright and license details. */

#include "rgtag.h"

#include <apetag.h>
#include <id3v2tag.h>
#include <relativevolumeframe.h>
#include <taglib.h>
#include <textidentificationframe.h>
#include <xiphcomment.h>

#include <flacfile.h>
#include <mp4file.h>
#include <mpcfile.h>
#include <mpegfile.h>
#include <oggfile.h>
#include <opusfile.h>
#include <vorbisfile.h>
#include <wavpackfile.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <ios>
#include <sstream>

#ifdef _WIN32
#define CAST_FILENAME (wchar_t const *)
#else
#define CAST_FILENAME
#endif

static float
parse_string_to_float(std::string const &s, bool string_dummy = false)
{
	std::stringstream ss;
	ss << s;
	if (string_dummy) {
		std::string dummy;
		ss >> dummy;
	}
	float f;
	ss >> f;
	return f;
}

static bool
clear_txxx_tag(TagLib::ID3v2::Tag *tag, TagLib::String const &tag_name,
    float *old_content = nullptr)
{
	TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
	for (auto *it : l) {
		auto *fr = dynamic_cast</**/
		    TagLib::ID3v2::UserTextIdentificationFrame *>(it);
		if (fr && fr->description().upper() == tag_name) {
			if (old_content) {
				*old_content = parse_string_to_float(
				    fr->fieldList().toString().to8Bit(), true);
			}

			tag->removeFrame(fr);
			return true;
		}
	}
	return false;
}

static bool
clear_rva2_tag(TagLib::ID3v2::Tag *tag, TagLib::String const &tag_name)
{
	TagLib::ID3v2::FrameList l = tag->frameList("RVA2");
	for (auto *it : l) {
		auto *fr = /**/
		    dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(it);
		if (fr && fr->identification().upper() == tag_name) {
			tag->removeFrame(fr);
			return true;
		}
	}
	return false;
}

static void
set_txxx_tag(TagLib::ID3v2::Tag *tag, std::string const &tag_name,
    std::string const &value)
{
	TagLib::ID3v2::UserTextIdentificationFrame *txxx =
	    TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tag_name);
	if (!txxx) {
		txxx = new TagLib::ID3v2::UserTextIdentificationFrame;
		txxx->setDescription(tag_name);
		tag->addFrame(txxx);
	}
	txxx->setText(value);
}

static void
set_rva2_tag(TagLib::ID3v2::Tag *tag, std::string const &tag_name, double gain,
    double peak)
{
	TagLib::ID3v2::RelativeVolumeFrame *rva2 = nullptr;
	TagLib::ID3v2::FrameList rva2_frame_list = tag->frameList("RVA2");
	for (auto *it : rva2_frame_list) {
		auto *fr = /**/
		    dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame *>(it);
		if (fr->identification() == tag_name) {
			rva2 = fr;
			break;
		}
	}
	if (!rva2) {
		rva2 = new TagLib::ID3v2::RelativeVolumeFrame;
		rva2->setIdentification(tag_name);
		tag->addFrame(rva2);
	}
	rva2->setVolumeAdjustment(float(gain),
	    TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);

	TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peak_volume;
	peak_volume.bitsRepresentingPeak = 16;
	double amp_peak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
	auto amp_peak_int = static_cast<unsigned int>(std::ceil(amp_peak));
	TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(amp_peak_int);
	peak_volume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
	rva2->setPeakVolume(peak_volume);
}

struct gain_data_strings {
	gain_data_strings(struct gain_data *gd)
	{
		std::stringstream ss;
		ss.precision(2);
		ss << std::fixed;
		ss << gd->album_gain << " dB";
		album_gain = ss.str();
		ss.str(std::string());
		ss.clear();
		ss << gd->track_gain << " dB";
		track_gain = ss.str();
		ss.str(std::string());
		ss.clear();
		ss.precision(6);
		ss << gd->album_peak;
		ss >> album_peak;
		ss.str(std::string());
		ss.clear();
		ss << gd->track_peak;
		ss >> track_peak;
		ss.str(std::string());
		ss.clear();
	}

	std::string track_gain;
	std::string track_peak;
	std::string album_gain;
	std::string album_peak;
};

static int
tag_id3v2(char const *filename, struct gain_data *gd,
    struct gain_data_strings *gds)
{
	TagLib::MPEG::File f(CAST_FILENAME filename);
	TagLib::ID3v2::Tag *id3v2tag = f.ID3v2Tag(true);
	TagLib::uint version = id3v2tag->header()->majorVersion();
	if (version > 4) {
		return 1;
	}

	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_album_gain").upper())) {
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_album_peak").upper())) {
	}
	while (clear_rva2_tag(id3v2tag, TagLib::String("album").upper())) {
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_track_gain").upper())) {
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_track_peak").upper())) {
	}
	while (clear_rva2_tag(id3v2tag, TagLib::String("track").upper())) {
	}
	set_txxx_tag(id3v2tag, "replaygain_track_gain", gds->track_gain);
	set_txxx_tag(id3v2tag, "replaygain_track_peak", gds->track_peak);
	if (version == 4) {
		set_rva2_tag(id3v2tag, "track", gd->track_gain, gd->track_peak);
	}
	if (gd->album_mode) {
		set_txxx_tag(id3v2tag, "replaygain_album_gain",
		    gds->album_gain);
		set_txxx_tag(id3v2tag, "replaygain_album_peak",
		    gds->album_peak);
		if (version == 4) {
			set_rva2_tag(id3v2tag, "album", gd->album_gain,
			    gd->album_peak);
		}
	}
	return (int)!f.save(TagLib::MPEG::File::ID3v2,
	    TagLib::File::StripTags::StripNone,
	    version <= 3 ? TagLib::ID3v2::Version::v3 :
				 TagLib::ID3v2::Version::v4);
}

static bool
has_tag_id3v2(char const *filename)
{
	TagLib::MPEG::File f(CAST_FILENAME filename);
	TagLib::ID3v2::Tag *id3v2tag = f.ID3v2Tag(true);

	bool has_tag = false;
	float old_tag_value = 0;

	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_album_gain").upper())) {
		has_tag = true;
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_album_peak").upper(), &old_tag_value)) {
		if (old_tag_value == 0) {
			return false;
		}
		has_tag = true;
	}
	while (clear_rva2_tag(id3v2tag, TagLib::String("album").upper())) {
		has_tag = true;
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_track_gain").upper())) {
		has_tag = true;
	}
	while (clear_txxx_tag(id3v2tag,
	    TagLib::String("replaygain_track_peak").upper(), &old_tag_value)) {
		if (old_tag_value == 0) {
			return false;
		}
		has_tag = true;
	}
	while (clear_rva2_tag(id3v2tag, TagLib::String("track").upper())) {
		has_tag = true;
	}

	return has_tag;
}

static std::pair<TagLib::File *, TagLib::Ogg::XiphComment *>
get_ogg_file(char const *filename, char const *extension)
{
	TagLib::File *file = nullptr;
	TagLib::Ogg::XiphComment *xiph = nullptr;
	if (!std::strcmp(extension, "flac")) {
		auto *f = new TagLib::FLAC::File(CAST_FILENAME filename);
		xiph = f->xiphComment(true);
		file = f;
	} else if (!std::strcmp(extension, "ogg") ||
	    !std::strcmp(extension, "oga")) {
		auto *f = new TagLib::Ogg::Vorbis::File(CAST_FILENAME filename);
		xiph = f->tag();
		file = f;
	} else if (!std::strcmp(extension, "opus")) {
		auto *f = new TagLib::Ogg::Opus::File(CAST_FILENAME filename);
		xiph = f->tag();
		file = f;
	}
	return std::make_pair(file, xiph);
}

static int16_t
to_opus_gain(double gain)
{
	gain = 256 * gain + 0.5;

	if (gain < INT16_MIN) {
		return INT16_MIN;
	}

	if (gain >= INT16_MAX) {
		return INT16_MAX;
	}

	return static_cast<int16_t>(std::floor(gain));
}

static void
adjust_gain_data(struct gain_data *gd, double opus_correction_db)
{
	gd->album_gain += opus_correction_db;
	gd->track_gain += opus_correction_db;
	if (gd->album_peak > 0.0) {
		gd->album_peak = std::pow(10.0,
		    ((20 * log10(gd->album_peak)) - opus_correction_db) / 20.0);
	}
	if (gd->track_peak > 0.0) {
		gd->track_peak = std::pow(10.0,
		    ((20 * log10(gd->track_peak)) - opus_correction_db) / 20.0);
	}
}

double
clamp_rg(double x)
{
	if (x < -51.0) {
		return -51.0;
	}

	if (x > 51.0) {
		return 51.0;
	}

	return x;
}

void
clamp_gain_data(struct gain_data *gd)
{
	gd->album_gain = clamp_rg(gd->album_gain);
	gd->track_gain = clamp_rg(gd->track_gain);
}

static int
tag_vorbis_comment(char const *filename, char const *extension,
    struct gain_data *gd, struct gain_data_strings *gds,
    OpusTagInfo const *opus_tag_info)
{
	std::pair<TagLib::File *, TagLib::Ogg::XiphComment *> p =
	    get_ogg_file(filename, extension);

	bool is_opus = !std::strcmp(extension, "opus");
	struct gain_data gd_opus;
	struct gain_data_strings gds_opus(&gd_opus);

	if (is_opus) {
		double opus_header_gain;

		if (opus_tag_info->opus_gain_reference ==
		    OPUS_GAIN_REFERENCE_ABSOLUTE) {
			opus_header_gain = opus_tag_info->offset;
		} else {
			opus_header_gain = /**/
			    (!opus_tag_info->is_track && gd->album_mode) ?
				  gd->album_gain :
				  gd->track_gain;
			opus_header_gain -= 5.0;
			opus_header_gain += opus_tag_info->offset;
		}

		int16_t opus_header_gain_int = to_opus_gain(opus_header_gain);

		{
			auto *opus_file = /**/
			    static_cast<TagLib::Ogg::Opus::File *>(p.first);
			TagLib::ByteVector header = opus_file->packet(0);
			header[16] = static_cast<char>(
			    static_cast<uint16_t>(opus_header_gain_int) & 0xff);
			header[17] = static_cast<char>(
			    static_cast<uint16_t>(opus_header_gain_int) >> 8);
			opus_file->setPacket(0, header);
		}

		gd_opus = *gd;
		adjust_gain_data(&gd_opus, -opus_header_gain_int / 256.0);

		int16_t opus_r128_album_gain_int = /**/
		    to_opus_gain(gd_opus.album_gain - 5.0);
		int16_t opus_r128_track_gain_int = /**/
		    to_opus_gain(gd_opus.track_gain - 5.0);

		p.second->addField("R128_TRACK_GAIN",
		    std::to_string(opus_r128_track_gain_int));
		if (gd->album_mode) {
			p.second->addField("R128_ALBUM_GAIN",
			    std::to_string(opus_r128_album_gain_int));
		} else {
			p.second->removeFields("R128_ALBUM_GAIN");
		}

		clamp_gain_data(&gd_opus);
		gds_opus = gain_data_strings(&gd_opus);

		gd = &gd_opus;
		gds = &gds_opus;
	}

	if (is_opus && !opus_tag_info->vorbisgain_compat) {
		p.second->removeFields("REPLAYGAIN_TRACK_GAIN");
		p.second->removeFields("REPLAYGAIN_TRACK_PEAK");
		p.second->removeFields("REPLAYGAIN_ALBUM_GAIN");
		p.second->removeFields("REPLAYGAIN_ALBUM_PEAK");
	} else {
		p.second->addField("REPLAYGAIN_TRACK_GAIN", gds->track_gain);
		p.second->addField("REPLAYGAIN_TRACK_PEAK", gds->track_peak);
		if (gd->album_mode) {
			p.second->addField("REPLAYGAIN_ALBUM_GAIN",
			    gds->album_gain);
			p.second->addField("REPLAYGAIN_ALBUM_PEAK",
			    gds->album_peak);
		} else {
			p.second->removeFields("REPLAYGAIN_ALBUM_GAIN");
			p.second->removeFields("REPLAYGAIN_ALBUM_PEAK");
		}
	}

	bool success = p.first->save();
	delete p.first;
	return (int)!success;
}

static bool
has_vorbis_comment(char const *filename, char const *extension,
    bool opus_compat)
{
	std::pair<TagLib::File *, TagLib::Ogg::XiphComment *> p =
	    get_ogg_file(filename, extension);

	bool has_tag;
	TagLib::uint fieldCount = p.second->fieldCount();

	bool is_opus = !std::strcmp(extension, "opus");

	TagLib::Ogg::FieldListMap const &flm = p.second->fieldListMap();
	if (flm.contains("REPLAYGAIN_ALBUM_PEAK")) {
		TagLib::StringList const &sl = flm["REPLAYGAIN_ALBUM_PEAK"];
		for (auto const &i : sl) {
			if (parse_string_to_float(i.to8Bit()) == 0) {
				has_tag = false;
				goto end;
			}
		}
	}
	if (flm.contains("REPLAYGAIN_TRACK_PEAK")) {
		TagLib::StringList const &sl = flm["REPLAYGAIN_TRACK_PEAK"];
		for (auto const &i : sl) {
			if (parse_string_to_float(i.to8Bit()) == 0) {
				has_tag = false;
				goto end;
			}
		}
	}

	p.second->removeFields("REPLAYGAIN_ALBUM_GAIN");
	p.second->removeFields("REPLAYGAIN_ALBUM_PEAK");
	p.second->removeFields("REPLAYGAIN_TRACK_GAIN");
	p.second->removeFields("REPLAYGAIN_TRACK_PEAK");

	if (is_opus) {
		if (p.second->fieldCount() < fieldCount) {
			if (!opus_compat) {
				/* If we see any of those tags above in
				 * 'non-legacy' opus mode, we need to remove
				 * them. Just rescan the file for now. */
				has_tag = false;
				goto end;
			} else {
				/* If we are in legacy opus mode, we need _both_
				 * R128_* and REPLAYGAIN_* tags. So reset
				 * fieldCount so that the logic below works. */
				fieldCount = p.second->fieldCount();
			}
		} else if (opus_compat) {
			/* We need those tags in 'legacy' opus mode. Force a
			 * rescan. */
			has_tag = false;
			goto end;
		}

		p.second->removeFields("R128_ALBUM_GAIN");
		p.second->removeFields("R128_TRACK_GAIN");
	}

	has_tag = p.second->fieldCount() < fieldCount;
end:
	delete p.first;
	return has_tag;
}

static std::pair<TagLib::File *, TagLib::APE::Tag *>
get_ape_file(char const *filename, char const *extension)
{
	TagLib::File *file = nullptr;
	TagLib::APE::Tag *ape = nullptr;
	if (!std::strcmp(extension, "mpc")) {
		auto *f = new TagLib::MPC::File(CAST_FILENAME filename);
		ape = f->APETag(true);
		file = f;
	} else if (!std::strcmp(extension, "wv")) {
		auto *f = new TagLib::WavPack::File(CAST_FILENAME filename);
		ape = f->APETag(true);
		file = f;
	}
	return std::make_pair(file, ape);
}

static int
tag_ape(char const *filename, char const *extension, struct gain_data *gd,
    struct gain_data_strings *gds)
{
	std::pair<TagLib::File *, TagLib::APE::Tag *> p = get_ape_file(filename,
	    extension);
	p.second->addValue("replaygain_track_gain", gds->track_gain);
	p.second->addValue("replaygain_track_peak", gds->track_peak);
	if (gd->album_mode) {
		p.second->addValue("replaygain_album_gain", gds->album_gain);
		p.second->addValue("replaygain_album_peak", gds->album_peak);
	} else {
		p.second->removeItem("replaygain_album_gain");
		p.second->removeItem("replaygain_album_peak");
	}
	bool success = p.first->save();
	delete p.first;
	return (int)!success;
}

static bool
has_tag_ape(char const *filename, char const *extension)
{
	std::pair<TagLib::File *, TagLib::APE::Tag *> p = get_ape_file(filename,
	    extension);

	TagLib::uint fieldCount = p.second->itemListMap().size();

	p.second->removeItem("replaygain_album_gain");
	p.second->removeItem("replaygain_album_peak");
	p.second->removeItem("replaygain_track_gain");
	p.second->removeItem("replaygain_track_peak");

	bool has_tag = p.second->itemListMap().size() < fieldCount;
	delete p.first;
	return has_tag;
}

static int
tag_mp4(char const *filename, struct gain_data *gd,
    struct gain_data_strings *gds)
{
	TagLib::MP4::File f(CAST_FILENAME filename);
	TagLib::MP4::Tag *t = f.tag();
	if (!t) {
		return 1;
	}
	t->setItem("----:com.apple.iTunes:replaygain_track_gain",
	    TagLib::StringList(gds->track_gain));
	t->setItem("----:com.apple.iTunes:replaygain_track_peak",
	    TagLib::StringList(gds->track_peak));
	if (gd->album_mode) {
		t->setItem("----:com.apple.iTunes:replaygain_album_gain",
		    TagLib::StringList(gds->album_gain));
		t->setItem("----:com.apple.iTunes:replaygain_album_peak",
		    TagLib::StringList(gds->album_peak));
	} else {
		t->removeItem("----:com.apple.iTunes:replaygain_album_gain");
		t->removeItem("----:com.apple.iTunes:replaygain_album_peak");
	}
	return (int)!f.save();
}

static bool
has_tag_mp4(char const *filename)
{
	TagLib::MP4::File f(CAST_FILENAME filename);
	TagLib::MP4::Tag *t = f.tag();
	if (!t) {
		std::cerr << "Error reading mp4 tag" << std::endl;
		return false;
	}
	TagLib::MP4::ItemMap const &ilm = t->itemMap();

	TagLib::uint fieldCount = ilm.size();

	if (ilm.contains("----:com.apple.iTunes:replaygain_album_peak")) {
		TagLib::StringList const &sl =
		    ilm["----:com.apple.iTunes:replaygain_album_peak"]
			.toStringList();
		for (auto const &i : sl) {
			if (parse_string_to_float(i.to8Bit()) == 0) {
				return false;
			}
		}
	}
	if (ilm.contains("----:com.apple.iTunes:replaygain_track_peak")) {
		TagLib::StringList const &sl =
		    ilm["----:com.apple.iTunes:replaygain_track_peak"]
			.toStringList();
		for (auto const &i : sl) {
			if (parse_string_to_float(i.to8Bit()) == 0) {
				return false;
			}
		}
	}

	t->removeItem("----:com.apple.iTunes:replaygain_album_gain");
	t->removeItem("----:com.apple.iTunes:replaygain_album_peak");
	t->removeItem("----:com.apple.iTunes:replaygain_track_gain");
	t->removeItem("----:com.apple.iTunes:replaygain_track_peak");

	bool has_tag = t->itemMap().size() < fieldCount;
	return has_tag;
}

int
set_rg_info(char const *filename, char const *extension, struct gain_data *gd,
    OpusTagInfo const *opus_tag_info)
{
	if (std::strcmp(extension, "opus") != 0) {
		/* For opus, we clamp in tag_vorbis_comment(). */
		clamp_gain_data(gd);
	}

	struct gain_data_strings gds(gd);

	if (!std::strcmp(extension, "mp3") || !std::strcmp(extension, "mp2")) {
		return tag_id3v2(filename, gd, &gds);
	}

	if (!std::strcmp(extension, "flac") ||
	    !std::strcmp(extension, "opus") || /**/
	    !std::strcmp(extension, "ogg") ||  /**/
	    !std::strcmp(extension, "oga")) {
		return tag_vorbis_comment(filename, extension, gd, &gds,
		    opus_tag_info);
	}

	if (!std::strcmp(extension, "mpc") || !std::strcmp(extension, "wv")) {
		return tag_ape(filename, extension, gd, &gds);
	}

	if (!std::strcmp(extension, "mp4") || !std::strcmp(extension, "m4a")) {
		return tag_mp4(filename, gd, &gds);
	}

	return 1;
}

bool
has_rg_info(char const *filename, char const *extension,
    OpusTagInfo const *opus_tag_info)
{
	if (!std::strcmp(extension, "mp3") || !std::strcmp(extension, "mp2")) {
		return has_tag_id3v2(filename);
	}

	if (!std::strcmp(extension, "flac") ||
	    !std::strcmp(extension, "opus") || /**/
	    !std::strcmp(extension, "ogg") ||  /**/
	    !std::strcmp(extension, "oga")) {
		return has_vorbis_comment(filename, extension,
		    opus_tag_info->vorbisgain_compat);
	}

	// TODO: implement "0.0 workaround" for ape
	if (!std::strcmp(extension, "mpc") || !std::strcmp(extension, "wv")) {
		return has_tag_ape(filename, extension);
	}

	if (!std::strcmp(extension, "mp4") || !std::strcmp(extension, "m4a")) {
		return has_tag_mp4(filename);
	}

	return false;
}
