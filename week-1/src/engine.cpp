#include "engine.hpp"
#include <fstream>
#include <sstream>
#include <deque>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

std::vector<csot::Tick> load_ticks(const std::string& path) {
    std::vector<csot::Tick> ticks;
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(path);
    }

    std::string line;
    std::getline(file, line);

    std::deque<std::string> symbol_storage;
    std::unordered_map<std::string, std::string_view> symbol_map;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        csot::Tick t;

        std::getline(ss, token, ',');
        t.timestamp_ns = std::stoull(token);

        std::getline(ss, token, ',');
        if (symbol_map.find(token) == symbol_map.end()) {
            symbol_storage.push_back(token);
            symbol_map[token] = symbol_storage.back();
        }
        t.symbol = symbol_map[token];

        std::getline(ss, token, ',');
        t.bid_px = std::stod(token);

        std::getline(ss, token, ',');
        t.ask_px = std::stod(token);

        std::getline(ss, token, ',');
        t.bid_qty = std::stoul(token);

        std::getline(ss, token, ',');
        t.ask_qty = std::stoul(token);

        ticks.push_back(t);
    }
    return ticks;
}