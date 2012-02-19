project(kernel)
cmake_minimum_required(VERSION 2.6)

macro(assemble OUTPUT_LIST FILENAME INPUT_LIST)
  if (${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o
      COMMAND mkdir -p `dirname ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}`
      COMMAND ${NASM} ${NASM_ARGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o
      ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
      COMMENT "Assembling ${FILENAME}.o")
    list(APPEND ${OUTPUT_LIST} ${INPUT_LIST}
      ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o)
  else(${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
    list(APPEND ${OUTPUT_LIST} ${INPUT_LIST} FILENAME)
  endif(${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
endmacro(assemble)


set(TARGET "Hosted" CACHE STRING "Build target type - Hosted, X86, X64 or ARM")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker --whole-archive ${CMAKE_BINARY_DIR}/liblibkernel.a -Xlinker --no-whole-archive ")

if(${TARGET} STREQUAL "Hosted")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/src/hosted/link.ld")
  list(APPEND DEFINITIONS "-DHOSTED")
endif()

if(${TARGET} STREQUAL "X86")
  find_program(NASM NAMES nasm yasm)
  set(NASM_ARGS -felf)

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/src/x86/link.ld -nostdlibinc -nostdlib -m32 -Xlinker --build-id=none")
  list(APPEND DEFINITIONS -DX86 -m32 -ffreestanding)
endif()

list(APPEND DEFINITIONS "-Wall" "-Wextra" "-Wno-unused-parameter" "-std=c99" "-nostdlibinc" "-fno-builtin")

set(INCLUDES ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_BINARY_DIR})
set(SOURCES
  src/adt/ringbuf.c
  src/console.c
  src/hal.c
  src/main.c
  src/string.c
  src/printf.c
)
if (${TARGET} STREQUAL "X86")
  assemble(SOURCES src/x86/bringup-1.s ${SOURCES})
  set(SOURCES ${SOURCES}
    src/x86/bringup-2.c
    src/x86/screen.c
    src/x86/serial.c
    src/x86/keyboard.c)
endif()

include(FindPythonInterp)
add_custom_target(check "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/test/lit/lit.py" "${CMAKE_SOURCE_DIR}/test" "-sv" DEPENDS kernel)


include_directories(${INCLUDES})
add_definitions(${DEFINITIONS})
add_library(libkernel ${SOURCES})


add_executable(kernel src/dummy.c)
target_link_libraries(kernel libkernel)
set(CMAKE_TARGET kernel)
configure_file("${CMAKE_SOURCE_DIR}/test/lit.site.cfg.in"
  "${CMAKE_BINARY_DIR}/test/lit.site.cfg"  @ONLY)

if(${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
  macro(make_image name elf out depend)
    add_custom_target(${name} ALL
      "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/image.py"
      "${CMAKE_SOURCE_DIR}/src" ${elf} ${out} DEPENDS ${depend})
  endmacro(make_image)

  make_image(kernel.img ${CMAKE_BINARY_DIR}/kernel ${CMAKE_BINARY_DIR}/kernel.img kernel)
  
  set(SCANTABLE "src/x86/en_US.scantable" CACHE STRING "Scantable location")
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/scantable.inc"
    COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/scantable.py"
    "${CMAKE_SOURCE_DIR}/${SCANTABLE}" "${CMAKE_BINARY_DIR}/scantable.inc"
    DEPENDS "${CMAKE_SOURCE_DIR}/${SCANTABLE}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating keyboard scancode table from ${SCANTABLE}")
  set_source_files_properties(src/x86/keyboard.c OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/scantable.inc")

  add_executable(screen examples/screen.c)
  target_link_libraries(screen libkernel)
  make_image(screen.img ${CMAKE_BINARY_DIR}/examples/screen ${CMAKE_BINARY_DIR}/examples/screen.img screen)

  add_executable(keyboard examples/keyboard.c)
  target_link_libraries(keyboard libkernel)
  make_image(keyboard.img ${CMAKE_BINARY_DIR}/examples/keyboard ${CMAKE_BINARY_DIR}/examples/keyboard.img keyboard)

  add_executable(serial examples/serial.c)
  target_link_libraries(serial libkernel)
  make_image(serial.img ${CMAKE_BINARY_DIR}/examples/serial ${CMAKE_BINARY_DIR}/examples/serial.img serial)

  set_target_properties(keyboard screen serial
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples)

endif()

