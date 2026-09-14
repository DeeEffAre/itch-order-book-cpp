#include "common/synthetic.hpp"
#include "matching_engine/engine/engine.hpp"
#include "matching_engine/itch_parser/parser.hpp"

#include <benchmark/benchmark.h>

namespace {

using namespace bench::common::synthetic;

constexpr Weights ZERO_WEIGHTS{.add_no_mpid = 0,
                               .add_with_mpid = 0,
                               .cancel = 0,
                               .delete_order = 0,
                               .replace = 0,
                               .execute = 0,
                               .execute_with_price = 0};

struct NoopHandler {
    void on_system_event(const SystemEventMessage*) {};                // NOLINT
    void on_add_order_no_mpid(const AddOrderNoMPID*) {};               // NOLINT
    void on_add_order_with_mpid(const AddOrderWithMPID*) {};           // NOLINT
    void on_order_cancel(const OrderCancel*) {};                       // NOLINT
    void on_order_delete(const OrderDelete*) {};                       // NOLINT
    void on_order_replace(const OrderReplace*) {};                     // NOLINT
    void on_order_execute(const OrderExecute*) {};                     // NOLINT
    void on_order_execute_with_price(const OrderExecuteWithPrice*) {}; // NOLINT
};

void run_mixed_stream_bench(benchmark::State& state, const SyntheticConfig& cfg) {
    const auto feed = generate(cfg);

    Engine template_engine{};
    for (const auto& m : feed.prefill) {
        ItchParser<Engine>::parse_and_dispatch(m, template_engine);
    }

    for (auto _ : state) {
        state.PauseTiming();
        Engine engine = template_engine; // Deep copy / clone
        state.ResumeTiming();

        for (const auto& m : feed.messages) {
            benchmark::DoNotOptimize(m.data());
            ItchParser<Engine>::parse_and_dispatch(m, engine);
        }
        benchmark::DoNotOptimize(engine);
    }
}

void bm_mixed_parse_and_dispatch(benchmark::State& state) {
    SyntheticConfig cfg;
    run_mixed_stream_bench(state, cfg);
}

void bm_parser_parse_and_dispatch_noop(benchmark::State& state) {
    SyntheticConfig cfg;
    const auto feed = generate(cfg);

    for (auto _ : state) {
        NoopHandler handler;
        for (const auto& m : feed.messages) {
            benchmark::DoNotOptimize(m.data());
            ItchParser<NoopHandler>::parse_and_dispatch(m, handler);
        }
        benchmark::DoNotOptimize(handler);
    }
}

// Single Type Benchmarks
void bm_single_type_add(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.message_count = 200'000;
    cfg.prefill_orders = 300'000;
    cfg.weights = ZERO_WEIGHTS;
    cfg.weights.add_no_mpid = 1;
    run_mixed_stream_bench(state, cfg);
}

void bm_single_type_cancel(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.message_count = 200'000;
    cfg.prefill_orders = 300'000;
    cfg.weights = ZERO_WEIGHTS;
    cfg.weights.cancel = 1;
    run_mixed_stream_bench(state, cfg);
}

void bm_single_type_delete(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.message_count = 200'000;
    cfg.prefill_orders = 300'000;
    cfg.weights = ZERO_WEIGHTS;
    cfg.weights.delete_order = 1;
    run_mixed_stream_bench(state, cfg);
}

void bm_single_type_replace(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.message_count = 200'000;
    cfg.prefill_orders = 300'000;
    cfg.weights = ZERO_WEIGHTS;
    cfg.weights.replace = 1;
    run_mixed_stream_bench(state, cfg);
}

void bm_single_type_execute(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.message_count = 200'000;
    cfg.prefill_orders = 300'000;
    cfg.weights = ZERO_WEIGHTS;
    cfg.weights.execute = 1;
    run_mixed_stream_bench(state, cfg);
}

// Depth Benchmarks
void bm_depth_shallow(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.half_band = 1'000;
    run_mixed_stream_bench(state, cfg);
}

void bm_depth_medium(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.half_band = 30'000;
    run_mixed_stream_bench(state, cfg);
}

void bm_depth_deep(benchmark::State& state) {
    SyntheticConfig cfg;
    cfg.half_band = 250'000;
    run_mixed_stream_bench(state, cfg);
}

BENCHMARK(bm_mixed_parse_and_dispatch)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_parser_parse_and_dispatch_noop)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_single_type_add)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_single_type_cancel)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_single_type_delete)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_single_type_replace)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_single_type_execute)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_depth_shallow)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_depth_medium)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_depth_deep)->Unit(benchmark::kMillisecond);

} // namespace
