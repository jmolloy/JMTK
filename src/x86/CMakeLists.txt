# We need a custom macro to call NASM for assembling instead of GNU as.
macro(assemble OUTPUT_LIST DIR FILENAME INPUT_LIST)

  set(FN "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o")
  add_custom_command(OUTPUT ${FN}
    # Ensure the target directory actually exists.
    COMMAND ${NASM} ${NASM_ARGS} -o ${FN} ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/${FILENAME}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/${FILENAME}
    COMMENT "Assembling ${FILENAME}.o")

  list(APPEND ${OUTPUT_LIST} ${INPUT_LIST} ${FN})

endmacro(assemble)

set(SCANTABLE "src/x86/en_US.scantable" CACHE STRING "Scantable location")
add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/scantable.inc"
  COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/scantable.py"
  "${CMAKE_SOURCE_DIR}/${SCANTABLE}" "${CMAKE_BINARY_DIR}/scantable.inc"
  DEPENDS "${CMAKE_SOURCE_DIR}/${SCANTABLE}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating keyboard scancode table from ${SCANTABLE}")

set(SOURCES ${SOURCES}
  x86/bringup-2.c
  x86/screen.c
  x86/serial.c
  x86/keyboard.c
  x86/gdt.c
  x86/interrupts.c
  x86/hal.c)
assemble(SOURCES x86 bringup-1.s ${SOURCES})
assemble(SOURCES x86 interrupts.s ${SOURCES})
