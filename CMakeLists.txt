cmake_minimum_required(VERSION 3.15)
project(EfficientLimitOrderBook VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Main executable
add_executable(elob src/main.cpp)
target_compile_options(elob PRIVATE -Wall -Wextra -Wpedantic)

# Unit tests
add_executable(elob_test_cancel
    tests/unit/test_cancel_modify.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
)
target_compile_options(elob_test_cancel PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_test_edgecases
    tests/unit/test_edgecases.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
)
target_compile_options(elob_test_edgecases PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_test_ingest
    tests/unit/test_ingest.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
)
target_compile_options(elob_test_ingest PRIVATE -Wall -Wextra -Wpedantic)

# Replay tests
add_executable(elob_test_replay
    tests/replay/test_replay.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/logging/logger.cpp
)
target_compile_options(elob_test_replay PRIVATE -Wall -Wextra -Wpedantic)

# Benchmarks
add_executable(elob_benchmark
    benchmarks/test_benchmark.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/utils/metrics.cpp
)
target_compile_options(elob_benchmark PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_bench_runner
    benchmarks/bench_runner.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/utils/metrics.cpp
)
target_compile_options(elob_bench_runner PRIVATE -Wall -Wextra -Wpedantic)

# Concurrency tests
add_executable(elob_test_mt
    tests/concurrency/test_mt.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/utils/metrics.cpp
)
target_compile_options(elob_test_mt PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(elob_test_mt PRIVATE pthread)

# Lock-free concurrency test
add_executable(elob_test_lockfree
    tests/concurrency/test_lockfree.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/utils/metrics.cpp
)
target_compile_options(elob_test_lockfree PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(elob_test_lockfree PRIVATE pthread)

# Event parser test
add_executable(elob_test_parser
    tests/unit/test_parser.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
)
target_compile_options(elob_test_parser PRIVATE -Wall -Wextra -Wpedantic)

# Parser benchmark
add_executable(elob_bench_parser
    benchmarks/bench_parser.cpp
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/ingestor.cpp
    src/utils/metrics.cpp
)
target_compile_options(elob_bench_parser PRIVATE -Wall -Wextra -Wpedantic)