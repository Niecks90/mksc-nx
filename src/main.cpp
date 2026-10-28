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
#include <SDL.h>
#include <unistd.h>
#include <switch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cstdarg>
#include <exception>

void log_boot(const char* fmt, ...) {
    FILE* f = fopen("sdmc:/switch/mksc_nx/boot.log", "a");
    if (f) {
        va_list args, args_copy;
        va_start(args, fmt);
        va_copy(args_copy, args);
        vfprintf(f, fmt, args);
        vprintf(fmt, args_copy);
        va_end(args_copy);
        va_end(args);
        fflush(f);
        fclose(f);
    }
}

extern "C" const uint32_t kDispatchTableLen;
extern "C" int gba_mod_set_adaptive_view_enabled(int);
extern "C" int gba_mod_set_60fps_track_enabled(int);
extern "C" bool g_config_audio = true;
bool g_config_show_fps = false;

struct GameThreadArgs {
    int argc;
    char** argv;
    gbarecomp::RunOptions* opts;
    int return_code;
};

static void game_runner_thread(void* arg) {
    auto* ctx = static_cast<GameThreadArgs*>(arg);
    try {
        log_boot("Starting gbarecomp runtime inside 32 MiB stack thread...\n");
        ctx->return_code = gbarecomp::run_game(ctx->argc, ctx->argv, *ctx->opts);
        log_boot("Exited gbarecomp runtime cleanly with code %d.\n", ctx->return_code);
    } catch (const std::exception& e) {
        log_boot("FATAL EXCEPTION in game thread: %s\n", e.what());
        ctx->return_code = 1;
    } catch (...) {
        log_boot("FATAL UNKNOWN EXCEPTION in game thread!\n");
        ctx->return_code = 1;
    }
}

#endif

int main(int argc, char** argv) {
#if defined(__SWITCH__)
    std::set_terminate([]() {
        log_boot("FATAL: std::terminate() called!\n");
    });
    atexit([]() {
        log_boot("Application exited via atexit / end.\n");
    });

    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/mksc_nx", 0777);
    mkdir("sdmc:/switch/mksc_nx/save", 0777);
    chdir("sdmc:/switch/mksc_nx");
    setenv("GBARECOMP_BIOS_HLE", "1", 1);
    setenv("GBARECOMP_DIAGNOSTICS", "0", 1);
    setenv("GBARECOMP_PRESENT_IN_PLACE", "1", 1);

    FILE* log_f = fopen("sdmc:/switch/mksc_nx/boot.log", "w");
    if (log_f) {
        fprintf(log_f, "--- MKSC NX Boot Log ---\n");
        fclose(log_f);
    }

    log_boot("Verified directories.\n");

    SDL_LogSetOutputFunction([](void* userdata, int category, SDL_LogPriority priority, const char* message) {
        log_boot("[SDL] (%d) %s\n", priority, message);
    }, nullptr);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    FILE* rom = fopen("sdmc:/switch/mksc_nx/rom.gba", "rb");
    if (rom) {
        fseek(rom, 0, SEEK_END);
        long size = ftell(rom);
        fclose(rom);
        log_boot("Found rom.gba: %ld bytes.\n", size);
    } else {
        log_boot("ERROR: Missing sdmc:/switch/mksc_nx/rom.gba!\n");
        printf("ERROR: Missing rom.gba!\n");
        svcSleepThread(5000000000ULL);
        return 1;
    }

    FILE* bios = fopen("sdmc:/switch/mksc_nx/gba_bios.bin", "rb");
    if (bios) {
        fseek(bios, 0, SEEK_END);
        long size = ftell(bios);
        fclose(bios);
        log_boot("Found gba_bios.bin: %ld bytes.\n", size);
    } else {
        log_boot("ERROR: Missing sdmc:/switch/mksc_nx/gba_bios.bin!\n");
        printf("ERROR: Missing gba_bios.bin!\n");
        svcSleepThread(5000000000ULL);
        return 1;
    }

    log_boot("kDispatchTableLen = %u\n", kDispatchTableLen);

    bool config_widescreen = true;
    bool config_fps60 = true;
    bool config_linear_filter = false;
    g_config_show_fps = false;

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
            if (line.find("show_fps = 0") != std::string::npos) g_config_show_fps = false;
            if (line.find("show_fps = 1") != std::string::npos) g_config_show_fps = true;
            if (line.find("audio = 0") != std::string::npos) g_config_audio = false;
            if (line.find("audio = 1") != std::string::npos) g_config_audio = true;
        }
    } else {
        std::ofstream new_config(config_path);
        new_config << "widescreen = 1\n";
        new_config << "fps60 = 1\n";
        new_config << "linear_filter = 0\n";
        new_config << "show_fps = 0\n";
        new_config << "audio = 1\n";
    }
    log_boot("Config: widescreen=%d, fps60=%d, linear_filter=%d, show_fps=%d\n", config_widescreen, config_fps60, config_linear_filter, g_config_show_fps);
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

#if defined(__SWITCH__)
    gba_mod_set_adaptive_view_enabled(config_widescreen ? 1 : 0);
    gba_mod_set_60fps_track_enabled(config_fps60 ? 1 : 0);

    if (!config_widescreen) {
        opts.high_resolution_affine_provider = nullptr;
    }


    static char arg_prog[]        = "mksc_nx";
    static char arg_bios_flag[]   = "--bios";
    static char arg_bios_path[]   = "sdmc:/switch/mksc_nx/gba_bios.bin";
    static char arg_rom_flag[]    = "--rom";
    static char arg_rom_path[]    = "sdmc:/switch/mksc_nx/rom.gba";
    static char arg_linear_flag[] = "--linear-filter";
    static char arg_hle[] = "--bios-hle";

    std::vector<char*> av;
    av.push_back(argv && argv[0] ? argv[0] : arg_prog);
    av.push_back(arg_bios_flag);
    av.push_back(arg_bios_path);
    av.push_back(arg_rom_flag);
    av.push_back(arg_rom_path);
    av.push_back(arg_hle);

    if (config_linear_filter) {
        av.push_back(arg_linear_flag);
    }
    av.push_back(nullptr);

    GameThreadArgs thread_args;
    thread_args.argc = static_cast<int>(av.size() - 1);
    thread_args.argv = av.data();
    thread_args.opts = &opts;
    thread_args.return_code = 0;

    Thread game_thread;
    constexpr size_t kStackSize = 32 * 1024 * 1024; // 32 MiB stack
    Result rc = threadCreate(&game_thread, game_runner_thread, &thread_args, nullptr, kStackSize, 0x2C, -2);
    if (R_FAILED(rc)) {
        log_boot("ERROR: Failed to create game thread (rc=0x%x)!\n", rc);
        return 1;
    }

    log_boot("Launching game thread...\n");
    rc = threadStart(&game_thread);
    if (R_FAILED(rc)) {
        log_boot("ERROR: Failed to start game thread (rc=0x%x)!\n", rc);
        threadClose(&game_thread);
        return 1;
    }

    threadWaitForExit(&game_thread);
    threadClose(&game_thread);

    return thread_args.return_code;
#else
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#endif
}