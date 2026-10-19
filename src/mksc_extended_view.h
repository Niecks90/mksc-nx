#pragma once

#include <cstdint>

namespace mksc {
void reset_extended_view();
bool high_resolution_affine_provider(int bg, int screen_y);

void install_extended_view(std::uint32_t extra_left,
                           std::uint32_t extra_right);

}  // namespace mksc
