#pragma once
#include "defs.hpp"

namespace draw {

struct Vk_Vars {};

Vk_Vars setup_vulkan(const char** extensions, u32 count);
void cleanup_vulkan();

} // namespace draw