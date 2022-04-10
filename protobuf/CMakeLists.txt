cmake_minimum_required(VERSION 3.5)

set(PROTO_C_COMPILER "protoc-c")
set(C_OUT_PATH "${CMAKE_SOURCE_DIR}/build/protobuf")
set(PROTO_PATH "${CMAKE_CURRENT_LIST_DIR}/")
file(GLOB PROTO_SRCS "${CMAKE_CURRENT_LIST_DIR}/*.proto")

execute_process(
COMMAND mkdir -p ${C_OUT_PATH}
)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
foreach(proto_file ${PROTO_SRCS})
string(REPLACE ".proto" ".pb-c.c" c_file ${proto_file})
get_filename_component(c_file ${c_file} NAME)
string(PREPEND c_file "${C_OUT_PATH}/")
add_custom_command(OUTPUT ${c_file}
COMMAND ${PROTO_C_COMPILER} --c_out=${C_OUT_PATH} --proto_path=${PROTO_PATH} ${proto_file}
WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
DEPENDS ${proto_file}
COMMENT "Generated ${c_file}"
)
list(APPEND C_OUT_FILES ${c_file})
endforeach()
endif()


idf_component_register(
  SRCS ${C_OUT_FILES}
  INCLUDE_DIRS ${C_OUT_PATH}
  REQUIRES protobuf-c
)