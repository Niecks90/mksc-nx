#include "mksc_margin_objects.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_bus_bridge.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

extern "C" unsigned g_ws_extra_left, g_ws_extra_right;
extern "C" unsigned long long g_runtime_state_epoch;

namespace mksc {
namespace {
// AMKE0 facts verified against the matching ROM layout and imported symbols:
// race state 03002E44, projection 03003668, actor draw lists +6F4/+704/+714.
// The original 08049B0C draw pass projects before its native horizontal test
// (0802F674), then submits cells at 0802FA1C. Reconstruct only host margins.
constexpr std::uint32_t race = 0x03002E44, camera = 0x03003668;
constexpr int absent = std::numeric_limits<int>::min();
MarginObjectMetrics sum_metrics(MarginObjectMetrics a,const MarginObjectMetrics& b) {
    a.candidates+=b.candidates; a.pieces+=b.pieces; a.native_pieces+=b.native_pieces;
    a.wrapped_x+=b.wrapped_x; a.rows+=b.rows; a.pixels+=b.pixels; return a;
}
struct Sprite {
    int x, y, width, height, bounds_w, bounds_h, depth;
    std::int16_t pa, pb, pc, pd;
    std::uint16_t a0, a1, a2;
    bool affine, native = false;
    unsigned order = 127;
};
struct PreparedFrame {
    const gba::GbaBus* bus = nullptr;
    unsigned long long frame = ~0ull;
    unsigned long long epoch = ~0ull;
    unsigned left = 0;
    unsigned right = 0;
    std::vector<Sprite> sprites;
    std::array<int, 128> object_x{};
    bool has_margin_sprites = false;
};

std::mutex g_margin_mutex;
std::shared_ptr<const PreparedFrame> g_prepared;
std::atomic<std::uint64_t> g_reset_gen{1};
MarginObjectMetrics g_metrics{};
MarginObjectMetrics g_completed_metrics{};
std::atomic<unsigned> g_metric_rows{0};
std::atomic<unsigned> g_metric_pixels{0};

thread_local std::shared_ptr<const PreparedFrame> t_prepared;
thread_local std::uint64_t t_reset_gen = 0;
thread_local std::array<gba::WsMarginObjPixel, gba::GbaPpu::kMaxRenderWidth> t_row{};

const std::uint8_t* region(const gba::GbaBus& bus, std::uint32_t a, unsigned n) {
    if (a >= 0x02000000 && a <= 0x02040000 && n <= 0x02040000-a)
        return bus.ewram_ptr() + (a-0x02000000);
    if (a >= 0x03000000 && a <= 0x03008000 && n <= 0x03008000-a)
        return bus.iwram_ptr() + (a-0x03000000);
    if (a >= 0x08000000 && a-0x08000000 <= bus.rom_size() && n <= bus.rom_size()-(a-0x08000000))
        return bus.rom_ptr() + (a-0x08000000);
    return nullptr;
}
std::uint16_t u16(const std::uint8_t* p) { return p[0] | (unsigned(p[1])<<8); }
std::uint32_t u32(const std::uint8_t* p) { return u16(p) | (std::uint32_t(u16(p+2))<<16); }
std::int32_t s32(const std::uint8_t* p) { return std::bit_cast<std::int32_t>(u32(p)); }
std::int32_t wrap(std::int64_t n) { return std::bit_cast<std::int32_t>(std::uint32_t(n)); }
int sign9(unsigned x) { return (x & 256) ? int(x)-512 : int(x); }
int sign8(unsigned x) { return (x & 128) ? int(x)-256 : int(x); }
bool project(const gba::GbaBus& bus, const std::uint8_t* actor, int& x, int& y, int& scale) {
    const auto* c = region(bus, camera, 0x384);
    if (!c) return false;
    const std::int64_t d[] = {std::int64_t(s32(actor))-s32(c+0x38),
        std::int64_t(s32(actor+4))-s32(c+0x3c), -std::int64_t(s32(c+0x40))};
    for (auto n : d) if (n < -0x3ffffff || n > 0x3ffffff) return false;
    int v[3]{};
    for (unsigned column=0; column<3; ++column) {
        std::int64_t sum=0;
        for (unsigned r=0; r<3; ++r) {
            if (r==2 && column==0) continue; // flat-ground projection
            sum += wrap((d[r]/128) * (s32(c+0x5c+r*12+column*4)/16)) / 32;
        }
        v[column]=wrap(sum);
    }
    const int lens=s32(c+0x10), denominator=wrap(std::int64_t(lens)-v[2]);
    if (lens<=0 || denominator<=0) return false;
    const int depth=denominator<=0x1fffff ? wrap(std::int64_t(denominator)*256)/lens
        : (lens>=256 ? denominator/(lens/256) : 0);
    if (depth<32 || depth>1024) return false;
    scale=depth*4;
    x=s32(c+0x24)+(v[0]/depth)/256;
    y=s32(c+0x28)+(v[1]/depth)/256+s32(c+0x380);
    y-=int((std::int64_t(s32(actor+8))/16384)*256/scale);
    // Prefer the guest's completed projection when it is current. Besides
    // preserving its exact row phase, this makes a native/margin crossing use
    // the very same origin. Stale/unprojected actors use the read-only camera.
    if (std::int16_t(u16(actor+0x24))==scale && std::int16_t(u16(actor+0x20))==x &&
        std::abs(std::int16_t(u16(actor+0x22))-y)<=1)
        y=std::int16_t(u16(actor+0x22));
    return y>=0 && y<=224 && scale>=128;
}
bool checkpoint_visible(const std::uint8_t* state, unsigned cp) {
    if (u16(state+0x7da)&0x800) return true;
    const unsigned first=state[0xbec], last=state[0xbed];
    return first<=last ? cp>=first && cp<=last : cp>=first || cp<=last;
}
void find_native(const gba::GbaBus& bus, Sprite& s, int anchor_x,
                 PreparedFrame& pf, MarginObjectMetrics& m) {
    if (anchor_x < -64 || anchor_x > 304) return;
    const unsigned expected0=(s.a0 & ~0x3ffu) | (unsigned(s.y)&255) |
        (s.affine ? (s.bounds_w>s.width ? 0x300 : 0x100) : 0);
    const unsigned expected1=(s.affine ? s.a1&0xc000 : s.a1&0xfe00) | (unsigned(s.x)&511);
    const unsigned mask1=s.affine ? 0xc1ff : 0xffff;
    for (unsigned i=0; i<128; ++i) {
        const auto* oam=bus.oam_ptr()+i*8;
        const unsigned a0=u16(oam), a1=u16(oam+2);
        if (a0!=expected0 || (a1&mask1)!=(expected1&mask1) || u16(oam+4)!=s.a2) continue;
        if (s.affine) {
            const auto* m_ptr=bus.oam_ptr()+((a1>>9)&31)*32;
            if (std::int16_t(u16(m_ptr+6))!=s.pa || std::int16_t(u16(m_ptr+14))!=s.pb ||
                std::int16_t(u16(m_ptr+22))!=s.pc || std::int16_t(u16(m_ptr+30))!=s.pd) continue;
        }
        s.native=true; s.order=i; ++m.native_pieces;
        if (sign9(a1&511)!=s.x) { pf.object_x[i]=s.x; ++m.wrapped_x; }
        return;
    }
}
void add_cell(const gba::GbaBus& bus, std::uint32_t address, int x, int y,
              int depth, int sx, int sy, int angle, int native_anchor,
              PreparedFrame& pf, MarginObjectMetrics& m) {
    const auto* data=region(bus,address,2);
    if (!data) return;
    const unsigned count=u16(data);
    if (!count || count>64 || !region(bus,address,2+count*6)) return;
    if (!sx || !sy) sx=sy=256;
    if (sx<1 || sy<1 || sx>32767 || sy>32767) return;
    const auto* sine=region(bus,0x080e645c,4096);
    if (!sine && angle) return;
    const int index=(angle/64)&1023;
    const int sin=sine ? s32(sine+index*4) : 0;
    const int cos=sine ? s32(sine+((256-index)&1023)*4) : 32768;
    const int flip_x=(u16(data+4)&0x1000) ? -1 : 1, flip_y=(u16(data+4)&0x2000) ? -1 : 1;
    const bool affine=!(sx==256 && sy==256 && std::int64_t(sx)*cos/32768==256 &&
                        std::int64_t(sx)*sin/32768==0);
    const bool doubled=affine && (sx<256 || sy<256);
    const int dims[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
    for (unsigned i=0; i<count && pf.sprites.size()<1024; ++i) {
        const auto* cell=data+2+i*6;
        const unsigned a0=u16(cell), a1=u16(cell+2), a2=u16(cell+4);
        const unsigned shape=a0>>14, size=a1>>14;
        if (shape>=3 || (a0&0xc00)) continue; // world OBJ-window/alpha is not an opaque billboard
        const int w=dims[shape][size][0], h=dims[shape][size][1];
        int px=x+sign9(a1&511), py=y+sign8(a0&255);
        if (affine) {
            // 0802F91C uses -(rawX ^ 511) for the scaled cell center;
            // preserve that one-unit asymmetry instead of normalizing it.
            const unsigned raw_x=a1&511;
            const int cx=(raw_x>255 ? -int(raw_x^511) : int(raw_x))+w/2;
            const int cy=sign8(a0&255)+h/2;
            px+=(cx*256)/sx-cx-(doubled ? w/2 : 0);
            py+=(cy*256)/sy-cy-(doubled ? h/2 : 0);
        }
        Sprite s{px,py,w,h,w*(doubled?2:1),h*(doubled?2:1),depth,
            std::int16_t((std::int64_t(sx)*cos/32768)*flip_x),
            std::int16_t((std::int64_t(sx)*sin/32768)*flip_x),
            std::int16_t((-std::int64_t(sy)*sin/32768)*flip_y),
            std::int16_t((std::int64_t(sy)*cos/32768)*flip_y),
            std::uint16_t(a0),std::uint16_t(a1),std::uint16_t(a2),affine};
        if (s.y>=160 || s.y+s.bounds_h<=0) continue;
        find_native(bus,s,native_anchor,pf,m);
        if (!s.native) pf.has_margin_sprites=true;
        pf.sprites.push_back(s); ++m.pieces;
    }
}
std::shared_ptr<const PreparedFrame> ensure_prepared() {
    const auto* bus=gbarecomp::active_bus();
    const unsigned long long frame=g_runtime_vblank_starts;
    const unsigned long long epoch=g_runtime_state_epoch;
    const unsigned left=g_ws_extra_left;
    const unsigned right=g_ws_extra_right;
    const std::uint64_t gen=g_reset_gen.load(std::memory_order_acquire);
    if (t_reset_gen==gen && t_prepared && t_prepared->bus==bus && t_prepared->frame==frame &&
        t_prepared->epoch==epoch && t_prepared->left==left && t_prepared->right==right) {
        return t_prepared;
    }
    std::lock_guard<std::mutex> lock(g_margin_mutex);
    if (g_prepared && g_prepared->bus==bus && g_prepared->frame==frame &&
        g_prepared->epoch==epoch && g_prepared->left==left && g_prepared->right==right) {
        t_prepared=g_prepared;
        t_reset_gen=gen;
        return t_prepared;
    }
    g_metrics.rows=g_metric_rows.exchange(0, std::memory_order_relaxed);
    g_metrics.pixels=g_metric_pixels.exchange(0, std::memory_order_relaxed);
    g_completed_metrics=sum_metrics(g_completed_metrics,g_metrics);
    g_metrics={};

    auto pf=std::make_shared<PreparedFrame>();
    pf->bus=bus; pf->frame=frame; pf->epoch=epoch; pf->left=left; pf->right=right;
    pf->object_x.fill(absent);

    if (bus && left+right!=0) {
        const auto* state=region(*bus,race,0xc20);
        if (state) {
            for (unsigned list : {0x6f4u,0x704u,0x714u}) {
                std::uint32_t address=u32(state+list);
                std::array<std::uint32_t,512> visited{}; unsigned count=0;
                while (address && count<visited.size()) {
                    if (std::find(visited.begin(),visited.begin()+count,address)!=visited.begin()+count) break;
                    visited[count++]=address;
                    const auto* actor=region(*bus,address,list==0x704 ? 0x84 : 0x5c);
                    if (!actor) break;
                    // Static scenery is all resident. Its native checkpoint slice is
                    // a draw optimization for the old field of view; use the widened
                    // geometric frustum instead. Keep dynamic actors' scene policy.
                    if ((u16(actor+0x34)&1) && (list==0x6f4 || checkpoint_visible(state,actor[0x36]))) {
                        ++g_metrics.candidates;
                        int x,y,depth;
                        if (project(*bus,actor,x,y,depth)) {
                            int sx=depth,sy=depth,angle=0;
                            std::uint32_t cell=u32(actor+(list==0x6f4 ? 0x4c : 0x48));
                            if (list==0x6f4) {
                                const auto* mip=region(*bus,cell,32);
                                if (!mip) { address=u32(actor+0x3c); continue; }
                                const unsigned level=depth<=511 ? 0 : depth<=1023 ? 1 : depth<=2047 ? 2 : 3;
                                const unsigned shift=u32(mip+level*8+4);
                                if (shift>8) { address=u32(actor+0x3c); continue; }
                                cell=u32(mip+level*8);
                                sx=(depth*std::int16_t(u16(actor+0x56))/256)>>shift;
                                sy=(depth*std::int16_t(u16(actor+0x58))/256)>>shift;
                                angle=std::int16_t(u16(actor+0x5a));
                            } else if (list==0x704) sx=sy=int(std::int64_t(depth)*s32(actor+0x80)/256);
                            else { sx=sy=depth*std::int16_t(u16(actor+0x58))/256; angle=std::int16_t(u16(actor+0x5a)); }
                            add_cell(*bus,cell,x,y,depth,sx,sy,angle,x,*pf,g_metrics);
                        }
                    }
                    address=u32(actor+0x3c);
                }
            }
            std::stable_sort(pf->sprites.begin(),pf->sprites.end(),[](const Sprite& a,const Sprite& b){return a.depth<b.depth;});
            for (auto& s:pf->sprites) if (!s.native) {
                // Preserve depth relative to native world cells already in OAM. Between
                // adjacent integer OAM keys, tie with the nearer cell (which wins).
                for (const auto& n:pf->sprites) if (n.native && n.depth>s.depth)
                    s.order=std::min(s.order,n.order ? n.order-1 : 0);
            }
        }
    }
    g_prepared=pf;
    t_prepared=pf;
    t_reset_gen=g_reset_gen.load(std::memory_order_acquire);
    return pf;
}
}
void reset_margin_objects() {
    std::lock_guard<std::mutex> lock(g_margin_mutex);
    g_reset_gen.fetch_add(1, std::memory_order_release);
    g_prepared.reset();
    t_prepared.reset();
    g_metrics={}; g_completed_metrics={};
    g_metric_rows.store(0, std::memory_order_relaxed);
    g_metric_pixels.store(0, std::memory_order_relaxed);
}
MarginObjectMetrics margin_object_metrics() {
    if (gbarecomp::active_bus()) (void)ensure_prepared();
    std::lock_guard<std::mutex> lock(g_margin_mutex);
    MarginObjectMetrics m=g_metrics;
    m.rows=g_metric_rows.load(std::memory_order_relaxed);
    m.pixels=g_metric_pixels.load(std::memory_order_relaxed);
    return m;
}
MarginObjectMetrics margin_object_totals() {
    std::lock_guard<std::mutex> lock(g_margin_mutex);
    MarginObjectMetrics m=g_metrics;
    m.rows=g_metric_rows.load(std::memory_order_relaxed);
    m.pixels=g_metric_pixels.load(std::memory_order_relaxed);
    return sum_metrics(g_completed_metrics,m);
}
int margin_object_x(int index,std::uint16_t a0,std::uint16_t a1,std::uint16_t a2,int* x) {
    if (!x || index<0 || index>=128) return 0;
    const auto pf=ensure_prepared();
    const auto* bus=gbarecomp::active_bus();
    if (!bus || !pf || pf->object_x[index]==absent) return 0;
    const auto* actual=bus->oam_ptr()+index*8;
    if (u16(actual)!=a0 || u16(actual+2)!=a1 || u16(actual+4)!=a2) return 0;
    *x=pf->object_x[index]; return 1;
}
const gba::WsMarginObjPixel* margin_objects(int y,int* left,int* width) {
    if (!left || !width || y<0 || y>=160) return nullptr;
    const auto pf=ensure_prepared();
    const auto* bus=gbarecomp::active_bus();
    if (!bus || !pf) return nullptr;
    g_metric_rows.fetch_add(1, std::memory_order_relaxed);
    *left=-int(g_ws_extra_left);
    const unsigned row_width=std::min<unsigned>(240+g_ws_extra_left+g_ws_extra_right,t_row.size());
    *width=static_cast<int>(row_width);
    std::fill_n(t_row.data(),row_width,gba::WsMarginObjPixel{});
    if (!pf->has_margin_sprites) return t_row.data();
    const bool mapping1d=(u16(bus->io().raw())&0x40)!=0;
    unsigned local_pixels=0;
    for (const auto& s:pf->sprites) {
        if (s.native || y<s.y || y>=s.y+s.bounds_h) continue;
        const int first=std::max(s.x,*left), last=std::min(s.x+s.bounds_w,*left+*width);
        for (int x=first;x<last;++x) {
            if (x>=0 && x<240) continue;
            int tx=x-s.x, ty=y-s.y;
            if (s.affine) {
                const int dx=tx-s.bounds_w/2,dy=ty-s.bounds_h/2;
                tx=((s.pa*dx+s.pb*dy)>>8)+s.width/2;
                ty=((s.pc*dx+s.pd*dy)>>8)+s.height/2;
            } else {
                if (s.a1&0x1000) tx=s.width-1-tx;
                if (s.a1&0x2000) ty=s.height-1-ty;
            }
            if (tx<0 || tx>=s.width || ty<0 || ty>=s.height) continue;
            const bool color256=(s.a0&0x2000)!=0;
            unsigned tile=s.a2&1023;
            if (color256) tile&=~1u;
            tile+=mapping1d ? ((ty/8)*(s.width/8)+tx/8)*(color256?2:1) : (ty/8)*32+(tx/8)*(color256?2:1);
            const unsigned offset=0x10000+tile*32+(ty&7)*(color256?8:4)+((tx&7)/(color256?1:2));
            if (offset>=96*1024) continue;
            const unsigned packed=bus->vram_ptr()[offset];
            unsigned index=color256 ? packed : ((tx&1) ? packed>>4 : packed&15);
            if (!index) continue;
            if (!color256) index+=((s.a2>>12)&15)*16;
            auto& p=t_row[x-*left];
            const unsigned priority=(s.a2>>10)&3;
            if ((p.color&0x8000) || priority<p.priority) {
                p.color=u16(bus->pal_ptr()+0x200+index*2)&0x7fff;
                p.priority=std::uint8_t(priority); p.order=std::uint8_t(s.order);
                ++local_pixels;
            }
        }
    }
    if (local_pixels) g_metric_pixels.fetch_add(local_pixels, std::memory_order_relaxed);
    return t_row.data();
}
}
