#include "strategy.hpp"
#include <vector>
#include <cmath>
#include <array>
#include <string_view>

class SpecStrategy : public csot::Strategy {
    std::array<const char*, 64> symbol_keys{};
    
    alignas(64) std::array<double, 4096> all_mids{};
    alignas(64) std::array<double, 64> all_sums{};
    alignas(64) std::array<double, 64> all_sum_sqs{};
    std::array<uint32_t, 64> all_counts{};
    std::array<uint32_t, 64> all_heads{};
    std::array<int32_t, 64> all_positions{};
    uint32_t num_symbols = 0;

    __attribute__((always_inline)) inline uint32_t get_state_idx(const char* sym_ptr) {
        for (uint32_t i = 0; i < num_symbols; ++i) {
            if (symbol_keys[i] == sym_ptr) {
                return i;
            }
        }
        symbol_keys[num_symbols] = sym_ptr;
        return num_symbols++;
    }

public:
    std::vector<csot::Order> on_tick(const csot::Tick& t) override {
        const uint32_t idx = get_state_idx(t.symbol.data());
        const double mid = (t.bid_px + t.ask_px) * 0.5;

        uint32_t head = all_heads[idx];
        uint32_t count = all_counts[idx];
        int32_t position = all_positions[idx];
        double* __restrict mids = &all_mids[idx << 6];

        double old_mid = mids[head];
        mids[head] = mid;
        all_heads[idx] = (head + 1) & 63;
        
        double sum = all_sums[idx];
        double sum_sq = all_sum_sqs[idx];

        if (__builtin_expect(count < 64, 0)) {
            count++;
            all_counts[idx] = count;
            sum += mid;
            sum_sq += mid * mid;
            all_sums[idx] = sum;
            all_sum_sqs[idx] = sum_sq;
            if (count < 64) {
                return {};
            }
        } else {
            sum += mid - old_mid;
            sum_sq += (mid * mid) - (old_mid * old_mid);
            
            if (__builtin_expect(head == 0, 0)) {
                double seq_sum = 0.0, seq_sq = 0.0;
                #pragma GCC unroll 8
                for (int i = 0; i < 64; ++i) {
                    seq_sum += mids[i];
                    seq_sq += mids[i] * mids[i];
                }
                sum = seq_sum;
                sum_sq = seq_sq;
            }
            
            all_sums[idx] = sum;
            all_sum_sqs[idx] = sum_sq;
        }

        const double mean = sum * 0.015625;
        const double variance = sum_sq * 0.015625 - (mean * mean);
        
        if (__builtin_expect(variance < 1e-18, 0)) return {};
        
        const double diff = mid - mean;
        const double diff_sq = diff * diff;
        
        const double margin = 1e-7 * variance; 
        const double thresh_entry = 4.0 * variance;
        const double thresh_exit  = 0.25 * variance;

        bool near_entry = std::abs(diff_sq - thresh_entry) <= margin;
        bool near_exit  = std::abs(diff_sq - thresh_exit) <= margin;

        if (__builtin_expect(near_entry || near_exit, 0)) {
            double seq_sum = 0.0;
            for (int i = 0; i < 64; ++i) seq_sum += mids[i];
            double seq_mean = seq_sum * 0.015625;
            
            double seq_sq = 0.0;
            for (int i = 0; i < 64; ++i) {
                double d = mids[i] - seq_mean;
                seq_sq += d * d;
            }
            double seq_var = seq_sq * 0.015625;
            double seq_std = std::sqrt(seq_var);
            double seq_z = (mid - seq_mean) / seq_std;
            double seq_abs_z = std::abs(seq_z);
            
            if (position == 0) {
                if (seq_z >= 2.0) return {csot::Order{csot::Order::Side::SELL, t.symbol, t.bid_px, 1}};
                if (seq_z <= -2.0) return {csot::Order{csot::Order::Side::BUY, t.symbol, t.ask_px, 1}};
                return {};
            }
            if (position > 0 && seq_abs_z <= 0.5) {
                return {csot::Order{csot::Order::Side::SELL, t.symbol, t.bid_px, static_cast<uint32_t>(position)}};
            }
            if (position < 0 && seq_abs_z <= 0.5) {
                return {csot::Order{csot::Order::Side::BUY, t.symbol, t.ask_px, static_cast<uint32_t>(-position)}};
            }
            return {};
        }

        if (__builtin_expect(position == 0, 1)) {
            if (diff_sq >= thresh_entry) {
                if (diff >= 0.0) return {csot::Order{csot::Order::Side::SELL, t.symbol, t.bid_px, 1}};
                if (diff <= 0.0) return {csot::Order{csot::Order::Side::BUY, t.symbol, t.ask_px, 1}};
            }
            return {};
        }

        if (position > 0 && diff_sq <= thresh_exit) {
            return {csot::Order{csot::Order::Side::SELL, t.symbol, t.bid_px, static_cast<uint32_t>(position)}};
        }
        if (position < 0 && diff_sq <= thresh_exit) {
            return {csot::Order{csot::Order::Side::BUY, t.symbol, t.ask_px, static_cast<uint32_t>(-position)}};
        }

        return {};
    }

    void on_fill(const csot::Order& order, double, uint32_t) override {
        const uint32_t idx = get_state_idx(order.symbol.data());
        if (order.side == csot::Order::Side::BUY) {
            all_positions[idx] += order.qty;
        } else {
            all_positions[idx] -= order.qty;
        }
    }
};

extern "C" csot::Strategy* create_strategy() {
    return new SpecStrategy();
}