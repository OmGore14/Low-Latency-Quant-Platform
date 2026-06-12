#include "cache_sim.hpp"
#include <array>
#include <cstdint>

namespace {

template <std::uint64_t SETS, std::uint64_t WAYS>
struct Level {
    static constexpr std::uint64_t INDEX_MASK = SETS - 1;
    
    alignas(64) std::array<std::uint64_t, SETS * WAYS> tag{};
    std::array<std::uint8_t, SETS * WAYS> valid{};
    std::array<std::uint8_t, SETS * WAYS> dirty{};
    std::array<std::uint64_t, SETS * WAYS> lru{};

    inline int find(std::uint64_t set, std::uint64_t t) const {
        unsigned hit = 0;
        std::uint64_t base = set * WAYS;
        for (std::uint64_t w = 0; w < WAYS; ++w) {
            hit |= unsigned((tag[base + w] == t) & (valid[base + w] != 0)) << w;
        }
        return hit ? __builtin_ctz(hit) : -1;
    }

    inline int victim(std::uint64_t set) const {
        std::uint64_t base = set * WAYS;
        for (std::uint64_t w = 0; w < WAYS; ++w) {
            if (!valid[base + w]) return int(w);
        }
        
        int v = 0;
        std::uint64_t min_lru = lru[base];
        for (std::uint64_t w = 1; w < WAYS; ++w) {
            if (lru[base + w] < min_lru) {
                min_lru = lru[base + w];
                v = int(w);
            }
        }
        return v;
    }
};

class FastCacheSim final : public csot::CacheSim {
    Level<64, 8> l1_;
    Level<512, 8> l2_;

public:
    void on_init() override {}

    csot::CacheStats run(const csot::MemAccess* __restrict__ acc, std::size_t n) override {
        csot::CacheStats st{};
        std::uint64_t timer = 0;
        constexpr std::size_t PF = 8;

        for (std::size_t i = 0; i < n; ++i) {
            if (i + PF < n) {
                const std::uint64_t nb = acc[i + PF].address >> 6;
                __builtin_prefetch(&l1_.tag[(nb & decltype(l1_)::INDEX_MASK) * 8], 0, 3);
            }

            const std::uint64_t b = acc[i].address >> 6;
            const bool wr = acc[i].is_write != 0;
            
            if (wr) st.writes++; else st.reads++;
            timer++; 

            const std::uint64_t s1 = b & decltype(l1_)::INDEX_MASK;
            int w1 = l1_.find(s1, b);

            if (w1 >= 0) [[likely]] {
                st.l1_hits++;
                l1_.lru[s1 * 8 + w1] = timer;
                if (wr) l1_.dirty[s1 * 8 + w1] = 1;
                continue;
            }

            st.l1_misses++;

            const std::uint64_t s2 = b & decltype(l2_)::INDEX_MASK;
            int w2 = l2_.find(s2, b);

            if (w2 >= 0) {
                st.l2_hits++;
                l2_.lru[s2 * 8 + w2] = timer;
            } else {
                st.l2_misses++;
                int v2 = l2_.victim(s2);
                std::uint64_t idx2 = s2 * 8 + v2;
                
                if (l2_.valid[idx2] && l2_.dirty[idx2]) st.dirty_writebacks++;
                
                l2_.valid[idx2] = 1;
                l2_.dirty[idx2] = 0;
                l2_.tag[idx2] = b;
                l2_.lru[idx2] = timer;
            }

            int v1 = l1_.victim(s1);
            std::uint64_t idx1 = s1 * 8 + v1;
            
            if (l1_.valid[idx1] && l1_.dirty[idx1]) {
                std::uint64_t bv = l1_.tag[idx1];
                std::uint64_t s2v = bv & decltype(l2_)::INDEX_MASK;
                int wv = l2_.find(s2v, bv);

                if (wv >= 0) {
                    l2_.dirty[s2v * 8 + wv] = 1;
                } else {
                    int vv = l2_.victim(s2v);
                    std::uint64_t idx2v = s2v * 8 + vv;
                    if (l2_.valid[idx2v] && l2_.dirty[idx2v]) st.dirty_writebacks++;
                    
                    l2_.valid[idx2v] = 1;
                    l2_.dirty[idx2v] = 1;
                    l2_.tag[idx2v] = bv;
                    l2_.lru[idx2v] = timer;
                }
            }
            
            l1_.valid[idx1] = 1;
            l1_.dirty[idx1] = wr ? 1 : 0;
            l1_.tag[idx1] = b;
            l1_.lru[idx1] = timer;
        }
        return st;
    }
};

}

extern "C" csot::CacheSim* create_cache_sim() {
    return new FastCacheSim();
}