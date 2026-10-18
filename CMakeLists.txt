cmake_minimum_required(VERSION 3.16)
project(moas C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

set(MOAS_SOURCES
  src/chk.c
  src/digi_io.c
  src/digicore.c
  src/grafix.c
  src/hercbak.c
  src/if.c
  src/init.c
  src/io.c
  src/misc.c
  src/moas.c
  src/mouse.c
  src/powerbld.c
  src/print.c
  src/platform_sdl.c
)

add_executable(moas ${MOAS_SOURCES})

target_include_directories(moas PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(moas PRIVATE ${SDL2_LIBRARIES} m)

if(TARGET SDL2::SDL2)
  target_link_libraries(moas PRIVATE SDL2::SDL2)
endif()

target_compile_definitions(moas PRIVATE
  $<$<NOT:$<BOOL:${WIN32}>>:LINUX>
  $<$<NOT:$<BOOL:${WIN32}>>:_GNU_SOURCE>
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(moas PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -fcommon
  )
endif()

install(TARGETS moas RUNTIME DESTINATION bin)
