#pragma once
#include <cstdint>
#include <stdexcept>
namespace integration {
inline void requireSlotRetired(bool leased,bool submitted,bool unsafe) {
    if(leased||submitted||unsafe)throw std::runtime_error("Slot/command reuse before retirement");
}
inline void requireD3DDone(uint64_t observed,uint64_t done) {
    if(!done||observed==UINT64_MAX||observed<done)throw std::runtime_error("Slot reuse before D3D12 done/device removed");
}
}
