#pragma once
#include <cstdint>

namespace gba { struct WsMarginObjPixel; }
namespace mksc {
// Read-only AMKE0 billboard presentation. These buffers never become OAM.
const gba::WsMarginObjPixel* margin_objects(int y, int* left, int* width);
int margin_object_x(int oam_index, std::uint16_t attr0, std::uint16_t attr1,
                    std::uint16_t attr2, int* x);
void reset_margin_objects();
struct MarginObjectMetrics {
    std::uint64_t candidates = 0, pieces = 0, native_pieces = 0, wrapped_x = 0;
    std::uint64_t rows = 0, pixels = 0;
};
MarginObjectMetrics margin_object_metrics();
MarginObjectMetrics margin_object_totals();
}
