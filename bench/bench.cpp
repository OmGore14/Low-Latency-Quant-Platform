#include <benchmark/benchmark.h>
#include <vector>

static void BM_ScalarMath(benchmark::State& state) {
    alignas(64) double mids[64];
    for(int i=0; i<64; ++i) mids[i] = 100.0 + (i % 5);
    
    for (auto _ : state) {
        double sum = 0.0;
        for (int i = 0; i < 64; ++i) sum += mids[i];
        benchmark::DoNotOptimize(sum);
        
        double mean = sum * 0.015625;
        double sq = 0.0;
        for (int i = 0; i < 64; ++i) {
            double d = mids[i] - mean;
            sq += d * d;
        }
        benchmark::DoNotOptimize(sq * 0.015625);
    }
}
BENCHMARK(BM_ScalarMath);

static void BM_ILP8Math(benchmark::State& state) {
    alignas(64) double mids[64];
    for(int i=0; i<64; ++i) mids[i] = 100.0 + (i % 5);
    
    for (auto _ : state) {
        double s0=0, s1=0, s2=0, s3=0, s4=0, s5=0, s6=0, s7=0;
        for (int i = 0; i < 64; i += 8) {
            s0 += mids[i]; s1 += mids[i+1]; s2 += mids[i+2]; s3 += mids[i+3];
            s4 += mids[i+4]; s5 += mids[i+5]; s6 += mids[i+6]; s7 += mids[i+7];
        }
        double mean = (s0+s1+s2+s3+s4+s5+s6+s7) * 0.015625;
        
        double v0=0, v1=0, v2=0, v3=0, v4=0, v5=0, v6=0, v7=0;
        for (int i = 0; i < 64; i += 8) {
            double d0 = mids[i]-mean; double d1 = mids[i+1]-mean;
            double d2 = mids[i+2]-mean; double d3 = mids[i+3]-mean;
            double d4 = mids[i+4]-mean; double d5 = mids[i+5]-mean;
            double d6 = mids[i+6]-mean; double d7 = mids[i+7]-mean;
            v0 += d0*d0; v1 += d1*d1; v2 += d2*d2; v3 += d3*d3;
            v4 += d4*d4; v5 += d5*d5; v6 += d6*d6; v7 += d7*d7;
        }
        benchmark::DoNotOptimize((v0+v1+v2+v3+v4+v5+v6+v7) * 0.015625);
    }
}
BENCHMARK(BM_ILP8Math);

BENCHMARK_MAIN();