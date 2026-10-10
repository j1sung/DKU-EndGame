cmake_minimum_required(VERSION 3.20)
project(eos_token_verifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED IMPORTED_TARGET json-c)

set(EOS_SDK_ROOT "" CACHE PATH "Path containing EOS SDK Include and lib directories")
if(NOT EXISTS "${EOS_SDK_ROOT}/Include/eos_sdk.h" OR
   NOT EXISTS "${EOS_SDK_ROOT}/lib/libEOSSDK-Linux-Shipping.so")
  message(FATAL_ERROR "EOS_SDK_ROOT must contain Include/eos_sdk.h and lib/libEOSSDK-Linux-Shipping.so")
endif()

add_library(EOSSDK SHARED IMPORTED GLOBAL)
set_target_properties(EOSSDK PROPERTIES
  IMPORTED_LOCATION "${EOS_SDK_ROOT}/lib/libEOSSDK-Linux-Shipping.so"
  INTERFACE_INCLUDE_DIRECTORIES "${EOS_SDK_ROOT}/Include"
)

add_library(eos_protocol src/protocol.cpp)
target_include_directories(eos_protocol PUBLIC src)
target_link_libraries(eos_protocol PUBLIC PkgConfig::JSON_C)
target_compile_options(eos_protocol PRIVATE -Wall -Wextra -Wpedantic)

add_library(eos_verifier src/eos-verifier.cpp)
target_include_directories(eos_verifier PUBLIC src)
target_link_libraries(eos_verifier PUBLIC eos_protocol ${CMAKE_DL_LIBS})
target_include_directories(eos_verifier PRIVATE "${EOS_SDK_ROOT}/Include")
target_compile_options(eos_verifier PRIVATE -Wall -Wextra -Wpedantic)

add_executable(eos-token-verifier src/main.cpp)
target_link_libraries(eos-token-verifier PRIVATE eos_verifier)
target_compile_options(eos-token-verifier PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(eos-token-verifier PROPERTIES
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)
add_custom_command(TARGET eos-token-verifier POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${EOS_SDK_ROOT}/lib/libEOSSDK-Linux-Shipping.so"
    "$<TARGET_FILE_DIR:eos-token-verifier>/libEOSSDK-Linux-Shipping.so"
)

if(BUILD_TESTING)
  add_executable(protocol-test test/protocol-test.cpp)
  target_link_libraries(protocol-test PRIVATE eos_protocol)
  target_compile_options(protocol-test PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME protocol-test COMMAND protocol-test)
endif()
