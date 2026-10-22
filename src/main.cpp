#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "runtime.h"
#include "mksc_extended_view.h"
#include "mksc_flash_dispatch.h"
#if defined(GBAGAME_NETPLAY)
#include "multiplayer_launch.h"
#include "gba_netplay_build_identity.h"
#endif

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif


#if defined(__SWITCH__)
#include <switch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#endif

int main(int argc, char** argv) {
#if defined(__SWITCH__)
    // Create necessary directories on Switch
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/mksc_nx", 0777);
    mkdir("sdmc:/switch/mksc_nx/save", 0777);

    bool config_widescreen = true;
    bool config_fps60 = true;
    bool config_linear_filter = false;
    bool config_show_fps = false;

    // Config Parsing
    std::string config_path = "sdmc:/switch/mksc_nx/config.txt";
    std::ifstream config_file(config_path);
    if (config_file.is_open()) {
        std::string line;
        while (std::getline(config_file, line)) {
            if (line.find("widescreen = 0") != std::string::npos) config_widescreen = false;
            if (line.find("widescreen = 1") != std::string::npos) config_widescreen = true;
            if (line.find("fps60 = 0") != std::string::npos) config_fps60 = false;
            if (line.find("fps60 = 1") != std::string::npos) config_fps60 = true;
            if (line.find("linear_filter = 0") != std::string::npos) config_linear_filter = false;
            if (line.find("linear_filter = 1") != std::string::npos) config_linear_filter = true;
            if (line.find("show_fps = 0") != std::string::npos) config_show_fps = false;
            if (line.find("show_fps = 1") != std::string::npos) config_show_fps = true;
        }
    } else {
        std::ofstream new_config(config_path);
        new_config << "widescreen = 1\n";
        new_config << "fps60 = 1\n";
        new_config << "linear_filter = 0\n";
        new_config << "show_fps = 0\n";
    }

#endif

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("MarioKartSuperCircuitRecomp v%s\n", MKSC_VERSION);
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "MarioKartSuperCircuitRecomp [--bios <path>] [--rom <path>] [game.toml]\n");
            return 0;
        }
    }
    g_runtime_ram_dispatch_hook = &mksc::flash_stack_dispatch;
    gbarecomp::RunOptions opts;
    std::vector<std::string> args(argv, argv + argc);

#if defined(__SWITCH__)
    // Enforce Paths and Options for Switch


    opts.launcher_save_path = "sdmc:/switch/mksc_nx/save/mario_kart_super_circuit_usa.sav";
#endif

#if defined(GBAGAME_NETPLAY) && !defined(__SWITCH__)
    opts.netplay=gbarecomp::make_gba_netplay_launch("mksc-usa",GBARECOMP_NETPLAY_BUILD_ID,
        "aadc8b9f3c947ff6f610b6c8d7fddaaa9bea98b4bd43576df84f26c457ae1f90");
    opts.netplay->view_policy = {true, true, mksc::install_extended_view,
        nullptr, mksc::reset_extended_view};
    opts.netplay->view_policy.high_resolution_affine_provider = &mksc::high_resolution_affine_provider;
    try { gbarecomp::parse_gba_netplay_arguments(args,*opts.netplay); }
    catch (const std::exception& e) { std::fprintf(stderr,"netplay: %s\n",e.what()); return 1; }
#endif
    opts.builtin_game_name = "Mario Kart: Super Circuit (USA)";
    opts.builtin_rom_sha1 = "9d327c030c3e2d9007990518594f70c3340ac56f";
    opts.builtin_rom_crc32 = 0xED316E37u;
    opts.mod_game_id = "mario-kart-super-circuit-us";
    opts.mod_owns_adaptive_view = true;
    opts.high_resolution_affine_provider = &mksc::high_resolution_affine_provider;
    opts.max_view_width = 480;
    opts.max_resize_view_width = 480;
    opts.resize_driven_view = true;
    opts.extended_view_init = &mksc::install_extended_view;
    // Zero retains existing mod selections when upgrading an older config.
    // Explicit Display modes override the optional view mod for this run.
    static const char* const view_labels[] = {"Default (Mods)", "Native (3:2)", "16:9"};
    static const uint16_t view_widths[] = {0, 240, 284};
    opts.launcher_aspect_labels = view_labels;
    opts.launcher_aspect_view_widths = view_widths;
    opts.launcher_num_aspects = 3;
    opts.launcher_expose_widescreen = true;
    opts.launcher_expose_adaptive_view = true;
    opts.launcher_expose_sharp_filter = true;
    opts.launcher_default_sharp_filter = true;
    opts.launcher_expose_affine_filter = true;
    opts.launcher_default_affine_filter = true;
    opts.launcher_region = "USA";
#if !defined(__SWITCH__)
    // The launcher reads [rom].path and [bios].path from this file when its
    // per-user cache is empty, so the verified local ROM is preselected.
    opts.launcher_game_config = "game.toml";
    opts.launcher_save_path = "saves/mario_kart_super_circuit_usa.sav";
#endif



#if defined(GBAGAME_RECOMP_UI) && !defined(__SWITCH__)
    if (game_launcher_preboot(args, opts)) return 0;
#endif
    std::vector<char*> av;
    av.push_back(argv[0]);
#if defined(__SWITCH__)
    std::string s_bios_flag = "--bios";
    std::string s_bios_path = "sdmc:/switch/mksc_nx/gba_bios.bin";
    std::string s_rom_flag = "--rom";
    std::string s_rom_path = "sdmc:/switch/mksc_nx/rom.gba";
    av.push_back(s_bios_flag.data());
    av.push_back(s_bios_path.data());
    av.push_back(s_rom_flag.data());
    av.push_back(s_rom_path.data());

    std::string s_mod_flag = "--mod";
    std::string s_60fps = "mario-kart-super-circuit.track-60fps";
    std::string s_view = "mario-kart-super-circuit.adaptive-view";

    if (config_fps60) {
        av.push_back(s_mod_flag.data());
        av.push_back(s_60fps.data());
    }
    if (config_widescreen) {
        av.push_back(s_mod_flag.data());
        av.push_back(s_view.data());
    }
#else
    for (size_t i = 1; i < args.size(); ++i) av.push_back(args[i].data());
#endif

#if defined(__SWITCH__)
    // Force specific arguments if needed to bypass launcher or set filters
    if (config_linear_filter) {
        av.push_back(const_cast<char*>("--linear-filter"));
    }
    if (config_show_fps) {
        av.push_back(const_cast<char*>("--show-fps"));
    }
#endif

    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
}
