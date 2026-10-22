# Unpack the game data, pack it back and require identical files.
#   cmake -DMOAS_UTIL=... -DDATADIR=... -DWORKDIR=... -P roundtrip.cmake
file(REMOVE_RECURSE "${WORKDIR}")

execute_process(COMMAND "${MOAS_UTIL}" --datadir "${DATADIR}" --outdir "${WORKDIR}/src" unpack
  RESULT_VARIABLE rc)
if(rc)
  message(FATAL_ERROR "unpack failed")
endif()

execute_process(COMMAND "${MOAS_UTIL}" --srcdir "${WORKDIR}/src" --outdir "${WORKDIR}/data"
  --reference "${DATADIR}" pack
  RESULT_VARIABLE rc)
if(rc)
  message(FATAL_ERROR "pack failed")
endif()

foreach(f GAME.1 GAME.2 GAME.3 GAME.4 GAME.PCK GAME.XXX LICENCE.DAT)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${DATADIR}/${f}" "${WORKDIR}/data/${f}"
    RESULT_VARIABLE rc)
  if(rc)
    message(FATAL_ERROR "${f} differs after unpack + pack")
  endif()
endforeach()
