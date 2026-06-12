#include <vector>
#include <chrono>
#include <iostream>

void measure(size_t N) {
    std::vector<int> v(N, 1);
    long sum = 0;
    auto t1 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 1000; ++rep) {
        for (size_t i = 0; i < N; ++i) sum += v[i];
    }
    auto t2 = std::chrono::steady_clock::now();
    double ns_per_elem = std::chrono::duration<double, std::nano>(t2-t1).count() / (1000.0 * N);
    std::cout << "N=" << N*4 << " bytes  " << ns_per_elem << " ns/elem  sum=" << sum << "\n";
}

int main() {
    for (size_t N = 1<<10; N <= 1<<24; N <<= 1) measure(N);
}