#include "common/itch.hpp"
#include "matching_engine/engine/engine.hpp"
#include "matching_engine/itch_parser/parser.hpp"

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <string>

namespace {

std::string default_path() {
    const char* home = std::getenv("HOME");
    std::string h = (home != nullptr) ? home : ".";
    return h + "/data/itch/itch_subset.bin";
}

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

void bm_replay_book_full_stream(benchmark::State& state) {
    const char* env_path = std::getenv("ITCH_FILE");
    std::string path = (env_path != nullptr) ? env_path : default_path();

    static const auto data = bench::common::itch::load(path);
    static const auto locates = bench::common::itch::distinct_locates(data);

    for (auto _ : state) {
        state.PauseTiming();
        Engine engine{};
        for (std::size_t locate : locates) {
            engine.inline_get_book(locate);
        }
        state.ResumeTiming();

        for (auto msg : bench::common::itch::Splitter(data)) {
            benchmark::DoNotOptimize(msg);
            ItchParser<Engine>::parse_and_dispatch(msg, engine);
        }
        benchmark::DoNotOptimize(engine);
    }
}

void bm_replay_parser_only_noop(benchmark::State& state) {
    const char* env_path = std::getenv("ITCH_FILE");
    std::string path = env_path ? env_path : default_path();

    static const auto data = bench::common::itch::load(path);

    for (auto _ : state) {
        NoopHandler handler;
        for (auto msg : bench::common::itch::Splitter(data)) {
            benchmark::DoNotOptimize(msg);
            ItchParser<NoopHandler>::parse_and_dispatch(msg, handler);
        }
        benchmark::DoNotOptimize(handler);
    }
}

BENCHMARK(bm_replay_book_full_stream)->Unit(benchmark::kMillisecond);
BENCHMARK(bm_replay_parser_only_noop)->Unit(benchmark::kMillisecond);

} // namespace
