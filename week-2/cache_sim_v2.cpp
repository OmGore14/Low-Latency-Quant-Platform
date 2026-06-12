#include "cache_sim.hpp"
#include <vector>
#include <cstdint>

namespace {

struct Level {
    std::uint64_t sets;
    std::uint64_t ways;
    
    // Flat Struct-of-Arrays layout. No objects, no pointers.
    std::vector<std::uint64_t> tag;
    std::vector<std::uint8_t> valid;
    std::vector<std::uint8_t> dirty;
    std::vector<std::uint64_t> lru; // Timestamp for LRU

    void init(std::uint64_t s, std::uint64_t w) {
        sets = s;
        ways = w;
        // Allocate ONCE in the cold path. (Zero allocation in hot path)
        tag.assign(s * w, 0);
        valid.assign(s * w, 0);
        dirty.assign(s * w, 0);
        lru.assign(s * w, 0);
    }

    // Scan contiguous memory for a hit
    int find(std::uint64_t set, std::uint64_t t) const {
        std::uint64_t base = set * ways;
        for (std::uint64_t w = 0; w < ways; ++w) {
            if (valid[base + w] && tag[base + w] == t) return w;
        }
        return -1;
    }

    // Find first invalid way, or the one with the lowest timestamp
    int victim(std::uint64_t set) const {
        std::uint64_t base = set * ways;
        for (std::uint64_t w = 0; w < ways; ++w) {
            if (!valid[base + w]) return w;
        }
        
        int v = 0;
        std::uint64_t min_lru = lru[base];
        for (std::uint64_t w = 1; w < ways; ++w) {
            if (lru[base + w] < min_lru) {
                min_lru = lru[base + w];
                v = w;
            }
        }
        return v;
    }
};

class FlatCacheSim final : public csot::CacheSim {
    Level l1_;
    Level l2_;

public:
    void on_init() override {
        l1_.init(64, 8);
        l2_.init(512, 8);
    }

    csot::CacheStats run(const csot::MemAccess* acc, std::size_t n) override {
        csot::CacheStats st{};
        std::uint64_t timer = 0; // Simple integer counter for LRU

        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t b = acc[i].address >> 6;
            const bool wr = acc[i].is_write != 0;
            
            if (wr) st.writes++; else st.reads++;
            timer++; 

            const std::uint64_t s1 = b & 63;
            // Trick: We store the full block address 'b' as the tag to avoid shifting later!
            int w1 = l1_.find(s1, b);

            if (w1 >= 0) {
                st.l1_hits++;
                l1_.lru[s1 * 8 + w1] = timer; // Mark MRU
                if (wr) l1_.dirty[s1 * 8 + w1] = 1;
                continue;
            }

            st.l1_misses++;

            const std::uint64_t s2 = b & 511;
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
                l2_.dirty[idx2] = 0; // Clean demand install
                l2_.tag[idx2] = b;
                l2_.lru[idx2] = timer;
            }

            int v1 = l1_.victim(s1);
            std::uint64_t idx1 = s1 * 8 + v1;
            
            if (l1_.valid[idx1] && l1_.dirty[idx1]) {
                // Writeback victim to L2
                std::uint64_t bv = l1_.tag[idx1]; // No reconstruction needed!
                std::uint64_t s2v = bv & 511;
                int wv = l2_.find(s2v, bv);

                if (wv >= 0) {
                    l2_.dirty[s2v * 8 + wv] = 1; // DO NOT TOUCH LRU
                } else {
                    int vv = l2_.victim(s2v);
                    std::uint64_t idx2v = s2v * 8 + vv;
                    if (l2_.valid[idx2v] && l2_.dirty[idx2v]) st.dirty_writebacks++;
                    
                    l2_.valid[idx2v] = 1;
                    l2_.dirty[idx2v] = 1; // Install dirty
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

}  // namespace

extern "C" csot::CacheSim* create_cache_sim() {
    return new FlatCacheSim();
}