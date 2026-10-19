#include "mksc_margin_objects.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include "gba_simulation_state.h"
#include "runtime_bus_bridge.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

extern "C" {
unsigned g_ws_extra_left=71, g_ws_extra_right=71;
extern unsigned long long g_runtime_state_epoch;
}
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main() {
    auto bus=std::make_unique<gba::GbaBus>();
    auto ppu=std::make_unique<gba::GbaPpu>();
    std::vector<std::uint8_t> rom(1024);
    auto rom16=[&](unsigned p,unsigned n){rom[p]=n;rom[p+1]=n>>8;};
    auto rom32=[&](unsigned p,unsigned n){rom16(p,n);rom16(p+2,n>>16);};
    for (unsigned level=0;level<4;++level) {
        rom32(0x100+level*8,0x08000200+level*8); rom32(0x104+level*8,level);
        rom16(0x200+level*8,1); // one 8x8, 4bpp piece per mip level
        rom16(0x202+level*8,0); rom16(0x204+level*8,0); rom16(0x206+level*8,0x0c01+level);
    }
    bus->set_rom(rom.data(),rom.size()); bus->rtc().set_emulated_clock(0);
    gbarecomp::set_active_bus(bus.get());
    constexpr unsigned actor=0x02001000, root=0x03002e44, camera=0x03003668;
    bus->write32(root+0x6f4,actor);
    bus->write16(root+0xbec,0x7f00); // checkpoints 0..127
    bus->write16(actor+0x34,1); bus->write32(actor+0x4c,0x08000100);
    bus->write16(actor+0x56,256); bus->write16(actor+0x58,256);
    bus->write32(camera+0x10,0x1000000); bus->write32(camera+0x24,120);
    bus->write32(camera+0x5c,32768); bus->write32(camera+0x6c,32768); bus->write32(camera+0x7c,32768);
    for (unsigned i=0;i<128;++i) bus->write16(0x07000000+i*8,0x200);
    for (unsigned t=1;t<5;++t) for (unsigned i=0;i<32;++i) bus->write8(0x06010000+t*32+i,0x11*t);
    for (unsigned i=1;i<5;++i) bus->write16(0x05000200+i*2,i*3);
    auto position=[&](int x,int y=100,int scale=256){
        const int depth=scale/4;
        bus->write32(camera+0x40,std::uint32_t(2*(depth*65536-0x1000000)));
        bus->write32(actor,std::uint32_t((x-120)*depth*512));
        bus->write32(actor+4,std::uint32_t(y*depth*512));
        bus->write16(actor+0x20,x);bus->write16(actor+0x22,y);bus->write16(actor+0x24,scale);
        ++g_runtime_vblank_starts;
    };
    auto pixel=[&](int x,int y=100){
        int left=0,width=0;const auto* row=mksc::margin_objects(y,&left,&width);
        CHECK(row && left==-int(g_ws_extra_left) && width==240+int(g_ws_extra_left+g_ws_extra_right));
        return row[x-left];
    };
    // New billboards must exist outside the native viewport even if there was
    // never a matching OAM entry, including the 255/256 signed-X boundary.
    for (int x : {-72,-65,-64,-1,0,232,240,255,256,304,305,310,311}) {
        position(x);
        const auto before=gba::save_device_state(*bus,*ppu);
        for (int sample=-71;sample<311;++sample) {
            const bool visible=sample>=x && sample<x+8 && (sample<0 || sample>=240);
            CHECK(bool(!(pixel(sample).color&0x8000))==visible);
        }
        CHECK(before==gba::save_device_state(*bus,*ppu));
    }
    // Palette changes at a published row are not hidden in a pixel cache.
    position(270);CHECK(pixel(270).color==3);
    bus->write16(root+0xbec,0x0501); // native draw slice excludes checkpoint 8
    bus->write8(actor+0x36,8);++g_runtime_vblank_starts;
    CHECK(pixel(270).color==3); // resident scenery uses the widened frustum
    bus->write16(0x05000202,31);CHECK(pixel(270).color==31);
    // Existing native OAM is reused; only the expanded interpretation changes.
    position(256);
    bus->write16(0x07000060,100);bus->write16(0x07000062,256);bus->write16(0x07000064,0x0c01);
    ++g_runtime_vblank_starts;
    int x=0;CHECK(mksc::margin_object_x(12,100,256,0x0c01,&x)==1 && x==256);
    CHECK(pixel(256).color&0x8000); // no duplicate host sprite
    CHECK(mksc::margin_object_x(12,100,256,0x0c02,&x)==0);
    CHECK(mksc::margin_object_metrics().native_pieces==1);
    bus->write16(0x07000060,0x200);
    // A farther mip uses its actual alternate tile instead of a copied image
    // of the last sprite seen in the original screen.
    position(270,100,512);CHECK(pixel(270).color==6);
    // A restore to the same frame must still invalidate host projection data.
    bus->write16(actor+0x34,0);++g_runtime_state_epoch;
    CHECK(pixel(270).color&0x8000);
    bus->write16(actor+0x34,1);position(270);
    g_ws_extra_left=10;g_ws_extra_right=9;
    CHECK(pixel(248).color&0x8000);
    // Malformed/cyclic data stays bounded and never writes canonical memory.
    bus->write32(actor+0x3c,actor);++g_runtime_vblank_starts;
    CHECK(mksc::margin_object_metrics().candidates==1);
    bus->write32(actor+0x4c,0x01000000);++g_runtime_vblank_starts;
    CHECK(pixel(248).color&0x8000);
    mksc::reset_margin_objects();gbarecomp::set_active_bus(nullptr);
    std::puts("margin objects: unseen billboards, viewport edges, signed X, mip/palette, native reuse, state isolation and restore passed");
}
