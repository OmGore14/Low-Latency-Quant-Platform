#include <vector>
#include <list>
#include <chrono>
#include <numeric>
#include <iostream>

int main() {
    constexpr size_t N = 10'000'000;

    std::vector<int> v(N, 1);
    std::list<int>   l(N, 1);

    auto t1 = std::chrono::steady_clock::now();
    long sv = std::accumulate(v.begin(), v.end(), 0L);
    auto t2 = std::chrono::steady_clock::now();
    long sl = std::accumulate(l.begin(), l.end(), 0L);
    auto t3 = std::chrono::steady_clock::now();

    std::cout << "vector sum=" << sv << " took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << " ms\n";
    std::cout << "list   sum=" << sl << " took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count() << " ms\n";
}