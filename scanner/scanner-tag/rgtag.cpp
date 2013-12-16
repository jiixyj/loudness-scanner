/* See COPYING file for copyright and license details. */

#include "rgtag.h"

#include <taglib.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <relativevolumeframe.h>
#include <xiphcomment.h>
#include <apetag.h>

#if TAGLIB_MINOR_VERSION >= 9
#define HAS_OPUS_SUPPORT
#endif

#include <mpegfile.h>
#include <flacfile.h>
#include <oggfile.h>
#ifdef HAS_OPUS_SUPPORT
#include <opusfile.h>
#endif
#include <vorbisfile.h>
#include <mpcfile.h>
#include <wavpackfile.h>
#include <mp4file.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <ios>

#ifdef _WIN32
#define CAST_FILENAME (const wchar_t *)
#else
#define CAST_FILENAME
#endif

float parse_string_to_float(std::string const& s, bool string_dummy = false) {
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

static bool clear_txxx_tag(TagLib::ID3v2::Tag* tag, TagLib::String tag_name,
    float* old_content = NULL) {
  TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
  for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    TagLib::ID3v2::UserTextIdentificationFrame* fr =
                dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);
    if (fr && fr->description().upper() == tag_name) {
      if (old_content) {
        *old_content =
            parse_string_to_float(fr->fieldList().toString().to8Bit(), true);
      }

      tag->removeFrame(fr);
      return true;
    }
  }
  return false;
}

static bool clear_rva2_tag(TagLib::ID3v2::Tag* tag, TagLib::String tag_name) {
  TagLib::ID3v2::FrameList l = tag->frameList("RVA2");
  for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    TagLib::ID3v2::RelativeVolumeFrame* fr =
                         dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
    if (fr && fr->identification().upper() == tag_name) {
      tag->removeFrame(fr);
      return true;
    }
  }
  return false;
}

static void set_txxx_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, std::string value) {
  TagLib::ID3v2::UserTextIdentificationFrame* txxx = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tag_name);
  if (!txxx) {
    txxx = new TagLib::ID3v2::UserTextIdentificationFrame;
    txxx->setDescription(tag_name);
    tag->addFrame(txxx);
  }
  txxx->setText(value);
}

static void set_rva2_tag(TagLib::ID3v2::Tag* tag, std::string tag_name, double gain, double peak) {
  TagLib::ID3v2::RelativeVolumeFrame* rva2 = NULL;
  TagLib::ID3v2::FrameList rva2_frame_list = tag->frameList("RVA2");
  TagLib::ID3v2::FrameList::ConstIterator it = rva2_frame_list.begin();
  for (; it != rva2_frame_list.end(); ++it) {
    TagLib::ID3v2::RelativeVolumeFrame* fr =
                         dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
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
  rva2->setChannelType(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
  rva2->setVolumeAdjustment(float(gain));

  TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peak_volume;
  peak_volume.bitsRepresentingPeak = 16;
  double amp_peak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
  unsigned int amp_peak_int = static_cast<unsigned int>(std::ceil(amp_peak));
  TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(amp_peak_int);
  peak_volume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
  rva2->setPeakVolume(peak_volume);
}

struct gain_data_strings {
  gain_data_strings(struct gain_data* gd) {
    std::stringstream ss;
    ss.precision(2);
    ss << std::fixed;
    ss << gd->album_gain << " dB"; album_gain = ss.str(); ss.str(std::string()); ss.clear();
    ss << gd->track_gain << " dB"; track_gain = ss.str(); ss.str(std::string()); ss.clear();
    ss.precision(6);
    ss << gd->album_peak; ss >> album_peak; ss.str(std::string()); ss.clear();
    ss << gd->track_peak; ss >> track_peak; ss.str(std::string()); ss.clear();
  }
  std::string track_gain, track_peak, album_gain, album_peak;
};

static bool tag_id3v2(const char* filename,
                      struct gain_data* gd,
                      struct gain_data_strings* gds) {
  TagLib::MPEG::File f(CAST_FILENAME filename);
  TagLib::ID3v2::Tag* id3v2tag = f.ID3v2Tag(true);
  TagLib::uint version = id3v2tag->header()->majorVersion();

  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_gain").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_peak").upper()));
  while (clear_rva2_tag(id3v2tag, TagLib::String("album").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_gain").upper()));
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_peak").upper()));
  while (clear_rva2_tag(id3v2tag, TagLib::String("track").upper()));
  set_txxx_tag(id3v2tag, "replaygain_track_gain", gds->track_gain);
  set_txxx_tag(id3v2tag, "replaygain_track_peak", gds->track_peak);
  if (version == 4)
    set_rva2_tag(id3v2tag, "track", gd->track_gain, gd->track_peak);
  if (gd->album_mode) {
    set_txxx_tag(id3v2tag, "replaygain_album_gain", gds->album_gain);
    set_txxx_tag(id3v2tag, "replaygain_album_peak", gds->album_peak);
    if (version == 4)
      set_rva2_tag(id3v2tag, "album", gd->album_gain, gd->album_peak);
  }
#if TAGLIB_MAJOR_VERSION > 1 || TAGLIB_MINOR_VERSION > 7
  return !f.save(TagLib::MPEG::File::ID3v2, false, version);
#else
  return !f.save(TagLib::MPEG::File::ID3v2, false);
#endif
}

static bool has_tag_id3v2(const char* filename) {
  TagLib::MPEG::File f(CAST_FILENAME filename);
  TagLib::ID3v2::Tag* id3v2tag = f.ID3v2Tag(true);

  bool has_tag = false;
  float old_tag_value = 0;

  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_gain").upper()))
    has_tag = true;
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_album_peak").upper(), &old_tag_value)) {
    if (old_tag_value == 0)
      return false;
    has_tag = true;
  }
  while (clear_rva2_tag(id3v2tag, TagLib::String("album").upper()))
    has_tag = true;
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_gain").upper()))
    has_tag = true;
  while (clear_txxx_tag(id3v2tag, TagLib::String("replaygain_track_peak").upper(), &old_tag_value)) {
    if (old_tag_value == 0)
      return false;
    has_tag = true;
  }
  while (clear_rva2_tag(id3v2tag, TagLib::String("track").upper()))
    has_tag = true;

  return has_tag;
}

static std::pair<TagLib::File*, TagLib::Ogg::XiphComment*>
get_ogg_file(const char* filename, const char* extension) {
  TagLib::File* file = NULL;
  TagLib::Ogg::XiphComment* xiph = NULL;
  if (!std::strcmp(extension, "flac")) {
    TagLib::FLAC::File* f = new TagLib::FLAC::File(CAST_FILENAME filename);
    xiph = f->xiphComment(true);
    file = f;
  } else if (!std::strcmp(extension, "ogg") ||
             !std::strcmp(extension, "oga")) {
    TagLib::Ogg::Vorbis::File* f =
          new TagLib::Ogg::Vorbis::File(CAST_FILENAME filename);
    xiph = f->tag();
    file = f;
#ifdef HAS_OPUS_SUPPORT
  } else if (!std::strcmp(extension, "opus")) {
    TagLib::Ogg::Opus::File* f =
          new TagLib::Ogg::Opus::File(CAST_FILENAME filename);
    xiph = f->tag();
    file = f;
#endif
  }
  return std::make_pair(file, xiph);
}

static bool tag_vorbis_comment(const char* filename,
                               const char* extension,
                               struct gain_data* gd,
                               struct gain_data_strings* gds) {
  std::pair<TagLib::File*, TagLib::Ogg::XiphComment*> p
      = get_ogg_file(filename, extension);

  p.second->addField("REPLAYGAIN_TRACK_GAIN", gds->track_gain);
  p.second->addField("REPLAYGAIN_TRACK_PEAK", gds->track_peak);
  if (gd->album_mode) {
    p.second->addField("REPLAYGAIN_ALBUM_GAIN", gds->album_gain);
    p.second->addField("REPLAYGAIN_ALBUM_PEAK", gds->album_peak);
  } else {
    p.second->removeField("REPLAYGAIN_ALBUM_GAIN");
    p.second->removeField("REPLAYGAIN_ALBUM_PEAK");
  }
  bool success = p.first->save();
  delete p.first;
  return !success;
}

static bool has_vorbis_comment(const char* filename,
                               const char* extension) {
  std::pair<TagLib::File*, TagLib::Ogg::XiphComment*> p
      = get_ogg_file(filename, extension);

  bool has_tag;
  TagLib::uint fieldCount = p.second->fieldCount();

  TagLib::Ogg::FieldListMap const& flm = p.second->fieldListMap();
  if (flm.contains("REPLAYGAIN_ALBUM_PEAK")) {
    TagLib::StringList const& sl = flm["REPLAYGAIN_ALBUM_PEAK"];
    for (TagLib::StringList::ConstIterator i = sl.begin(); i != sl.end(); ++i) {
      if (parse_string_to_float(i->to8Bit()) == 0) {
        has_tag = false;
        goto end;
      }
    }
  }
  if (flm.contains("REPLAYGAIN_TRACK_PEAK")) {
    TagLib::StringList const& sl = flm["REPLAYGAIN_TRACK_PEAK"];
    for (TagLib::StringList::ConstIterator i = sl.begin(); i != sl.end(); ++i) {
      if (parse_string_to_float(i->to8Bit()) == 0) {
        has_tag = false;
        goto end;
      }
    }
  }

  p.second->removeField("REPLAYGAIN_ALBUM_GAIN");
  p.second->removeField("REPLAYGAIN_ALBUM_PEAK");
  p.second->removeField("REPLAYGAIN_TRACK_GAIN");
  p.second->removeField("REPLAYGAIN_TRACK_PEAK");

  has_tag = p.second->fieldCount() < fieldCount;
end:
  delete p.first;
  return has_tag;
}

static std::pair<TagLib::File*, TagLib::APE::Tag*>
get_ape_file(const char* filename, const char* extension) {
  TagLib::File* file = NULL;
  TagLib::APE::Tag* ape = NULL;
  if (!std::strcmp(extension, "mpc")) {
    TagLib::MPC::File* f = new TagLib::MPC::File(CAST_FILENAME filename);
    ape = f->APETag(true);
    file = f;
  } else if (!std::strcmp(extension, "wv")) {
    TagLib::WavPack::File* f = new TagLib::WavPack::File(CAST_FILENAME filename);
    ape = f->APETag(true);
    file = f;
  }
  return std::make_pair(file, ape);
}

static bool tag_ape(const char* filename,
                    const char* extension,
                    struct gain_data* gd,
                    struct gain_data_strings* gds) {
  std::pair<TagLib::File*, TagLib::APE::Tag*> p
      = get_ape_file(filename, extension);
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
  return !success;
}

static bool has_tag_ape(const char* filename,
                        const char* extension) {
  std::pair<TagLib::File*, TagLib::APE::Tag*> p
      = get_ape_file(filename, extension);

  TagLib::uint fieldCount = p.second->itemListMap().size();

  p.second->removeItem("replaygain_album_gain");
  p.second->removeItem("replaygain_album_peak");
  p.second->removeItem("replaygain_track_gain");
  p.second->removeItem("replaygain_track_peak");

  bool has_tag = p.second->itemListMap().size() < fieldCount;
  delete p.first;
  return has_tag;
}

static bool tag_mp4(const char* filename,
                    struct gain_data* gd,
                    struct gain_data_strings* gds) {
  TagLib::MP4::File f(CAST_FILENAME filename);
  TagLib::MP4::Tag* t = f.tag();
  if (!t) {
    return 1;
  }
  TagLib::MP4::ItemListMap& ilm = t->itemListMap();
  ilm["----:com.apple.iTunes:replaygain_track_gain"] = TagLib::MP4::Item(TagLib::StringList(gds->track_gain));
  ilm["----:com.apple.iTunes:replaygain_track_peak"] = TagLib::MP4::Item(TagLib::StringList(gds->track_peak));
  if (gd->album_mode) {
    ilm["----:com.apple.iTunes:replaygain_album_gain"] = TagLib::MP4::Item(TagLib::StringList(gds->album_gain));
    ilm["----:com.apple.iTunes:replaygain_album_peak"] = TagLib::MP4::Item(TagLib::StringList(gds->album_peak));
  } else {
    ilm.erase("----:com.apple.iTunes:replaygain_album_gain");
    ilm.erase("----:com.apple.iTunes:replaygain_album_peak");
  }
  return !f.save();
}

static bool has_tag_mp4(const char* filename) {
  TagLib::MP4::File f(CAST_FILENAME filename);
  TagLib::MP4::Tag* t = f.tag();
  if (!t) {
    std::cerr << "Error reading mp4 tag" << std::endl;
    return false;
  }
  TagLib::MP4::ItemListMap& ilm = t->itemListMap();

  TagLib::uint fieldCount = ilm.size();

  if (ilm.contains("----:com.apple.iTunes:replaygain_album_peak")) {
    TagLib::StringList const& sl =
        ilm["----:com.apple.iTunes:replaygain_album_peak"].toStringList();
    for (TagLib::StringList::ConstIterator i = sl.begin(); i != sl.end(); ++i) {
      if (parse_string_to_float(i->to8Bit()) == 0) {
        return false;
      }
    }
  }
  if (ilm.contains("----:com.apple.iTunes:replaygain_track_peak")) {
    TagLib::StringList const& sl =
        ilm["----:com.apple.iTunes:replaygain_track_peak"].toStringList();
    for (TagLib::StringList::ConstIterator i = sl.begin(); i != sl.end(); ++i) {
      if (parse_string_to_float(i->to8Bit()) == 0) {
        return false;
      }
    }
  }

  ilm.erase("----:com.apple.iTunes:replaygain_album_gain");
  ilm.erase("----:com.apple.iTunes:replaygain_album_peak");
  ilm.erase("----:com.apple.iTunes:replaygain_track_gain");
  ilm.erase("----:com.apple.iTunes:replaygain_track_peak");

  bool has_tag = ilm.size() < fieldCount;
  return has_tag;
}

int set_rg_info(const char* filename,
                const char* extension,
                struct gain_data* gd) {
  struct gain_data_strings gds(gd);

  if (!std::strcmp(extension, "mp3") ||
      !std::strcmp(extension, "mp2")) {
    return tag_id3v2(filename, gd, &gds);
  } else if (!std::strcmp(extension, "flac") ||
#ifdef HAS_OPUS_SUPPORT
             !std::strcmp(extension, "opus") ||
#endif
             !std::strcmp(extension, "ogg") ||
             !std::strcmp(extension, "oga")) {
    return tag_vorbis_comment(filename, extension, gd, &gds);
  } else if (!std::strcmp(extension, "mpc") ||
             !std::strcmp(extension, "wv")) {
    return tag_ape(filename, extension, gd, &gds);
  } else if (!std::strcmp(extension, "mp4") ||
             !std::strcmp(extension, "m4a")) {
    return tag_mp4(filename, gd, &gds);
  }
  return 1;
}

int has_rg_info(const char* filename, const char* extension) {
  if (!std::strcmp(extension, "mp3") ||
      !std::strcmp(extension, "mp2")) {
    return has_tag_id3v2(filename);
  } else if (!std::strcmp(extension, "flac") ||
#ifdef HAS_OPUS_SUPPORT
             !std::strcmp(extension, "opus") ||
#endif
             !std::strcmp(extension, "ogg") ||
             !std::strcmp(extension, "oga")) {
    return has_vorbis_comment(filename, extension);
  // TODO: implement "0.0 workaround" for ape
  } else if (!std::strcmp(extension, "mpc") ||
             !std::strcmp(extension, "wv")) {
    return has_tag_ape(filename, extension);
  } else if (!std::strcmp(extension, "mp4") ||
             !std::strcmp(extension, "m4a")) {
    return has_tag_mp4(filename);
  }
  return 0;
}
