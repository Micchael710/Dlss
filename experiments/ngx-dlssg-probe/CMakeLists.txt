cmake_minimum_required(VERSION 3.20)
project(ngx_stock_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
add_executable(ngx_stock_probe probe.cpp)
target_compile_definitions(ngx_stock_probe PRIVATE VK_NO_PROTOTYPES NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(ngx_stock_probe PRIVATE /W4 /utf-8)
target_include_directories(ngx_stock_probe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../logs/research/dlssg-ampere/deep/official-sdk/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../superresolution/native/cpp/third_party")
target_link_libraries(ngx_stock_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nvsdk_ngx_s.lib"
    advapi32 user32 shell32 ole32 dxgi d3d12 d3d11 version bcrypt wintrust psapi shlwapi)
add_custom_command(TARGET ngx_stock_probe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nvngx_dlssg.dll" "$<TARGET_FILE_DIR:ngx_stock_probe>/nvngx_dlssg.dll")
