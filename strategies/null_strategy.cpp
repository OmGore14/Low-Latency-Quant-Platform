#include "strategy.hpp"
#include <vector>

class NullStrategy : public csot::Strategy {
public:
    std::vector<csot::Order> on_tick(const csot::Tick& /* tick */) override {
        // Do absolutely nothing. This is for testing absolute minimum overhead.
        return {};
    }
};

// The C-linkage factory that allows the engine (and the live judge) to load this dynamically
extern "C" csot::Strategy* create_strategy() {
    return new NullStrategy();
}