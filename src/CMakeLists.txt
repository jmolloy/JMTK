project(kernel)
cmake_minimum_required(VERSION 2.6)

macro(assemble OUTPUT_LIST FILENAME INPUT_LIST)
  if (${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o
      COMMAND mkdir -p `dirname ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}`
      COMMAND ${NASM} ${NASM_ARGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o
      ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
      COMMENT "Assembling ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o")
    list(APPEND ${OUTPUT_LIST} ${INPUT_LIST}
      ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o)
  else(${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
    list(APPEND ${OUTPUT_LIST} ${INPUT_LIST} FILENAME)
  endif(${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
endmacro(assemble)


set(TARGET "Hosted" CACHE STRING "Build target type - Hosted, X86, X64 or ARM")

if(${TARGET} STREQUAL "Hosted")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/hosted/link.ld -nostdlibinc")
  list(APPEND DEFINITIONS "-DHOSTED")
endif()

if(${TARGET} STREQUAL "X86")
  find_program(NASM NAMES nasm yasm)
  set(NASM_ARGS -felf)

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/x86/link.ld -nostdlibinc -nostdlib -m32 -Xlinker --build-id=none")
  list(APPEND DEFINITIONS -DX86 -m32 -ffreestanding)
endif()

list(APPEND DEFINITIONS "-Wall" "-Wextra" "-Wno-unused-parameter" "-std=c99" "-nostdlibinc" "-fno-builtin")

include_directories(include)
set(SOURCES
  hal.c
  main.c
  string.c
  console.c
)
if (${TARGET} STREQUAL "X86")
  assemble(SOURCES x86/bringup-1.s ${SOURCES})
  set(SOURCES ${SOURCES}
    x86/bringup-2.c)
endif()

add_definitions(${DEFINITIONS})
add_executable(kernel ${SOURCES})

add_custom_target(floppy.img ALL "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/../scripts/image.py"
  "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/kernel" "${CMAKE_BINARY_DIR}/floppy.img"
  DEPENDS kernel)
  

set(CMAKE_TARGET kernel)
configure_file("${CMAKE_SOURCE_DIR}/test/lit.site.cfg.in"
  "${CMAKE_BINARY_DIR}/test/lit.site.cfg"  @ONLY)

include(FindPythonInterp)
add_custom_target(check "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/lit/lit.py" "${CMAKE_SOURCE_DIR}/test" "-sv" DEPENDS kernel)
