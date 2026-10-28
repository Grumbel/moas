cmake_minimum_required(VERSION 3.16)

# Source of truth: top-level VERSION (e.g. "0.1.0-dev"), or
# -DPROJECT_VERSION_FULL=... from packaging (Nix may append .<revCount>+g<rev>).
if(NOT DEFINED PROJECT_VERSION_FULL)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

# project() needs a numeric version; strip -dev / +git / similar suffixes.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _project_version_cmake "${PROJECT_VERSION_FULL}")

project(moas VERSION ${_project_version_cmake} LANGUAGES C)
message(STATUS "MOAS version: ${PROJECT_VERSION_FULL}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

# --- SDL2 discovery ---
# Sources use #include <SDL.h> (not <SDL2/SDL.h>), so INCLUDE_DIRS must point
# at the directory that contains SDL.h (often .../include/SDL2).
#
# Callers may pre-set SDL2_INCLUDE_DIRS + SDL2_LIBRARIES via -D (r36s sysroot
# cross-build). In that case skip find_package so host CMake configs cannot
# overwrite the sysroot paths.
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
elseif(SDL2_INCLUDE_DIRS AND SDL2_LIBRARIES)
  # Pre-set by the caller (e.g. nix/r36s.nix against the ArkOS/eoan sysroot).
  message(STATUS "SDL2 (pre-set): include=${SDL2_INCLUDE_DIRS} lib=${SDL2_LIBRARIES}")
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
  src/langgfx.c
  src/log.c
  src/io.c
  src/misc.c
  src/moas.c
  src/mouse.c
  src/powerbld.c
  src/print.c
  src/platform_sdl.c
  src/pngread.c
  src/pngwrite.c
  src/settings.c
)

if(EMSCRIPTEN)
  list(APPEND MOAS_SOURCES src/moas_globals.c)
endif()
add_executable(manick ${MOAS_SOURCES})

target_include_directories(manick PRIVATE src ${SDL2_INCLUDE_DIRS})
if(WIN32)
  # libm is not used on MinGW; SDL2main is pulled via find_package / SDL2_LIBRARIES.
  target_link_libraries(manick PRIVATE ${SDL2_LIBRARIES})
else()
  target_link_libraries(manick PRIVATE ${SDL2_LIBRARIES} m)
endif()

if(TARGET SDL2::SDL2)
  target_link_libraries(manick PRIVATE SDL2::SDL2)
endif()
# Windows entry point: SDL2main calls SDL_main (main after #include <SDL.h>).
if(WIN32 AND TARGET SDL2::SDL2main)
  target_link_libraries(manick PRIVATE SDL2::SDL2main)
endif()

# Default install data dir; can be overridden (-DMOAS_DATADIR=...)
if(NOT DEFINED MOAS_DATADIR OR MOAS_DATADIR STREQUAL "")
  if(EMSCRIPTEN)
    set(MOAS_DATADIR "/data")
  else()
    set(MOAS_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/manick")
  endif()
endif()

target_compile_definitions(manick PRIVATE
  $<$<NOT:$<BOOL:${WIN32}>>:LINUX>
  $<$<NOT:$<BOOL:${WIN32}>>:_GNU_SOURCE>
  "MOAS_DATADIR=\"${MOAS_DATADIR}\""
  "MOAS_VERSION=\"${PROJECT_VERSION_FULL}\""
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(manick PRIVATE
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
    -Wno-stringop-truncation
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
  add_executable(moas-util tools/moas-util.c tools/mu_common.c tools/mu_data.c
    tools/mu_gfx.c tools/mu_png.c tools/mu_script.c src/pngread.c
    src/pngwrite.c)
  target_include_directories(moas-util PRIVATE src)
  target_compile_definitions(moas-util PRIVATE
    $<$<NOT:$<BOOL:${WIN32}>>:_GNU_SOURCE>
  )
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(moas-util PRIVATE -Wall -Wextra)
  endif()
  install(TARGETS moas-util RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # unpack + pack of the shipped data must give identical files.
  if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_test(NAME moas-util-roundtrip
      COMMAND ${CMAKE_COMMAND} -DMOAS_UTIL=$<TARGET_FILE:moas-util>
        -DDATADIR=${CMAKE_CURRENT_SOURCE_DIR}/manick
        -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/roundtrip
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/roundtrip.cmake)
  endif()
endif()

# --- Game data ---
# manick-src/ (moas-util unpack format) is the source; it is packed into
# GAME.* at build time. manick/ is the untouched original: pack reuses its
# encoded graphics where the pixels are unchanged, so an unedited tree gives
# byte-identical files. Cross builds can't run moas-util and take already
# packed data via -DMOAS_GAMEDATA_DIR (Nix: the moas-gamedata package).
set(MOAS_GAMEDATA_FILES GAME.1 GAME.2 GAME.3 GAME.4 GAME.PCK GAME.XXX LICENCE.DAT)
set(MOAS_GAMEDATA_DIR "" CACHE PATH "Already packed game data (empty = pack manick-src/)")
if(MOAS_GAMEDATA_DIR)
  set(_gamedata "${MOAS_GAMEDATA_DIR}")
elseif(TARGET moas-util AND NOT CMAKE_CROSSCOMPILING)
  set(_gamedata "${CMAKE_CURRENT_BINARY_DIR}/gamedata")
  file(GLOB_RECURSE _gamedata_src CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/manick-src/*")
  set(_gamedata_out "")
  foreach(_f ${MOAS_GAMEDATA_FILES})
    list(APPEND _gamedata_out "${_gamedata}/${_f}")
  endforeach()
  add_custom_command(OUTPUT ${_gamedata_out}
    COMMAND moas-util --srcdir "${CMAKE_CURRENT_SOURCE_DIR}/manick-src"
      --outdir "${_gamedata}" --reference "${CMAKE_CURRENT_SOURCE_DIR}/manick" pack
    DEPENDS moas-util ${_gamedata_src}
    COMMENT "Packing manick-src/ into ${_gamedata}"
    VERBATIM)
  add_custom_target(gamedata ALL DEPENDS ${_gamedata_out})
else()
  set(_gamedata "")
  if(EMSCRIPTEN)
    message(FATAL_ERROR "The web build needs -DMOAS_GAMEDATA_DIR=<packed data> "
      "(moas-util pack, or nix build .#moas-gamedata)")
  endif()
  message(STATUS "No game data for install (cross build without MOAS_GAMEDATA_DIR)")
endif()

if(EMSCRIPTEN)
  # Web page: manick.html + .js + .wasm (+ data pack). ASYNCIFY lets the
  # existing blocking Wait/getch loop yield to the browser.
  # Touch overlay buttons on by default (as on Android); runtime switch
  # --touch-controls / F12 menu exists on every SDL target.
  target_compile_definitions(manick PRIVATE MOAS_OVERLAY_BUTTONS)
  set(MOAS_VERSION "${PROJECT_VERSION_FULL}")
  set(MOAS_SOURCE_URL "https://github.com/Grumbel/moas")
  configure_file(mk/wasm/shell.html ${CMAKE_CURRENT_BINARY_DIR}/shell.html @ONLY)

  # Stage only the game data (skip original DOS executables)
  set(_wasm_data ${CMAKE_CURRENT_BINARY_DIR}/wasm-data)
  file(MAKE_DIRECTORY ${_wasm_data})
  foreach(_f ${MOAS_GAMEDATA_FILES})
    configure_file("${_gamedata}/${_f}" "${_wasm_data}/${_f}" COPYONLY)
  endforeach()
  # .po files and per-language pictures (lang/<code>/<kind>/*.png)
  file(GLOB_RECURSE _lang_files RELATIVE "${CMAKE_SOURCE_DIR}/lang"
    "${CMAKE_SOURCE_DIR}/lang/*.po" "${CMAKE_SOURCE_DIR}/lang/*.png")
  foreach(_f ${_lang_files})
    configure_file("${CMAKE_SOURCE_DIR}/lang/${_f}" "${_wasm_data}/lang/${_f}" COPYONLY)
  endforeach()

  set_target_properties(manick PROPERTIES SUFFIX ".html")
  target_link_options(manick PRIVATE
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
  set_property(TARGET manick APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/mk/wasm/shell.html)

  set(_web ${CMAKE_CURRENT_BINARY_DIR}/manick)
  install(FILES ${_web}.html ${_web}.js ${_web}.wasm DESTINATION .)
  # Emscripten also emits a .data pack for --preload-file
  install(FILES ${_web}.data DESTINATION . OPTIONAL)
  install(FILES ${_web}.html DESTINATION . RENAME index.html)
else()
  install(TARGETS manick RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  # Ship game data (exclude the original DOS binaries)
  install(DIRECTORY lang/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/manick/lang
    OPTIONAL)
  if(_gamedata)
    foreach(_f ${MOAS_GAMEDATA_FILES})
      install(FILES "${_gamedata}/${_f}" DESTINATION ${CMAKE_INSTALL_DATADIR}/manick)
    endforeach()
  endif()

  install(FILES COPYING DESTINATION ${CMAKE_INSTALL_DATADIR}/manick)

  # --- Linux desktop integration ---
  install(FILES data/manick.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

  install(DIRECTORY data/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor
    FILES_MATCHING PATTERN "*.png")

  install(FILES data/metainfo/org.grumbel.manick.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()

# Sanitizers for development builds, e.g. -DMOAS_SANITIZE=address,undefined
# (the `nix develop` moas-configure sets this by default). Empty = off.
set(MOAS_SANITIZE "" CACHE STRING "Comma-separated -fsanitize= list (empty = off)")
if(MOAS_SANITIZE AND NOT EMSCRIPTEN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "Sanitizers: ${MOAS_SANITIZE}")
  foreach(tgt manick moas-util)
    if(TARGET ${tgt})
      # Script bytecode is read as unaligned shorts (Parameter[] in
      # Execute); by design, so UBSan's alignment check is just noise.
      target_compile_options(${tgt} PRIVATE -fsanitize=${MOAS_SANITIZE}
        -fno-sanitize=alignment -fno-omit-frame-pointer)
      target_link_options(${tgt} PRIVATE -fsanitize=${MOAS_SANITIZE})
    endif()
  endforeach()
endif()
