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
add_executable(elob_tests
	tests/test_cancel_modify.cpp
	src/order.cpp
	src/trade.cpp
	src/event.cpp
	src/price_level.cpp
	src/order_book.cpp
	src/matching_engine.cpp
	src/ingestor.cpp
)
target_include_directories(elob_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(elob_tests PRIVATE -Wall -Wextra -Wpedantic)
