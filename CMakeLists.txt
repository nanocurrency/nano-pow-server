cmake_minimum_required (VERSION 3.4)

if (CMAKE_VERSION VERSION_GREATER 3.12 OR CMAKE_VERSION VERSION_EQUAL 3.12)
	#find_package uses <PACKAGENAME>_ROOT variables
	cmake_policy(SET CMP0074 NEW)
endif()
if (CMAKE_VERSION VERSION_GREATER 3.13 OR CMAKE_VERSION VERSION_EQUAL 3.13)
	#option honors normal variables
	cmake_policy(SET CMP0077 NEW)
endif()

project (nano_pow_server)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set (NANO_POW_SERVER_TEST OFF CACHE BOOL "")
set (NANO_POW_STANDALONE OFF CACHE BOOL "")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (WIN32)
	add_definitions(-D_WIN32_WINNT=0x0600
					-DWINVER=0x0600
	)
	add_definitions (-DBOOST_ALL_NO_LIB)
	add_definitions(/bigobj)
endif ()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#if (APPLE)
#	set (PLATFORM_LINK_FLAGS "-framework OpenCL")
#	SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINK_FLAGS}" )
#else ()
#	if (WIN32)
#		set (PLATFORM_LINK_FLAGS "")
#		add_definitions (-DBOOST_ALL_NO_LIB)
#		add_definitions(/bigobj)
#	else ()
#		set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
#	endif ()
#	find_package (OpenCL REQUIRED)
#	include_directories (${OpenCL_INCLUDE_DIRS})
#	link_directories (${OpenCL_LIBRARY})
#endif ()

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

find_package (Boost 1.69.0 REQUIRED COMPONENTS filesystem thread program_options system)
include_directories (src)

add_definitions(-DFMT_HEADER_ONLY)

include_directories(deps/cpptoml/include)
include_directories(deps/spdlog/include)

add_library (nano_pow_server_library
	src/workserver/config.hpp
	src/workserver/webserver.hpp
	src/workserver/work_handler.hpp
	src/workserver/work_handler.cpp
	src/workserver/util.hpp
	)
target_link_libraries (nano_pow_server_library
	PUBLIC
		Boost::filesystem
		Boost::program_options
		Boost::boost
		Threads::Threads
		# Currently not used
		#${OpenCL_LIBRARY}
		)

add_executable (nano_pow_server
	src/entry/nano_pow_server.cpp)
target_link_libraries (nano_pow_server
	nano_pow_server_library)
add_custom_command(TARGET nano_pow_server
				   POST_BUILD
				   COMMAND nano_pow_server --generate_config > ${PROJECT_BINARY_DIR}/config-nano-pow-server.toml.sample)
install (FILES ${PROJECT_BINARY_DIR}/config-nano-pow-server.toml.sample DESTINATION .) # if you are installing you will want the sample either way.
if (NANO_POW_STANDALONE)
	install (TARGETS nano_pow_server
			 RUNTIME DESTINATION .
	)
	install (DIRECTORY ${PROJECT_SOURCE_DIR}/public DESTINATION .)
	set (CPACK_GENERATOR "TGZ")
	include (CPack)
endif ()

if (${NANO_POW_SERVER_TEST} AND ${NANO_POW_STANDALONE})
	if (WIN32)
		set(gtest_force_shared_crt ON)
	endif()
	add_subdirectory (deps/googletest/googletest)
	include_directories (deps/googletest/googletest/include)
	add_executable (tests
		src/entry/gtest.cpp
		src/tests/work.cpp)
	target_link_libraries (tests
		nano_pow_server_library
		gtest)
endif ()
