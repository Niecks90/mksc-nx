#include "mod_runtime.h"
#include <cstring>

namespace {

static int g_user_widescreen_enabled = 1;

void reset_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(0);
    (void)gba_mod_set_view_width(0);
}

void activate_adaptive_view() {
    if (!g_user_widescreen_enabled) {
        (void)gba_mod_set_adaptive_view_enabled(0);
        (void)gba_mod_set_view_width(0);
        return;
    }
    const char* mode = gba_mod_option_value(
        "mario-kart-super-circuit.enhancement.adaptive-view", "adaptive-view", "view");
    const bool fixed = mode && std::strcmp(mode, "16:9") == 0;
    (void)gba_mod_set_adaptive_view_enabled(fixed ? 0 : 1);
    (void)gba_mod_set_view_width(fixed ? 284 : 0);
}

} // namespace

GBA_MOD_CONSTRUCTOR(mksc_register_adaptive_view_plugin) {
    (void)gba_mod_register_reset_callback(reset_adaptive_view);
    (void)gba_mod_register_activation_plugin(
        "mario-kart-super-circuit.adaptive-view", activate_adaptive_view);
}

extern "C" void gba_mod_set_adaptive_view_user_config(int enabled) {
    g_user_widescreen_enabled = enabled;
    if (!enabled) {
        (void)gba_mod_set_adaptive_view_enabled(0);
        (void)gba_mod_set_view_width(0);
    }
}
