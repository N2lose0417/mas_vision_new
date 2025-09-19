# hikcamera/CMakeLists.txt
# 创建静态库
add_library(HikCamera STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/cam_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OpenCVCamera.cpp
)

find_package(OpenCV REQUIRED)
find_package(yaml-cpp REQUIRED)

# 手动指定 MVS 路径（安装默认 /opt/MVS）
set(MVS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SDK")
set(MVS_INCLUDE_DIR "${MVS_ROOT}/include")
set(MVS_LIB_DIR "${MVS_ROOT}/lib/64")

find_library(MVS_CAMERA_LIB NAMES MvCameraControl PATHS ${MVS_LIB_DIR} NO_DEFAULT_PATH)

if(MVS_CAMERA_LIB)
    target_sources(HikCamera PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/HikCamera.cpp)
    target_include_directories(HikCamera PUBLIC ${MVS_INCLUDE_DIR})
    target_link_directories(HikCamera PUBLIC ${MVS_LIB_DIR})
    target_link_libraries(HikCamera PUBLIC ${MVS_CAMERA_LIB})
    target_compile_definitions(HikCamera PUBLIC HAS_HIK_CAMERA=1)
    target_link_options(HikCamera PUBLIC "-Wl,-rpath,${MVS_LIB_DIR}")
else()
    message(WARNING "MvCameraControl library not found. Hikvision SDK features will be disabled.")
    target_compile_definitions(HikCamera PUBLIC HAS_HIK_CAMERA=0)
endif()

# 设置库的包含目录
target_include_directories(HikCamera PUBLIC
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接所需库
target_link_libraries(HikCamera PUBLIC
    ${OpenCV_LIBS}
    rm_utils
    yaml-cpp::yaml-cpp
)

# 将库添加到全局变量中，供主CMakeLists.txt使用
list(APPEND MAIN_PROJECT_LIBRARIES HikCamera)
list(APPEND MAIN_PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(MAIN_PROJECT_LIBRARIES ${MAIN_PROJECT_LIBRARIES} CACHE INTERNAL "Libraries for main project")
set(MAIN_PROJECT_INCLUDE_DIRS ${MAIN_PROJECT_INCLUDE_DIRS} CACHE INTERNAL "Include directories for main project")