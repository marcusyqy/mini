#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "defs.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN

#include "constant/roboto.font"

#include "draw.hpp"
#include "log.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

static void glfw_error_callback(int error, const char* description) {
  log_error("GLFW Error %d: %s", error, description);
}

int main(int, char**) {
  log_info("Hello world from %s!!", "Mini Engine");

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;
  defer { glfwTerminate(); };

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  constexpr auto window_name = "Place holder window name";
  GLFWwindow* window         = glfwCreateWindow(1280, 720, window_name, nullptr, nullptr);
  defer { glfwDestroyWindow(window); };

  if (!glfwVulkanSupported()) {
    log_error("GLFW: Vulkan not supported");
    return 1;
  }

  u32 extension_count     = {};
  const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  draw::setup_vulkan(extensions, extension_count);
  defer { draw::cleanup_vulkan(); };

  ImGui::StyleColorsDark();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
  // ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application
  // (e.g. use an assertion, or display an error and quit).
  // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
  // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
  // backslash \\ !
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
  // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr,
  // io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != nullptr);
  // load font
  ImFontConfig font_config{};
  font_config.FontDataOwnedByAtlas = false;
  io.Fonts->AddFontFromMemoryTTF((void*)roboto_font_bytes, sizeof(roboto_font_bytes), 15.0f, &font_config);

  /// at the end since i want to see the colors i am printing out.
  log_info("info");
  log_error("error");
  log_debug("debug");
  log_warn("warn");

  return 0;
}
