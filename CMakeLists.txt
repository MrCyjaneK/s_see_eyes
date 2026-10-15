cmake_minimum_required(VERSION 3.16)
project(eyes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TEST_EYES "Build test_eyes CLI" ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/models/yolo26n.onnx")
  add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/src/assets.cpp"
    COMMAND python3 "${CMAKE_SOURCE_DIR}/scripts/gen_assets.py"
            "${CMAKE_SOURCE_DIR}/models"
            "${CMAKE_SOURCE_DIR}/src/assets.cpp"
    DEPENDS "${CMAKE_SOURCE_DIR}/scripts/gen_assets.py"
    COMMENT "Generating assets.cpp from models/"
  )
  add_custom_target(eyes_assets DEPENDS "${CMAKE_SOURCE_DIR}/src/assets.cpp")
endif()

if(NOT SBS_PREFIX)
  if(DEFINED ENV{SBS_PREFIX} AND NOT "$ENV{SBS_PREFIX}" STREQUAL "")
    set(SBS_PREFIX "$ENV{SBS_PREFIX}")
  else()
    set(SBS_PREFIX "$ENV{HOME}/sbs/_/x86_64-linux-gnu")
  endif()
endif()
set(SBS_PREFIX "${SBS_PREFIX}" CACHE PATH "simplybs host prefix")

# iOS sets CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY; keep simplybs prefixes searchable.
find_library(ONNXRUNTIME_LIB onnxruntime PATHS "${SBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_library(ZXING_LIB ZXing PATHS "${SBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_library(EASYEXIF_LIB easyexif PATHS "${SBS_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)

include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  "${SBS_PREFIX}/include"
  "${SBS_PREFIX}/include/ZXing"
)

set(EYES_SOURCES
  src/abi.cpp
  src/assets.cpp
  src/jobs.cpp
  src/ctx.cpp
  src/image.cpp
  src/exif.cpp
  src/features.cpp
  src/onnx_session.cpp
  src/yolo.cpp
  src/ocr.cpp
  src/codes.cpp
  src/face.cpp
  src/face_id.cpp
  src/tags.cpp
  src/caption.cpp
  src/pipeline.cpp
  src/output_json.cpp
)

if(APPLE)
  list(APPEND EYES_SOURCES src/image_apple.cpp)
endif()

add_library(eyes SHARED ${EYES_SOURCES})
if(TARGET eyes_assets)
  add_dependencies(eyes eyes_assets)
endif()

target_link_libraries(eyes PRIVATE ${ZXING_LIB} ${EASYEXIF_LIB})
if(NOT APPLE)
  target_link_libraries(eyes PRIVATE ${ONNXRUNTIME_LIB})
endif()

if(APPLE)
  # ONNX Runtime is dlopened. The simplybs rpath is only so a local build can
  # open libonnxruntime.dylib before the app embeds onnxruntime.framework.
  set_target_properties(eyes PROPERTIES
    OUTPUT_NAME "eyes"
    PREFIX "lib"
    INSTALL_NAME_DIR "@rpath"
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "${SBS_PREFIX}/lib"
    INSTALL_RPATH "@loader_path;${SBS_PREFIX}/lib"
  )
  target_link_libraries(eyes PRIVATE
    "-framework Foundation"
    "-framework CoreFoundation"
    "-framework Accelerate"
    "-framework CoreGraphics"
    "-framework ImageIO"
  )
  if(EXISTS "${CMAKE_SOURCE_DIR}/exports.txt")
    target_link_libraries(eyes PRIVATE "-Wl,-exported_symbols_list,${CMAKE_SOURCE_DIR}/exports.txt")
  endif()
elseif(ANDROID)
  # $ORIGIN so jniLibs/libonnxruntime.so resolves next to libeyes.
  set_target_properties(eyes PROPERTIES
    OUTPUT_NAME "eyes"
    PREFIX "lib"
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
  target_link_libraries(eyes PRIVATE android log)
  if(EXISTS "${CMAKE_SOURCE_DIR}/exports.txt")
    set(_eyes_ver "${CMAKE_CURRENT_BINARY_DIR}/exports.ver")
    file(STRINGS "${CMAKE_SOURCE_DIR}/exports.txt" _eyes_syms)
    set(_eyes_ver_body "{\n  global:\n")
    foreach(_sym ${_eyes_syms})
      string(REGEX REPLACE "^_" "" _sym "${_sym}")
      string(APPEND _eyes_ver_body "    ${_sym};\n")
    endforeach()
    string(APPEND _eyes_ver_body "  local:\n    *;\n};\n")
    file(WRITE "${_eyes_ver}" "${_eyes_ver_body}")
    target_link_libraries(eyes PRIVATE "-Wl,--version-script,${_eyes_ver}")
  endif()
else()
  # $ORIGIN so dist/libonnxruntime.so resolves next to libeyes.
  target_link_libraries(eyes PRIVATE pthread dl)
  target_link_options(eyes PRIVATE "-static-libstdc++" "-static-libgcc")
  set_target_properties(eyes PROPERTIES
    OUTPUT_NAME "eyes"
    PREFIX "lib"
    BUILD_WITH_INSTALL_RPATH ON
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
  if(EXISTS "${CMAKE_SOURCE_DIR}/exports.txt")
    set(_eyes_ver "${CMAKE_CURRENT_BINARY_DIR}/exports.ver")
    file(STRINGS "${CMAKE_SOURCE_DIR}/exports.txt" _eyes_syms)
    set(_eyes_ver_body "{\n  global:\n")
    foreach(_sym ${_eyes_syms})
      string(REGEX REPLACE "^_" "" _sym "${_sym}")
      string(APPEND _eyes_ver_body "    ${_sym};\n")
    endforeach()
    string(APPEND _eyes_ver_body "  local:\n    *;\n};\n")
    file(WRITE "${_eyes_ver}" "${_eyes_ver_body}")
    target_link_libraries(eyes PRIVATE "-Wl,--version-script,${_eyes_ver}")
  endif()
endif()

if(BUILD_TEST_EYES AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT ANDROID)
  add_executable(test_eyes tools/test_eyes.cpp)
  target_link_libraries(test_eyes PRIVATE eyes)
  if(NOT APPLE)
    target_link_libraries(test_eyes PRIVATE ${ONNXRUNTIME_LIB} dl)
  endif()
endif()

install(TARGETS eyes LIBRARY DESTINATION lib)
