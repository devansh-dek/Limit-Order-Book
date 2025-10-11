cmake_minimum_required(VERSION 3.15)
project(EfficientLimitOrderBook VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(elob
	src/main.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
)

target_compile_options(elob PRIVATE -Wall -Wextra -Wpedantic)

target_sources(elob PRIVATE
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
)

# Include directory for headers
target_include_directories(elob PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Unit tests target
add_executable(elob_test_cancel
	tests/test_cancel_modify.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
)
target_include_directories(elob_test_cancel PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_test_cancel PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_test_edgecases
	tests/test_edgecases.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
)
target_include_directories(elob_test_edgecases PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_test_edgecases PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_test_ingest
	tests/test_ingest.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
)
target_include_directories(elob_test_ingest PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_test_ingest PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_test_replay
	tests/test_replay.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
	src/logger.cpp
)
target_include_directories(elob_test_replay PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_test_replay PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_benchmark
	tests/test_benchmark.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
	src/metrics.cpp
)
target_include_directories(elob_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_benchmark PRIVATE -Wall -Wextra -Wpedantic)

add_executable(elob_bench_runner
	tests/bench_runner.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
	src/metrics.cpp
)
target_include_directories(elob_bench_runner PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_bench_runner PRIVATE -Wall -Wextra -Wpedantic)
add_executable(elob_test_mt
    tests/test_mt.cpp
    src/order.cpp
    src/trade.cpp
    src/event.cpp
    src/price_level.cpp
    src/order_book.cpp
    src/matching_engine.cpp
    src/ingestor.cpp
    src/metrics.cpp
)
target_include_directories(elob_test_mt PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_test_mt PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(elob_test_mt PRIVATE pthread)