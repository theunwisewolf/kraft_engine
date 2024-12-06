set(VENDOR_LIB_SRC src/ufbx.c include/ufbx/ufbx.h)

project(Ufbx LANGUAGES C)

add_library(Ufbx ${VENDOR_LIB_SRC})

target_include_directories(Ufbx PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/ufbx)