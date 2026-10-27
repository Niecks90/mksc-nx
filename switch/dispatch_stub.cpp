#include <cstddef>
#include <cstdint>

typedef void (*GbaFn)();

extern "C" {
    const GbaFn kDispatchTable[] = { nullptr };
    const uint32_t kDispatchTableLen = 0;
}
