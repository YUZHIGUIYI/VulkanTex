add_library(VulkanTex STATIC
    ${CMAKE_CURRENT_LIST_DIR}/VulkanTex.h
    ${CMAKE_CURRENT_LIST_DIR}/VulkanTex.cpp
    ${CMAKE_CURRENT_LIST_DIR}/VulkanTexDDS.h
    ${CMAKE_CURRENT_LIST_DIR}/VulkanTexDDS.cpp)

target_include_directories(VulkanTex PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(VulkanTex PUBLIC Vulkan::Headers)
set_target_properties(VulkanTex PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON)
