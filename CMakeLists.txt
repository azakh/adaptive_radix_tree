cmake_minimum_required(VERSION 3.6)
project( adaptive_radix_tree-test )

enable_testing()

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(GTEST_DIR "external/googletest"
	CACHE PATH "The path to the GoogleTest test framework.")

if (MSVC)
	# force this option to ON so that Google Test will use /MD instead of /MT
	# /MD is now the default for Visual Studio, so it should be our default, too
	option(gtest_force_shared_crt
		"Use shared (DLL) run-time lib even when Google Test is built as static lib."
		ON)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
elseif (APPLE)
	add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)
endif()


include_directories(SYSTEM
	include
	${GTEST_DIR}/googletest/include
	${GTEST_DIR}/include
)

add_executable(art-unittest
	${PROJECT_SOURCE_DIR}/include/adaptive_radix_tree.h
	${PROJECT_SOURCE_DIR}/test/int_to_int-tests.cpp
	${PROJECT_SOURCE_DIR}/test/string_to_int-tests.cpp
)

add_subdirectory( ${GTEST_DIR}/googletest )

target_link_libraries(art-unittest
	gtest
	gtest_main
)
