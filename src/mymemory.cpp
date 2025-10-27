//
// Created by 86150 on 2025/9/20.
//
#include "mymemory.h"
#include "Common.h"       // ALIGNMENT, MAX_BYTES, SizeClass
#include <new>
#include <cstring>
#include <cstdint>

static inline std::uintptr_t align_up(std::uintptr_t p, std::size_t A) {
    return (p + (A - 1)) & ~(static_cast<std::uintptr_t>(A) - 1);
}

void* CMemory::AllocMemory(int memCount, bool ifmemset) {
    if (memCount < 0) return nullptr;

    const std::size_t u = static_cast<std::size_t>(memCount);
    constexpr std::size_t A = 16; // 我们的头部对齐

    // 若底层粒度(ALIGNMENT) < 16，就需要额外预留 A-1 字节以便把 hdr “挪”到 16 对齐位置；
    // 若 ALIGNMENT >= 16（例如你把 Common.h 调成 16），就不需要这笔余量（更省）。
    constexpr std::size_t EXTRA = (ALIGNMENT >= A) ? 0 : (A - 1);

    // 申请的“名义大小”（交给内存池归类）
    const std::size_t need = sizeof(MpHeader) + u + EXTRA;

    // 让 ThreadCache 以 need 归类（内部会用 SizeClass::getIndex / roundUp）：
    // > MAX_BYTES 时它会直接 malloc；否则按 size-class 从 CentralCache 取块。:contentReference[oaicite:1]{index=1}
    char* base = static_cast<char*>(MemoryPool::allocate(need));  // :contentReference[oaicite:2]{index=2}
    if (!base) throw std::bad_alloc{};

    // 把 header 放到 16 对齐的地址上（数学保证：预留 EXTRA 后 q 一定在块内）
    const std::uintptr_t p = reinterpret_cast<std::uintptr_t>(base);
    const std::uintptr_t q = align_up(p, A);
    auto* hdr = reinterpret_cast<MpHeader*>(q);

    hdr->back      = static_cast<std::uint8_t>(q - p); // < A(=16)，1B 足够
    hdr->user_size = u;
    hdr->flags     = 0;

#ifdef MP_DEBUG_GUARD
    hdr->magic     = kMpMagic;
#endif

    // 记录小对象的 size-class（释放 O(1)）；大对象设置 LARGE 标志走直通路径
    if (need <= MAX_BYTES) {
        const std::size_t idx = SizeClass::getIndex(need);       // :contentReference[oaicite:3]{index=3}
        hdr->size_class = static_cast<std::uint16_t>(idx);
    } else {
        hdr->size_class = 0;
        hdr->flags |= 0x1; // LARGE
    }

    // 返回用户指针；因为 sizeof(MpHeader)=16，(hdr+1) 天然 16 对齐
    void* user = static_cast<void*>(hdr + 1);
    if (ifmemset) std::memset(user, 0, u);
    return user;
}

void CMemory::FreeMemory(void* point) {
    if (!point) return;

    auto* hdr  = reinterpret_cast<MpHeader*>(point) - 1;

#ifdef MP_DEBUG_GUARD
    if (hdr->magic != kMpMagic) {
        // 调试期早发现误释放/越界
        return;
    }
#endif

    // 还原底层基址（把 hdr 从 16 对齐位置“退回”到池子块起点）
    char* base = reinterpret_cast<char*>(hdr) - hdr->back;

    if ((hdr->flags & 0x1) == 0) {
        // 小对象：直接按 class O(1) 归还（避免重复映射）
        const std::size_t rounded = (static_cast<std::size_t>(hdr->size_class) + 1) * ALIGNMENT; // :contentReference[oaicite:4]{index=4}
        MemoryPool::deallocate(base, rounded);                                                   // :contentReference[oaicite:5]{index=5}
    } else {
        // 大对象直通：把当时的“名义大小”再传回（>MAX_BYTES 时 ThreadCache 会直接 free）:contentReference[oaicite:6]{index=6}
        constexpr std::size_t A = 16;
        constexpr std::size_t EXTRA = (ALIGNMENT >= A) ? 0 : (A - 1);
        const std::size_t need = sizeof(MpHeader) + hdr->user_size + EXTRA;
        MemoryPool::deallocate(base, need);
    }
}
