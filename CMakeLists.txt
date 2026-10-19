cmake_minimum_required(VERSION 3.16)
project(moas C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

# --- SDL2 discovery ---
if(EMSCRIPTEN)
  # Static SDL2 under SDL2_ROOT (nix/wasm.nix) or Emscripten's -sUSE_SDL=2 port.
  if(SDL2_ROOT)
    if(NOT EXISTS "${SDL2_ROOT}/lib/libSDL2.a")
      message(FATAL_ERROR "SDL2_ROOT=${SDL2_ROOT} has no lib/libSDL2.a")
    endif()
    # Official SDL2 install: include/SDL2/SDL.h — our sources use <SDL.h>
    if(EXISTS "${SDL2_ROOT}/include/SDL2/SDL.h")
      set(SDL2_INCLUDE_DIRS "${SDL2_ROOT}/include/SDL2")
    elseif(EXISTS "${SDL2_ROOT}/include/SDL.h")
      set(SDL2_INCLUDE_DIRS "${SDL2_ROOT}/include")
    else()
      message(FATAL_ERROR "SDL2_ROOT=${SDL2_ROOT} has no SDL.h")
    endif()
    set(SDL2_LIBRARIES "${SDL2_ROOT}/lib/libSDL2.a")
  else()
    add_compile_options(-sUSE_SDL=2)
    add_link_options(-sUSE_SDL=2)
    set(SDL2_INCLUDE_DIRS "")
    set(SDL2_LIBRARIES "")
  endif()
else()
  find_package(SDL2 REQUIRED)
endif()

set(MOAS_SOURCES
  src/chk.c
  src/datapath.c
  src/debug.c
  src/digi_io.c
  src/digicore.c
  src/grafix.c
  src/hercbak.c
  src/if.c
  src/init.c
  src/lang.c
  src/io.c
  src/misc.c
  src/moas.c
  src/mouse.c
  src/powerbld.c
  src/print.c
  src/platform_sdl.c
)

if(EMSCRIPTEN)
  list(APPEND MOAS_SOURCES src/moas_globals.c)
endif()
add_executable(moas ${MOAS_SOURCES})

target_include_directories(moas PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(moas PRIVATE ${SDL2_LIBRARIES} m)

if(TARGET SDL2::SDL2)
  target_link_libraries(moas PRIVATE SDL2::SDL2)
endif()

# Default install data dir; can be overridden (-DMOAS_DATADIR=...)
if(NOT DEFINED MOAS_DATADIR OR MOAS_DATADIR STREQUAL "")
  if(EMSCRIPTEN)
    set(MOAS_DATADIR "/data")
  else()
    set(MOAS_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/moas")
  endif()
endif()

target_compile_definitions(moas PRIVATE
  $<$<NOT:$<BOOL:${WIN32}>>:LINUX>
  $<$<NOT:$<BOOL:${WIN32}>>:_GNU_SOURCE>
  "MOAS_DATADIR=\"${MOAS_DATADIR}\""
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(moas PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-pointer-sign
    -Wno-char-subscripts
    -Wno-missing-field-initializers
    -Wno-missing-braces
    -Wno-misleading-indentation
    -Wno-implicit-fallthrough
    -Wno-unused-result
    -Wno-format-truncation
    -Wno-format-overflow
    -Wno-absolute-value
    -Wno-error=format-security
    # Tentative globals need -fcommon on desktop; wasm32's MCStreamer
    # crashes on common symbols (emitLocalCommonSymbol).
    $<IF:$<BOOL:${EMSCRIPTEN}>,-fno-common,-fcommon>
  )
endif()


# Standalone data inspection tool (no SDL).
if(NOT EMSCRIPTEN)
  add_executable(moas-util tools/moas-util.c)
  target_compile_definitions(moas-util PRIVATE
    $<$<NOT:$<BOOL:${WIN32}>>:_GNU_SOURCE>
  )
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(moas-util PRIVATE -Wall -Wextra)
  endif()
  install(TARGETS moas-util RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(EMSCRIPTEN)
  # Web page: moas.html + .js + .wasm (+ data pack). ASYNCIFY lets the
  # existing blocking Wait/getch loop yield to the browser.
  # Touch overlay (Save/Load/USE/LOOK) same as Android handheld UI.
  target_compile_definitions(moas PRIVATE MOAS_OVERLAY_BUTTONS)
  if(NOT DEFINED MOAS_VERSION)
    set(MOAS_VERSION "0.1.0-sdl")
  endif()
  set(MOAS_SOURCE_URL "https://github.com/Grumbel/moas")
  configure_file(mk/wasm/shell.html ${CMAKE_CURRENT_BINARY_DIR}/shell.html @ONLY)

  # Stage only the game data (skip original DOS executables)
  set(_wasm_data ${CMAKE_CURRENT_BINARY_DIR}/wasm-data)
  file(MAKE_DIRECTORY ${_wasm_data})
  file(GLOB _manick_files "${CMAKE_SOURCE_DIR}/manick/*")
  foreach(_f ${_manick_files})
    get_filename_component(_name "${_f}" NAME)
    if(NOT _name MATCHES "\\.(EXE|exe)$")
      configure_file("${_f}" "${_wasm_data}/${_name}" COPYONLY)
    endif()
  endforeach()

  set_target_properties(moas PROPERTIES SUFFIX ".html")
  target_link_options(moas PRIVATE
    --shell-file ${CMAKE_CURRENT_BINARY_DIR}/shell.html
    -sALLOW_MEMORY_GROWTH=1
    -sGROWABLE_ARRAYBUFFERS=0
    -sFORCE_FILESYSTEM=1
    -lidbfs.js
    -sASYNCIFY
    -sASYNCIFY_STACK_SIZE=65536
    -sENVIRONMENT=web
    -sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap
    # Header-defined DOS globals: wasm-ld has no -fcommon merge; keep first def.
    -Wl,--allow-multiple-definition
    --preload-file ${_wasm_data}@/data
  )
  set_property(TARGET moas APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/mk/wasm/shell.html)

  set(_web ${CMAKE_CURRENT_BINARY_DIR}/moas)
  install(FILES ${_web}.html ${_web}.js ${_web}.wasm DESTINATION .)
  # Emscripten also emits a .data pack for --preload-file
  install(FILES ${_web}.data DESTINATION . OPTIONAL)
  install(FILES ${_web}.html DESTINATION . RENAME index.html)
else()
  install(TARGETS moas RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # Ship game data (exclude the original DOS binaries)
  install(DIRECTORY lang/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/moas/lang
    OPTIONAL)
  install(DIRECTORY manick/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/moas
    FILES_MATCHING
      PATTERN "GAME.*"
      PATTERN "LICENCE.DAT"
      PATTERN "Licence.Dat"
      PATTERN "licence.dat"
    PATTERN "GAME.EXE" EXCLUDE
    PATTERN "DOS4GW.EXE" EXCLUDE
  )

  install(FILES COPYING DESTINATION ${CMAKE_INSTALL_DATADIR}/moas)

  # --- Linux desktop integration ---
  install(FILES data/moas.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

  install(DIRECTORY data/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
    FILES_MATCHING PATTERN "*.png")

  install(FILES data/metainfo/org.grumbel.moas.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
