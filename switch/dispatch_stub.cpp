#include <cstddef>
#include <cstdint>

typedef void (*GbaFn)();

extern "C" {
    const GbaFn kDispatchTable[] = { nullptr };
    const size_t kDispatchTableLen = 0;
}
