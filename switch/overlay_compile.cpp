#include "overlay_compile.h"
#include <string>

namespace gbarecomp {

const char* heal_backend_name(HealBackend b) {
    return "none";
}

bool overlay_bundled_tcc_available() {
    return false;
}

bool overlay_compile_one(const OverlayWorkItem& w,
                         const std::string& build_dir,
                         const GbaOverlayCallbacks* callbacks,
                         bool compile_if_missing,
                         HealBackend backend,
                         OverlayCompiled* out,
                         std::string* err) {
    if (err) {
        *err = "Dynamic overlay compilation is disabled on Nintendo Switch.";
    }
    return false;
}

} // namespace gbarecomp
