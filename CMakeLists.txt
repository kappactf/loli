cmake_minimum_required(VERSION 3.6.0)
project("Loli" C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s -O2")

set(LOLI_MAJOR "0")
set(LOLI_MINOR "8")
add_definitions(-DLOLI_VERSION="${LOLI_MAJOR}.${LOLI_MINOR}")

file(GLOB_RECURSE LOLI_SRC src/*.c src/*.h)
list(FILTER LOLI_SRC EXCLUDE REGEX ".*loli.c$")

add_library(libloli_obj OBJECT ${LOLI_SRC})
add_library(libloli SHARED $<TARGET_OBJECTS:libloli_obj>)

if(NOT MINGW AND NOT WIN32 AND NOT MSVC)
  set_target_properties(libloli_obj PROPERTIES COMPILE_FLAGS "-fPIC")
endif()

set_target_properties(libloli PROPERTIES PREFIX "")

add_executable(loli src/loli.c $<TARGET_OBJECTS:libloli_obj>)
target_link_libraries(loli m)
target_link_libraries(libloli m)

# Fix for UNIX systems
if (UNIX)
   target_link_libraries(loli ${CMAKE_DL_LIBS})
   target_link_libraries(libloli ${CMAKE_DL_LIBS})
endif(UNIX)
