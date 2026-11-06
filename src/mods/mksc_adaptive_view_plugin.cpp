#include "mod_runtime.h"
namespace {

static int g_user_widescreen_enabled = 1;

void apply_adaptive_view() {
    if (!g_user_widescreen_enabled) {
        (void)gba_mod_set_adaptive_view_enabled(0);
        (void)gba_mod_set_view_width(0);
    } else {
        (void)gba_mod_set_adaptive_view_enabled(1);
        (void)gba_mod_set_view_width(284);
    }
}

void reset_adaptive_view() {
    apply_adaptive_view();
}

void activate_adaptive_view() {
    apply_adaptive_view();
}

} // namespace

GBA_MOD_CONSTRUCTOR(mksc_register_adaptive_view_plugin) {
    (void)gba_mod_register_reset_callback(reset_adaptive_view);
    (void)gba_mod_register_activation_plugin(
        "mario-kart-super-circuit.adaptive-view", activate_adaptive_view);
}

extern "C" void gba_mod_set_adaptive_view_user_config(int enabled) {
    g_user_widescreen_enabled = enabled;
    apply_adaptive_view();
}
