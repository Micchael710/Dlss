cmake_minimum_required(VERSION 3.23)
project(WisteriaFsrBridge LANGUAGES CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The initial FSR bridge targets Windows x64 only")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
foreach(path FSR_SDK_ROOT SR_VULKAN_HEADERS JNI_HOME)
    if(NOT DEFINED ${path})
        message(FATAL_ERROR "${path} must point to verified local headers")
    endif()
endforeach()
add_library(wisteria_fsr_bridge SHARED bridge.cpp)
target_include_directories(wisteria_fsr_bridge PRIVATE
    "${FSR_SDK_ROOT}/ffx-api/include" "${SR_VULKAN_HEADERS}"
    "${JNI_HOME}/include" "${JNI_HOME}/include/win32")
target_compile_definitions(wisteria_fsr_bridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(wisteria_fsr_bridge PRIVATE /W4 /utf-8)
# The official effects DLL is resolved dynamically. No AMD swapchain or Vulkan loader linkage.
