cmake_minimum_required(VERSION 3.10)
project(VulkanComputeSandbox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found. Please install it and make sure it's in your path.")
endif()

add_executable(vulkan_compute_test main.cpp)
target_include_directories(vulkan_compute_test PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vulkan_compute_test ${Vulkan_LIBRARIES})
