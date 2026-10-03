cmake_minimum_required(VERSION 3.20)
project(dlssg_external_harness LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
add_executable(dlssg_external_harness harness.cpp)
target_compile_definitions(dlssg_external_harness PRIVATE VK_NO_PROTOTYPES NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(dlssg_external_harness PRIVATE /W4 /utf-8)
target_include_directories(dlssg_external_harness PRIVATE
 "../../logs/research/dlssg-ampere/deep/official-sdk/include"
 "../../superresolution/native/cpp/third_party")
target_link_libraries(dlssg_external_harness PRIVATE
 "${CMAKE_CURRENT_SOURCE_DIR}/../ngx-dlssg-probe/vendor/nvsdk_ngx_s.lib"
 advapi32 user32 shell32 ole32 dxgi d3d12 d3d11 version bcrypt wintrust psapi shlwapi)
