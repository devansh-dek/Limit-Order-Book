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
)
