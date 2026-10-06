// ColorMeta.cpp — color metadata extraction + naming + explicit conversion gate.
#include "ColorMeta.hpp"

#include "JsonWriter.hpp"

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace anvil {

ColorMeta ColorMeta::fromFrame(const AVFrame* f) {
    ColorMeta m;
    if (!f) return m;
    m.range = f->color_range;
    m.primaries = f->color_primaries;
    m.transfer = f->color_trc;
    m.matrix = f->colorspace;
    m.chromaLocation = f->chroma_location;
    m.pixelFormat = f->format;
    if (const AVPixFmtDescriptor* d=av_pix_fmt_desc_get(static_cast<AVPixelFormat>(f->format));
        d && d->comp[0].depth>0) m.bitDepth=d->comp[0].depth;
    for (int sd = 0; sd < 2; ++sd) {
        // HDR side data presence is recorded; values are container truth.
        if (av_frame_get_side_data(const_cast<AVFrame*>(f),
                                   sd == 0 ? AV_FRAME_DATA_MASTERING_DISPLAY_METADATA
                                           : AV_FRAME_DATA_CONTENT_LIGHT_LEVEL)) {
            if (sd == 0) m.hasMasteringDisplay = true;
            else m.hasContentLightLevel = true;
        }
    }
    return m;
}

ColorMeta ColorMeta::unspecified() {
    return {};
}

bool ColorMeta::transferKnown() const {
    return transfer != AVCOL_TRC_UNSPECIFIED && transfer != AVCOL_TRC_RESERVED
        && transfer != AVCOL_TRC_RESERVED0;
}

bool ColorMeta::matrixKnown() const {
    return matrix != AVCOL_SPC_UNSPECIFIED && matrix != AVCOL_SPC_RESERVED;
}

bool ColorMeta::rangeKnown() const {
    return range == AVCOL_RANGE_MPEG || range == AVCOL_RANGE_JPEG;
}

bool ColorMeta::isHdrTransfer() const {
    return transfer == AVCOL_TRC_SMPTE2084 || transfer == AVCOL_TRC_ARIB_STD_B67;
}

bool ColorMeta::conversionFullySpecified() const {
    return transferKnown() && matrixKnown() && rangeKnown()
        && primaries != AVCOL_PRI_UNSPECIFIED && pixelFormat != AV_PIX_FMT_NONE;
}

std::string ColorMeta::rangeName(int range) {
    switch (range) {
        case AVCOL_RANGE_MPEG: return "limited";
        case AVCOL_RANGE_JPEG: return "full";
        case AVCOL_RANGE_UNSPECIFIED: return "unspecified";
        default: return "reserved";
    }
}

std::string ColorMeta::primariesName(int primaries) {
    switch (primaries) {
        case AVCOL_PRI_BT709: return "bt709";
        case AVCOL_PRI_BT470M: return "bt470m";
        case AVCOL_PRI_BT470BG: return "bt470bg";
        case AVCOL_PRI_SMPTE170M: return "smpte170m";
        case AVCOL_PRI_SMPTE240M: return "smpte240m";
        case AVCOL_PRI_FILM: return "film";
        case AVCOL_PRI_BT2020: return "bt2020";
        case AVCOL_PRI_SMPTE428: return "smpte428";
        case AVCOL_PRI_SMPTE431: return "smpte431";
        case AVCOL_PRI_SMPTE432: return "smpte432";
        case AVCOL_PRI_JEDEC_P22: return "jedec_p22";
        case AVCOL_PRI_UNSPECIFIED: return "unspecified";
        default: return "reserved";
    }
}

std::string ColorMeta::transferName(int transfer) {
    switch (transfer) {
        case AVCOL_TRC_BT709: return "bt709";
        case AVCOL_TRC_GAMMA22: return "gamma22";
        case AVCOL_TRC_GAMMA28: return "gamma28";
        case AVCOL_TRC_SMPTE170M: return "smpte170m";
        case AVCOL_TRC_SMPTE240M: return "smpte240m";
        case AVCOL_TRC_LINEAR: return "linear";
        case AVCOL_TRC_LOG: return "log100";
        case AVCOL_TRC_LOG_SQRT: return "log316";
        case AVCOL_TRC_IEC61966_2_4: return "iec61966_2_4";
        case AVCOL_TRC_IEC61966_2_1: return "srgb";
        case AVCOL_TRC_BT2020_10: return "bt2020_10";
        case AVCOL_TRC_BT2020_12: return "bt2020_12";
        case AVCOL_TRC_SMPTE2084: return "pq";
        case AVCOL_TRC_ARIB_STD_B67: return "hlg";
        case AVCOL_TRC_UNSPECIFIED: return "unspecified";
        default: return "reserved";
    }
}

std::string ColorMeta::matrixName(int matrix) {
    switch (matrix) {
        case AVCOL_SPC_RGB: return "rgb";
        case AVCOL_SPC_BT709: return "bt709";
        case AVCOL_SPC_FCC: return "fcc";
        case AVCOL_SPC_BT470BG: return "bt470bg";
        case AVCOL_SPC_SMPTE170M: return "smpte170m";
        case AVCOL_SPC_SMPTE240M: return "smpte240m";
        case AVCOL_SPC_YCGCO: return "ycgco";
        case AVCOL_SPC_BT2020_NCL: return "bt2020_ncl";
        case AVCOL_SPC_BT2020_CL: return "bt2020_cl";
        case AVCOL_SPC_CHROMA_DERIVED_NCL: return "chroma_derived_ncl";
        case AVCOL_SPC_CHROMA_DERIVED_CL: return "chroma_derived_cl";
        case AVCOL_SPC_UNSPECIFIED: return "unspecified";
        default: return "reserved";
    }
}

std::string ColorMeta::chromaLocationName(int loc) {
    switch (loc) {
        case AVCHROMA_LOC_LEFT: return "left";
        case AVCHROMA_LOC_CENTER: return "center";
        case AVCHROMA_LOC_TOPLEFT: return "topleft";
        case AVCHROMA_LOC_TOP: return "top";
        case AVCHROMA_LOC_BOTTOMLEFT: return "bottomleft";
        case AVCHROMA_LOC_BOTTOM: return "bottom";
        case AVCHROMA_LOC_UNSPECIFIED: return "unspecified";
        default: return "reserved";
    }
}

void ColorMeta::toJsonFields(JsonWriter& w) const {
    w.kv("range", rangeName(range));
    w.kv("primaries", primariesName(primaries));
    w.kv("transfer", transferName(transfer));
    w.kv("matrix", matrixName(matrix));
    w.kv("chroma_location", chromaLocationName(chromaLocation));
    w.kv("pixel_format", av_get_pix_fmt_name(static_cast<AVPixelFormat>(pixelFormat))
                             ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(pixelFormat))
                             : std::string("invalid"));
    w.kv("bit_depth", bitDepth);
    w.kv("hdr_mastering_display_present", hasMasteringDisplay);
    w.kv("hdr_content_light_level_present", hasContentLightLevel);
}

} // namespace anvil
