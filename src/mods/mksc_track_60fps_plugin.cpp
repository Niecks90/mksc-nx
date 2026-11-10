#include "mod_runtime.h"
#include "runtime_arm.h"

namespace {

constexpr uint32_t kAffineBufferSelectorImmediatePc = 0x08030C14u;

RuntimeThumbAluImmediateOverride previous_override = nullptr;
bool installed = false;

int override_affine_buffer_selector(uint32_t instruction_pc,
                                    uint32_t original_value,
                                    uint32_t* out_value) {
    if (__builtin_expect(instruction_pc == kAffineBufferSelectorImmediatePc, 0) &&
        original_value == 1u && out_value) {
        // The following guest SUBS becomes selector - selector, keeping the
        // HDMA-visible output buffer selected and freshly generated each frame.
        *out_value = g_cpu.R[1];
        return 1;
    }
    return previous_override
        ? previous_override(instruction_pc, original_value, out_value)
        : 0;
}

static int g_user_60fps_track_enabled = 1;

void activate_track_60fps() {
    if (!g_user_60fps_track_enabled) {
        if (installed) {
            if (g_runtime_thumb_alu_imm_override == override_affine_buffer_selector) {
                g_runtime_thumb_alu_imm_override = previous_override;
            }
            previous_override = nullptr;
            installed = false;
        }
        return;
    }
    if (installed) return;
    previous_override = g_runtime_thumb_alu_imm_override;
    g_runtime_thumb_alu_imm_override = override_affine_buffer_selector;
    installed = true;
}

void reset_track_60fps() {
    if (installed) {
        if (g_runtime_thumb_alu_imm_override == override_affine_buffer_selector) {
            g_runtime_thumb_alu_imm_override = previous_override;
        }
        previous_override = nullptr;
        installed = false;
    }
    if (g_user_60fps_track_enabled) {
        activate_track_60fps();
    }
}

}  // namespace






GBA_MOD_CONSTRUCTOR(mksc_register_track_60fps_plugin) {
    (void)gba_mod_register_reset_callback(reset_track_60fps);
    (void)gba_mod_register_activation_plugin(
        "mario-kart-super-circuit.track-60fps", activate_track_60fps);
}

extern "C" void gba_mod_set_60fps_track_enabled(int enabled) {
    g_user_60fps_track_enabled = enabled;
    if (enabled) {
        activate_track_60fps();
    } else {
        if (installed) {
            if (g_runtime_thumb_alu_imm_override == override_affine_buffer_selector) {
                g_runtime_thumb_alu_imm_override = previous_override;
            }
            previous_override = nullptr;
            installed = false;
        }
    }
}
