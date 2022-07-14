cmake_minimum_required(VERSION 3.23.2)
project(woof VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################

add_library(woof
	src/asio_impl.cpp
	src/case_insensitive.cpp
	src/default_log.cpp
	src/parsed_target.cpp
	src/path_pattern.cpp
	src/request.cpp
	src/response.cpp
	src/server.cpp
	src/server_run.cpp
)

target_compile_definitions(woof PRIVATE
	BOOST_ASIO_SEPARATE_COMPILATION
	BOOST_BEAST_USE_STD_STRING_VIEW
)

target_include_directories(woof PUBLIC include)

target_include_directories(woof PRIVATE deps/boost/asio/include)
target_include_directories(woof PRIVATE deps/boost/beast/include)

find_package(Threads REQUIRED)
target_link_libraries(woof PUBLIC Threads::Threads)

################################################################################

add_executable(example_hello example/hello.cpp)
target_link_libraries(example_hello PRIVATE woof)
