cmake_minimum_required(VERSION 3.12)

project(rco)

set(SRC

		../core/asm/context_jump.S
		../core/details/context.cpp
		../core/rcontext.cpp
		../task/task.cpp

		../cpc/channel.cpp

		../scheduler/processor.cpp
		../scheduler/scheduler.cpp
		../scheduler/runtime.cpp
		common/semaphore.h)

include_directories(../third_party/jemalloc/include)
link_directories(../third_party/jemalloc/lib)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ../../lib)

enable_language(ASM)

#add_library(${PROJECT_NAME} SHARED ${SRC})

set(CMAKE_CXX_FLAGS "-g")

set(CMAKE_CXX_STANDARD 11)

add_executable(${PROJECT_NAME} main.cpp ${SRC})
target_link_libraries(${PROJECT_NAME} pthread)


