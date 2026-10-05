cmake_minimum_required(VERSION 3.20)
project(hybrid_mfg_x4 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
add_executable(hybrid_mfg_x4 main.cpp)
target_compile_definitions(hybrid_mfg_x4 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(hybrid_mfg_x4 PRIVATE /W4 /utf-8)
target_include_directories(hybrid_mfg_x4 PRIVATE ../../logs/research/dlssg-ampere/deep/official-sdk/include)
target_link_libraries(hybrid_mfg_x4 PRIVATE
 "${CMAKE_CURRENT_SOURCE_DIR}/../ngx-dlssg-probe/vendor/nvsdk_ngx_s.lib"
 advapi32 user32 shell32 ole32 dxgi d3d12 d3dcompiler version bcrypt wintrust psapi shlwapi)
