#include "cache_sim.hpp"
#include <unordered_map>
#include <list>
#include <cstdint>

namespace {

// We store the full block address as the "tag" to avoid having to reconstruct 
// it during writebacks (a very common source of bugs).
struct Line {
    std::uint64_t block_addr;
    bool dirty;
};

class NaiveCacheSim final : public csot::CacheSim {
    // Maps a Set Index -> A linked list of cache lines (front = MRU, back = LRU)
    std::unordered_map<std::uint64_t, std::list<Line>> l1_;
    std::unordered_map<std::uint64_t, std::list<Line>> l2_;

public:
    void on_init() override {
        l1_.clear();
        l2_.clear();
    }

    csot::CacheStats run(const csot::MemAccess* acc, std::size_t n) override {
        csot::CacheStats st{};

        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t b = acc[i].address >> 6; // Block address
            const bool wr = acc[i].is_write != 0;

            // 5.1 — Count the access
            if (wr) st.writes++;
            else    st.reads++;

            const std::uint64_t s1 = b & 63; // L1 has 64 sets
            auto& l1_ways = l1_[s1];

            // 5.2 — Probe L1
            auto it1 = l1_ways.begin();
            while (it1 != l1_ways.end() && it1->block_addr != b) ++it1;

            if (it1 != l1_ways.end()) {
                // ---- L1 Hit ----
                st.l1_hits++;
                if (wr) it1->dirty = true;
                // Move to MRU (front of the list)
                l1_ways.splice(l1_ways.begin(), l1_ways, it1);
                continue; // Done with this access
            }

            // ---- L1 Miss ----
            st.l1_misses++;

            // 5.3 — Probe L2
            const std::uint64_t s2 = b & 511; // L2 has 512 sets
            auto& l2_ways = l2_[s2];
            
            auto it2 = l2_ways.begin();
            while (it2 != l2_ways.end() && it2->block_addr != b) ++it2;

            if (it2 != l2_ways.end()) {
                // L2 Hit
                st.l2_hits++;
                l2_ways.splice(l2_ways.begin(), l2_ways, it2); // Move to MRU
            } else {
                // L2 Miss: Demand-install CLEAN
                st.l2_misses++;
                if (l2_ways.size() == 8) { // L2 is full, evict LRU
                    if (l2_ways.back().dirty) st.dirty_writebacks++;
                    l2_ways.pop_back();
                }
                l2_ways.push_front({b, false}); // Install clean line at MRU
            }

            // 5.4 — Fill into L1 (Write-allocate)
            if (l1_ways.size() == 8) { // L1 is full, evict LRU
                Line victim = l1_ways.back();
                l1_ways.pop_back();

                // 5.5 — Writing a dirty L1 victim back to L2
                if (victim.dirty) {
                    const std::uint64_t s2v = victim.block_addr & 511;
                    auto& l2v_ways = l2_[s2v];
                    
                    auto it2v = l2v_ways.begin();
                    while (it2v != l2v_ways.end() && it2v->block_addr != victim.block_addr) ++it2v;

                    if (it2v != l2v_ways.end()) {
                        // Found in L2: mark dirty. DO NOT TOUCH LRU OR COUNTERS!
                        it2v->dirty = true;
                    } else {
                        // Not in L2: Install dirty
                        if (l2v_ways.size() == 8) {
                            if (l2v_ways.back().dirty) st.dirty_writebacks++;
                            l2v_ways.pop_back();
                        }
                        l2v_ways.push_front({victim.block_addr, true});
                    }
                }
            }
            // Place new line into L1 at MRU. Dirty if this was a write.
            l1_ways.push_front({b, wr});
        }

        return st;
    }
};

}  // namespace

extern "C" csot::CacheSim* create_cache_sim() {
    return new NaiveCacheSim();
}