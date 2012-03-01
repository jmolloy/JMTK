if (${TARGET} STREQUAL "X86" OR ${TARGET} STREQUAL "X64")
  include(x86/CMakeLists.txt)
endif()

if (${TARGET} STREQUAL "Hosted")
  include(hosted/CMakeLists.txt)
endif()

include(adt/CMakeLists.txt)

set(SOURCES ${SOURCES}
  console.c
  hal.c
  main.c
  string.c
  snprintf.c
  stdio.c
  stdlib.c
  readline.c
  debugger.c
)
set_source_files_properties(${GENERATED_FILES} PROPERTIES GENERATED 1)
set_source_files_properties(x86/keyboard.c OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/scantable.inc")

add_library(k ${SOURCES})

set(TARGETS X86 X64 Hosted)
add_image(kernel "X86" dummy.c)
add_image(kernel "Hosted" dummy.c)
target_link_libraries(kernel k)
