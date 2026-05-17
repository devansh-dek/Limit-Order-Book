cmake_minimum_required(VERSION 3.15)
project(EfficientLimitOrderBook VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/src)

# Core engine sources shared by every target
set(ELOB_CORE
    src/engine/order.cpp
    src/engine/trade.cpp
    src/data/event.cpp
    src/engine/price_level.cpp
    src/engine/order_pool.cpp
    src/engine/price_ladder.cpp
    src/engine/order_book.cpp
    src/engine/matching_engine.cpp
    src/data/validator.cpp
    src/data/ingestor.cpp
    src/marketdata/l2_publisher.cpp
)

# Main executable
add_executable(elob src/main.cpp)
target_compile_options(elob PRIVATE -Wall -Wextra -Wpedantic)

# Unit tests
add_executable(elob_test_cancel        tests/unit/test_cancel_modify.cpp  ${ELOB_CORE})
add_executable(elob_test_edgecases     tests/unit/test_edgecases.cpp      ${ELOB_CORE})
add_executable(elob_test_ingest        tests/unit/test_ingest.cpp         ${ELOB_CORE})
add_executable(elob_test_parser        tests/unit/test_parser.cpp         ${ELOB_CORE})
add_executable(elob_test_market_orders tests/unit/test_market_orders.cpp  ${ELOB_CORE})
add_executable(elob_test_stp           tests/unit/test_stp.cpp            ${ELOB_CORE})
add_executable(elob_test_iceberg       tests/unit/test_iceberg.cpp        ${ELOB_CORE})
add_executable(elob_test_l2            tests/unit/test_l2.cpp             ${ELOB_CORE})
add_executable(elob_test_wal           tests/unit/test_wal.cpp            ${ELOB_CORE} src/logging/wal_logger.cpp)
add_executable(elob_test_histogram     tests/unit/test_histogram.cpp      src/utils/latency_histogram.cpp)
add_executable(elob_test_validator     tests/unit/test_validator.cpp      ${ELOB_CORE})

foreach(t elob_test_cancel elob_test_edgecases elob_test_ingest elob_test_parser elob_test_market_orders elob_test_stp elob_test_iceberg elob_test_l2 elob_test_wal elob_test_histogram elob_test_validator)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

# Replay test
add_executable(elob_test_replay
    tests/replay/test_replay.cpp
    ${ELOB_CORE}
    src/logging/logger.cpp
)
target_compile_options(elob_test_replay PRIVATE -Wall -Wextra -Wpedantic)

# Benchmarks
add_executable(elob_benchmark    benchmarks/test_benchmark.cpp ${ELOB_CORE} src/utils/metrics.cpp)
add_executable(elob_bench_runner benchmarks/bench_runner.cpp   ${ELOB_CORE} src/utils/metrics.cpp src/utils/latency_histogram.cpp)
add_executable(elob_bench_parser benchmarks/bench_parser.cpp   ${ELOB_CORE} src/utils/metrics.cpp)

foreach(t elob_benchmark elob_bench_runner elob_bench_parser)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

# Concurrency tests
add_executable(elob_test_mt
    tests/concurrency/test_mt.cpp
    ${ELOB_CORE}
    src/utils/metrics.cpp
)
target_compile_options(elob_test_mt PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(elob_test_mt PRIVATE pthread)

add_executable(elob_test_concurrency
    tests/concurrency/test_lockfree.cpp
    ${ELOB_CORE}
    src/utils/metrics.cpp
)
target_compile_options(elob_test_concurrency PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(elob_test_concurrency PRIVATE pthread)
