#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

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
#include "host_window.h"

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
extern "C" void gba_mod_set_adaptive_view_user_config(int enabled);
bool g_config_show_fps = false;

static void mksc_presentation_request(gbarecomp::RunOptions::PresentationRequest* req) {
    if (!req) return;
    req->native_view = (g_mksc_qol.widescreen == MKSC_WS_CLASSIC);
    req->anchor_top = false;
}

static const char* mksc_filter_token(int f) {
    switch (f) {
        case MKSC_FILTER_BILINEAR: return "bilinear";
        case MKSC_FILTER_SHARP4X:  return "sharp4x";
        case MKSC_FILTER_LCD_GRID: return "lcd";
        default:                   return "nearest";
    }
}

static const char* mksc_color_token(int c) {
    switch (c) {
        case MKSC_COLOR_BACKLIT:  return "backlit";
        case MKSC_COLOR_FRONTLIT: return "frontlit";
        case MKSC_COLOR_UNLIT:    return "unlit";
        case MKSC_COLOR_CLASSIC:  return "classic";
        default:                  return "raw";
    }
}

extern "C" void save_mksc_config(void) {
    g_config_audio = (g_mksc_qol.audio_enabled != 0);
    g_config_show_fps = (g_mksc_qol.show_fps != 0);
    const char* path = (g_mksc_qol.config_path[0] != '\0')
        ? g_mksc_qol.config_path
        : "sdmc:/switch/mksc_nx/config.txt";
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    out << "widescreen = " << (g_mksc_qol.widescreen ? 1 : 0) << "\n";
    out << "fps60 = " << (g_mksc_qol.fps60 ? 1 : 0) << "\n";
    out << "video_filter = " << mksc_filter_token(g_mksc_qol.video_filter) << "\n";
    out << "color_mode = " << mksc_color_token(g_mksc_qol.color_mode) << "\n";
    out << "linear_filter = " << (g_mksc_qol.video_filter == MKSC_FILTER_BILINEAR ? 1 : 0) << "\n";
    out << "high_res_affine = " << (g_mksc_qol.high_res_affine ? 1 : 0) << "\n";
    out << "vsync = " << (g_mksc_qol.vsync ? 1 : 0) << "\n";
    out << "uncapped = " << (g_mksc_qol.uncapped ? 1 : 0) << "\n";
    out << "cpu_boost = " << g_mksc_qol.cpu_boost << "\n";
    out << "show_fps = " << (g_mksc_qol.show_fps ? 1 : 0) << "\n";
    out << "audio = " << (g_mksc_qol.audio_enabled ? 1 : 0) << "\n";
}

extern "C" void apply_mksc_mods(void) {
    gba_mod_set_60fps_track_enabled(g_mksc_qol.fps60 ? 1 : 0);
}

struct GameThreadArgs {
    int argc;
    char** argv;
    gbarecomp::RunOptions* opts;
    int return_code;
};

static void game_runner_thread(void* arg) {
    auto* ctx = static_cast<GameThreadArgs*>(arg);
    const Result affinity_rc =
        svcSetThreadCoreMask(threadGetSelf()->handle, 1, 1u << 1);
    log_boot("Game thread affinity=0x%x rc=0x%08x on Core %u\n",
             1u << 1, affinity_rc, svcGetCurrentProcessorNumber());
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
    mkdir("sdmc:/switch/mksc_nx/savestates", 0777);
    chdir("sdmc:/switch/mksc_nx");
    setenv("GBARECOMP_BIOS_HLE", "1", 1);
    setenv("GBARECOMP_DIAGNOSTICS", "0", 1);
    // On Switch, calling present() with SDL_RENDERER_PRESENTVSYNC on the
    // emulation thread (Core 1) causes SDL_RenderPresent to block up to
    // 16.67 ms/frame waiting for scanout, starving the GBA emulation loop and
    // reducing frame rate to <= 30 FPS. Setting GBARECOMP_NO_VSYNC=1 ensures
    // SDL_RenderPresent returns immediately, pacing smoothly at 60 FPS via audio.
    setenv("GBARECOMP_NO_VSYNC", "1", 1);
    unsetenv("GBARECOMP_VSYNC");

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
    int config_video_filter = MKSC_FILTER_NEAREST;
    bool has_explicit_video_filter = false;
    int config_color_mode = MKSC_COLOR_RAW;
    bool config_high_res_affine = false;
    bool config_vsync = false;
    bool config_uncapped = false;
    int config_cpu_boost = 0; // 0 = Normal (1020 MHz CPU, full GPU 307/768 MHz), 1 = FastLoad (1785 MHz CPU, min GPU), 2 = Type2 (1020 MHz CPU, min GPU)
    g_config_show_fps = false;
    g_config_audio = true;

    std::string config_paths[] = {
        "sdmc:/switch/mksc_nx/config.txt",
        "sdmc:/switch/mksc-nx/config.txt",
        "./config.txt"
    };

    std::string loaded_config_path;
    std::ifstream config_file;
    for (const auto& path : config_paths) {
        config_file.clear();
        config_file.open(path);
        if (config_file.is_open()) {
            loaded_config_path = path;
            break;
        }
    }

    if (config_file.is_open()) {
        log_boot("Loaded config from %s\n", loaded_config_path.c_str());
        std::string line;
        while (std::getline(config_file, line)) {
            // Ignorer les lignes vides et les commentaires
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#' || line[first] == ';') continue;
            // Strip whitespace
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (auto& c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                const bool is_true = (val == "1" || val == "true" || val == "on" || val == "yes");
                const bool is_false = (val == "0" || val == "false" || val == "off" || val == "no");

                if (key == "widescreen") {
                    if (is_true || val == "wide" || val == "auto") config_widescreen = true;
                    else if (is_false || val == "classic") config_widescreen = false;
                } else if (key == "fps60" || key == "60fps" || key == "fps_60" || key == "60_fps" ||
                           key == "track_60fps" || key == "track60fps" || key == "track_fps60") {
                    if (is_true) config_fps60 = true;
                    else if (is_false) config_fps60 = false;
                } else if (key == "video_filter" || key == "videofilter" || key == "filter") {
                    has_explicit_video_filter = true;
                    if (val == "bilinear" || val == "linear" || val == "1") config_video_filter = MKSC_FILTER_BILINEAR;
                    else if (val == "sharp4x" || val == "sharp" || val == "2") config_video_filter = MKSC_FILTER_SHARP4X;
                    else if (val == "lcd" || val == "lcd_grid" || val == "grid" || val == "3") config_video_filter = MKSC_FILTER_LCD_GRID;
                    else config_video_filter = MKSC_FILTER_NEAREST;
                } else if (key == "color_mode" || key == "colormode" || key == "color" ||
                           key == "color_correction" || key == "colorcorrection") {
                    if (val == "backlit" || val == "1" || val == "true" || val == "on" || val == "yes") config_color_mode = MKSC_COLOR_BACKLIT;
                    else if (val == "frontlit" || val == "2") config_color_mode = MKSC_COLOR_FRONTLIT;
                    else if (val == "unlit" || val == "3") config_color_mode = MKSC_COLOR_UNLIT;
                    else if (val == "classic" || val == "4") config_color_mode = MKSC_COLOR_CLASSIC;
                    else config_color_mode = MKSC_COLOR_RAW;
                } else if (key == "linear_filter" || key == "linearfilter") {
                    if (!has_explicit_video_filter && is_true) config_video_filter = MKSC_FILTER_BILINEAR;
                } else if (key == "high_res_affine" || key == "highresaffine") {
                    if (is_true) config_high_res_affine = true;
                    else if (is_false) config_high_res_affine = false;
                } else if (key == "show_fps" || key == "showfps") {
                    if (is_true) g_config_show_fps = true;
                    else if (is_false) g_config_show_fps = false;
                } else if (key == "vsync") {
                    if (is_true) config_vsync = true;
                    else if (is_false) config_vsync = false;
                } else if (key == "uncapped") {
                    if (is_true) config_uncapped = true;
                    else if (is_false) config_uncapped = false;
                } else if (key == "cpu_boost" || key == "cpuboost") {
                    config_cpu_boost = std::clamp(std::atoi(val.c_str()), 0, 2);
                } else if (key == "audio") {
                    if (is_true) g_config_audio = true;
                    else if (is_false) g_config_audio = false;
                }
            }
        }
    } else {
        loaded_config_path = "sdmc:/switch/mksc_nx/config.txt";
        log_boot("Config not found, creating default at %s\n", loaded_config_path.c_str());
    }

    g_mksc_qol.widescreen = config_widescreen ? MKSC_WS_WIDE : MKSC_WS_CLASSIC;
    g_mksc_qol.video_filter = config_video_filter;
    g_mksc_qol.color_mode = config_color_mode;
    g_mksc_qol.fps60 = config_fps60 ? 1 : 0;
    g_mksc_qol.high_res_affine = config_high_res_affine ? 1 : 0;
    g_mksc_qol.show_fps = g_config_show_fps ? 1 : 0;
    g_mksc_qol.vsync = config_vsync ? 1 : 0;
    g_mksc_qol.uncapped = config_uncapped ? 1 : 0;
    g_mksc_qol.cpu_boost = config_cpu_boost;
    g_mksc_qol.audio_enabled = g_config_audio ? 1 : 0;
    g_mksc_qol.reset_requested = 0;
    std::snprintf(g_mksc_qol.config_path, sizeof(g_mksc_qol.config_path), "%s",
                  loaded_config_path.empty() ? "sdmc:/switch/mksc_nx/config.txt" : loaded_config_path.c_str());
    std::snprintf(g_mksc_qol.savestate_dir, sizeof(g_mksc_qol.savestate_dir),
                  "sdmc:/switch/mksc_nx/savestates");
    g_mksc_qol.save_config_fn = &save_mksc_config;
    g_mksc_qol.apply_mods_fn = &apply_mksc_mods;
    if (!config_file.is_open()) {
        save_mksc_config();
    }

    ApmCpuBoostMode boost_mode = ApmCpuBoostMode_Normal;
    if (config_cpu_boost == 1) boost_mode = ApmCpuBoostMode_FastLoad;
    else if (config_cpu_boost == 2) boost_mode = ApmCpuBoostMode_Type2;
    const Result boost_rc = appletSetCpuBoostMode(boost_mode);
    log_boot("CPU boost mode %d rc=0x%08x\n", config_cpu_boost, boost_rc);
    if (config_vsync) {
        setenv("GBARECOMP_VSYNC", "1", 1);
        unsetenv("GBARECOMP_NO_VSYNC");
    } else {
        setenv("GBARECOMP_NO_VSYNC", "1", 1);
        unsetenv("GBARECOMP_VSYNC");
    }
    log_boot("Config: widescreen=%d, fps60=%d, filter=%d, color=%d, vsync=%d, uncapped=%d, cpu_boost=%d, high_res_affine=%d, show_fps=%d, audio=%d\n",
             g_mksc_qol.widescreen, g_mksc_qol.fps60, g_mksc_qol.video_filter, g_mksc_qol.color_mode,
             g_mksc_qol.vsync, g_mksc_qol.uncapped, g_mksc_qol.cpu_boost, g_mksc_qol.high_res_affine,
             g_mksc_qol.show_fps, g_mksc_qol.audio_enabled);
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
#if defined(__SWITCH__)
    opts.builtin_game_name = "Mario Kart: Super Circuit NX";
#else
    opts.builtin_game_name = "Mario Kart: Super Circuit (USA)";
#endif
    opts.builtin_rom_sha1 = "9d327c030c3e2d9007990518594f70c3340ac56f";
    opts.builtin_rom_crc32 = 0xED316E37u;
    opts.mod_game_id = "mario-kart-super-circuit-us";
    opts.mod_owns_adaptive_view = true;
#if defined(__SWITCH__)
    opts.high_resolution_affine_provider = g_mksc_qol.high_res_affine ? &mksc::high_resolution_affine_provider : nullptr;
    opts.max_view_width = 480;
    opts.max_resize_view_width = 480;
    opts.resize_driven_view = true;
    opts.extended_view_init = &mksc::install_extended_view;
    opts.presentation_request = &mksc_presentation_request;
#else
    opts.high_resolution_affine_provider = &mksc::high_resolution_affine_provider;
    opts.max_view_width = 480;
    opts.max_resize_view_width = 480;
    opts.resize_driven_view = true;
    opts.extended_view_init = &mksc::install_extended_view;
#endif
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
    gba_mod_set_adaptive_view_user_config(1);

    static char arg_prog[]        = "mksc_nx";
    static char arg_bios_flag[]   = "--bios";
    static char arg_bios_path[]   = "sdmc:/switch/mksc_nx/gba_bios.bin";
    static char arg_rom_flag[]    = "--rom";
    static char arg_rom_path[]    = "sdmc:/switch/mksc_nx/rom.gba";
    static char arg_save_flag[]   = "--save";
    static char arg_save_path[]   = "sdmc:/switch/mksc_nx/save/mario_kart_super_circuit_usa.sav";
    static char arg_state_flag[]  = "--state-dir";
    static char arg_state_dir[]   = "sdmc:/switch/mksc_nx/savestates";
    static char arg_linear_flag[] = "--linear-filter";
    static char arg_uncapped_flag[] = "--uncapped";
    static char arg_hle[] = "--bios-hle";

    int last_return_code = 0;
    do {
        g_mksc_qol.reset_requested = 0;
        g_config_audio = (g_mksc_qol.audio_enabled != 0);
        g_config_show_fps = (g_mksc_qol.show_fps != 0);
        opts.show_fps_by_default = g_config_show_fps;
        gba_mod_set_60fps_track_enabled(g_mksc_qol.fps60 ? 1 : 0);
        mksc::reset_extended_view();
        if (g_mksc_qol.vsync) {
            setenv("GBARECOMP_VSYNC", "1", 1);
            unsetenv("GBARECOMP_NO_VSYNC");
        } else {
            setenv("GBARECOMP_NO_VSYNC", "1", 1);
            unsetenv("GBARECOMP_VSYNC");
        }

        std::vector<char*> av;
        av.push_back(argv && argv[0] ? argv[0] : arg_prog);
        av.push_back(arg_bios_flag);
        av.push_back(arg_bios_path);
        av.push_back(arg_rom_flag);
        av.push_back(arg_rom_path);
        av.push_back(arg_save_flag);
        av.push_back(arg_save_path);
        av.push_back(arg_state_flag);
        av.push_back(arg_state_dir);
        av.push_back(arg_hle);

        if (g_mksc_qol.video_filter == MKSC_FILTER_BILINEAR) {
            av.push_back(arg_linear_flag);
        }
        if (g_mksc_qol.uncapped) {
            av.push_back(arg_uncapped_flag);
        }
        av.push_back(nullptr);

        GameThreadArgs thread_args;
        thread_args.argc = static_cast<int>(av.size() - 1);
        thread_args.argv = av.data();
        thread_args.opts = &opts;
        thread_args.return_code = 0;

        Thread game_thread;
        constexpr size_t kStackSize = 32 * 1024 * 1024; // 32 MiB stack
        Result rc = threadCreate(&game_thread, game_runner_thread, &thread_args, nullptr, kStackSize, 0x2C, 1);
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
        last_return_code = thread_args.return_code;
        if (g_mksc_qol.reset_requested) {
            log_boot("Reset requested from QoL menu; rebooting runtime...\n");
        }
    } while (g_mksc_qol.reset_requested != 0);

    return last_return_code;
#else
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#endif
}