#include "multiplayer_launch.h"
#include "../src/mksc_extended_view.h"
#include "../src/mksc_margin_objects.h"
#include "../src/mksc_flash_dispatch.h"
#include <cstdio>

namespace {
void report_and_reset() {
    const auto m=mksc::margin_object_totals();
    std::printf("mksc_margin candidates=%llu pieces=%llu native=%llu wrapped_x=%llu rows=%llu pixels=%llu\n",
        (unsigned long long)m.candidates,(unsigned long long)m.pieces,(unsigned long long)m.native_pieces,
        (unsigned long long)m.wrapped_x,(unsigned long long)m.rows,(unsigned long long)m.pixels);
    mksc::reset_extended_view();
}
}

gbarecomp::GbaNetplayLaunch gba_view_probe_game() {
    g_runtime_ram_dispatch_hook = &mksc::flash_stack_dispatch;
    gbarecomp::GbaNetplayLaunch game;
    game.program_id="mksc-usa";
    game.view_policy={true,true,mksc::install_extended_view,nullptr,report_and_reset};
    game.view_policy.high_resolution_affine_provider=&mksc::high_resolution_affine_provider;
    return game;
}
