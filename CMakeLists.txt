cmake_minimum_required (VERSION 3.4)
project (nano_pow_server)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set (NANO_POW_SERVER_TEST ON CACHE BOOL "")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (APPLE)
	set (PLATFORM_LINK_FLAGS "-framework OpenCL")
	SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINK_FLAGS}" )
else ()
	if (WIN32)
		set (PLATFORM_LINK_FLAGS "")
		add_definitions (-DBOOST_ALL_NO_LIB)
		add_definitions(/bigobj)
	else ()
		set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif ()
	find_package (OpenCL REQUIRED)
	include_directories (${OpenCL_INCLUDE_DIRS})
	link_directories (${OpenCL_LIBRARY})
endif ()

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

find_package (Boost 1.67.0 REQUIRED COMPONENTS filesystem thread program_options system)
include_directories (${CMAKE_SOURCE_DIR}/src)

add_definitions(-DFMT_HEADER_ONLY)

include_directories(deps/cpptoml/include)
include_directories(deps/spdlog/include)
include_directories (deps/nano-pow/include)
add_subdirectory (deps/nano-pow)

add_library (nano_pow_server_library
	src/workserver/config.hpp
	src/workserver/webserver.hpp
	src/workserver/work_handler.hpp
	src/workserver/work_handler.cpp
	src/workserver/util.hpp
	)
target_link_libraries (nano_pow_server_library
	PUBLIC
		nano_pow
		Boost::filesystem
		Boost::program_options
		Boost::boost
		Threads::Threads
		${OpenCL_LIBRARY})

add_executable (nano_pow_server
	src/entry/nano_pow_server.cpp)
target_link_libraries (nano_pow_server
	nano_pow_server_library)

if (${NANO_POW_SERVER_TEST})
	add_subdirectory (deps/nano-pow/googletest/googletest)
	include_directories (deps/nano-pow/googletest/googletest/include)

	add_executable (tests
		src/entry/gtest.cpp
		src/tests/work.cpp)
	target_link_libraries (tests
		nano_pow_server_library
		gtest)
endif ()
