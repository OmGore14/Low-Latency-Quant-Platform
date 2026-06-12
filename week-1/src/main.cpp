#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <dlfcn.h> // Required for dlopen and dlsym
#include "engine.hpp"
#include "strategy.hpp"
#include "histogram.hpp"

// Typedef for our C-linkage factory function
typedef csot::Strategy* (*CreateStrategyFunc)();

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./quant_runner <strategy.so> <ticks.csv>\n";
        return 1;
    }

    std::string strategy_path = argv[1];
    std::string csv_path = argv[2];

    std::cout << "Loading dataset: " << csv_path << "...\n";
    auto ticks = load_ticks(csv_path);
    std::cout << "Loaded " << ticks.size() << " ticks.\n";

    std::cout << "Loading strategy plugin: " << strategy_path << "...\n";
    
    // 1. Dynamically load the .so file
    void* handle = dlopen(strategy_path.c_str(), RTLD_NOW);
    if (!handle) {
        std::cerr << "Failed to load strategy: " << dlerror() << "\n";
        return 1;
    }

    // 2. Extract the create_strategy function pointer
    dlerror(); // Clear any existing errors
    CreateStrategyFunc create_strategy = (CreateStrategyFunc)dlsym(handle, "create_strategy");
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Failed to find create_strategy symbol: " << dlsym_error << "\n";
        dlclose(handle);
        return 1;
    }

    // 3. Instantiate the strategy
    csot::Strategy* strategy = create_strategy();
    if (!strategy) {
        std::cerr << "Failed to instantiate strategy.\n";
        dlclose(handle);
        return 1;
    }

    std::cout << "Starting replay loop...\n";
    csot::LatencyHistogram hist;

    // 4. The Hot Loop
    for (const auto& tick : ticks) {
        auto t1 = std::chrono::steady_clock::now();
        
        // Execute the strategy
        strategy->on_tick(tick);
        
        auto t2 = std::chrono::steady_clock::now();
        
        // Record the delta in nanoseconds
        hist.record(std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
    }

    // 5. Print results
    std::cout << "\n--- Latency Summary ---\n";
    hist.print(std::cout);

    // Clean up memory and handles
    delete strategy;
    dlclose(handle);

    return 0;
}