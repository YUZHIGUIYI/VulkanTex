# VulkanTex
## Introduction
Similar to [DirectXTex](https://github.com/microsoft/DirectXTex) project, encode a VkImage as a DDS file or a TGA file.

## Getting started
Just link VulkanTex library in CMakeLists.txt and include VulkanTex header file in your project.
```cmake
# In CMakeLists.txt
target_link_libraries(your_project PRIVATE VulkanTex);
```
```cpp
// In cpp
#include "VulkanTex.h"
```

## Dependencies
[Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/main)