#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "defs.hpp"
#include "math.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "embed/roboto.font"

#include "gpu/common.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"

#include "log.hpp"
#include <vulkan/vulkan.h>

#include "embed/color.frag"
#include "embed/color.vert"

static void glfw_error_callback(int error, const char* description) {
  log_error("GLFW Error %d: %s", error, description);
}


static void transition_image(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier2 image_barrier = {};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  image_barrier.pNext = nullptr;

  image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = new_layout;

  VkImageAspectFlags aspectMask = (image_barrier.newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageSubresourceRange sub_image = {};
  sub_image.aspectMask = aspectMask;
  sub_image.baseMipLevel = 0;
  sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
  sub_image.baseArrayLayer = 0;
  sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;
  image_barrier.subresourceRange = sub_image; 
  image_barrier.image = image;

  VkDependencyInfo dependency_info = {};
  dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.pNext = nullptr;

  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers = &image_barrier;
  vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

int main(int, char**) {
  log_info("Hello world from %s!!", "Mini Engine");

  // put some allocators here
  Linear_Allocator frame_allocator = { mega_bytes(20) };
  Linear_Allocator temp_allocator  = { mega_bytes(20) };

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

  init_gpu_instance(temp_allocator);
  defer { cleanup_gpu_instance(); };

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  defer { ImGui::DestroyContext(); };

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding              = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForVulkan(window, true);
  defer { ImGui_ImplGlfw_Shutdown(); };

  int w, h;
  glfwGetFramebufferSize(window, &w, &h);

  auto device = create_device(temp_allocator);
  defer { destroy_device(device); };

  auto surface = create_surface(temp_allocator, device, window, w, h);
  defer { destroy_surface(device, surface); };

  // Load Fonts
  ImFontConfig font_config{};
  font_config.FontDataOwnedByAtlas = false;
  io.Fonts->AddFontFromMemoryTTF((void*)roboto_font_bytes, sizeof(roboto_font_bytes), 14.0f, &font_config);

  /// Just want to see the colors we are printing out
  log_info("info");
  log_error("error");
  log_debug("debug");
  log_warn("warn");

  bool show_demo_window    = true;
  bool show_another_window = false;
  ImVec4 clear_color       = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  VkShaderModuleCreateInfo vertex_shader_create_info = {};
  vertex_shader_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  vertex_shader_create_info.pNext                    = nullptr;
  vertex_shader_create_info.codeSize                 = sizeof(_shader_vert_spv);
  vertex_shader_create_info.pCode                    = _shader_vert_spv;

  VkShaderModule vertex_shader_module = { 0 };
  VK_CHECK(vkCreateShaderModule(device.logical, &vertex_shader_create_info, device.allocator, &vertex_shader_module));
  defer { vkDestroyShaderModule(device.logical, vertex_shader_module, device.allocator); };

  VkShaderModuleCreateInfo fragment_shader_create_info = {};
  fragment_shader_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  fragment_shader_create_info.pNext                    = nullptr;
  fragment_shader_create_info.codeSize                 = sizeof(_shader_frag_spv);
  fragment_shader_create_info.pCode                    = _shader_frag_spv;

  VkShaderModule fragment_shader_module = { 0 };
  VK_CHECK(
      vkCreateShaderModule(device.logical, &fragment_shader_create_info, device.allocator, &fragment_shader_module));
  defer { vkDestroyShaderModule(device.logical, fragment_shader_module, device.allocator); };

  VkAttachmentDescription attachment = {};
  attachment.format                  = surface.format.format;
  attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear every frame.
  attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment = {};
  color_attachment.attachment            = 0;
  color_attachment.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &color_attachment;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass          = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass          = 0;
  dependency.srcStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask       = 0;
  dependency.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_create_info = {};
  render_pass_create_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.attachmentCount        = 1;
  render_pass_create_info.pAttachments           = &attachment;
  render_pass_create_info.subpassCount           = 1;
  render_pass_create_info.pSubpasses             = &subpass;
  render_pass_create_info.dependencyCount        = 1;
  render_pass_create_info.pDependencies          = &dependency;

  VkRenderPass render_pass;
  VK_CHECK(vkCreateRenderPass(device.logical, &render_pass_create_info, device.allocator, &render_pass));
  defer { vkDestroyRenderPass(device.logical, render_pass, device.allocator); };

  // VkPipeline pipeline;

  //
  struct Frame_Data {
    VkCommandPool command_pool;
    VkFence fence;
    VkCommandBuffer command_buffer;
    VkFramebuffer framebuffer;
  };

  u64 acc = 0;
  // initialize_frame_data
  const auto num_images = surface.num_images; // used to test if something changed.
  auto frame_data       = frame_allocator.push_array_no_init<Frame_Data>(surface.num_images);

  for(auto i = 0; i < num_images; ++i) {
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = 0;
    command_pool_create_info.queueFamilyIndex = device.queue_family;
    VK_CHECK(vkCreateCommandPool(device.logical, &command_pool_create_info, device.allocator, &frame_data[i].command_pool));

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = frame_data[i].command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.logical, &command_buffer_allocate_info, &frame_data[i].command_buffer));

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device.logical, &fence_create_info, device.allocator, &frame_data[i].fence));
  }

  defer {
    for(auto i = 0; i < num_images; ++i) {
      vkDestroyCommandPool(device.logical, frame_data[i].command_pool,device.allocator);
      vkDestroyFence(device.logical, frame_data[i].fence, device.allocator);
    }
  };

  u8 sync_idx = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto& current_frame = frame_data[surface.frame_idx];
    VK_CHECK(vkWaitForFences(device.logical, 1, &current_frame.fence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(device.logical, 1, &current_frame.fence));

    VkResult result = vkAcquireNextImageKHR(device.logical, surface.swapchain, UINT64_MAX, surface.image_avail[sync_idx], nullptr, &surface.frame_idx);
    if(result != VK_SUCCESS) {
      // may need to resize here.
      continue;
    }

    vkResetCommandPool(device.logical, current_frame.command_pool, 0);

    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(current_frame.command_buffer, &command_buffer_begin_info));

    transition_image(current_frame.command_buffer, surface.images[surface.frame_idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue clear_value = { { 0.0f, 0.0f, (float)fabs(sin(acc / 120.f)), 1.0f } };

    VkImageSubresourceRange clear_range =  {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    //clear image
    vkCmdClearColorImage(current_frame.command_buffer, surface.images[surface.frame_idx], VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

    transition_image(current_frame.command_buffer, surface.images[surface.frame_idx], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(current_frame.command_buffer));

    VkCommandBufferSubmitInfo command_buffer_submit_info = {};
    command_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer = current_frame.command_buffer;
    command_buffer_submit_info.deviceMask = 0;
    command_buffer_submit_info.pNext = nullptr;

    VkSemaphoreSubmitInfo wait_semaphore_submit_info = {};
    wait_semaphore_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_semaphore_submit_info.semaphore = surface.image_avail[sync_idx];
    wait_semaphore_submit_info.deviceIndex = 0;
    wait_semaphore_submit_info.value = 1;
    wait_semaphore_submit_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    
    VkSemaphoreSubmitInfo signal_semaphore_submit_info = {};
    signal_semaphore_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_semaphore_submit_info.semaphore = surface.render_done[sync_idx];
    signal_semaphore_submit_info.deviceIndex = 0;
    signal_semaphore_submit_info.value = 1;
    signal_semaphore_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    VkSubmitInfo2 submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_buffer_submit_info;
    submit_info.pWaitSemaphoreInfos = &wait_semaphore_submit_info;
    submit_info.waitSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_semaphore_submit_info;
    submit_info.signalSemaphoreInfoCount = 1;
    VK_CHECK(vkQueueSubmit2(device.queue, 1, &submit_info, current_frame.fence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pImageIndices = &surface.frame_idx;
    present_info.pSwapchains = &surface.swapchain;
    present_info.swapchainCount = 1;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &surface.render_done[sync_idx];
    VK_CHECK(vkQueuePresentKHR(device.queue, &present_info));

    // acquire after present
    // there's a chance that this will have to wait somehow.
    sync_idx = (sync_idx + 1) % surface.num_images;
    acc++;
  }

  vkDeviceWaitIdle(device.logical);
  return 0;
}


#if 0
// Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to
    // learn more about Dear ImGui!).
    if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
      static float f     = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

      ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
      ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window) {
      ImGui::Begin("Another Window", &show_another_window); // Pass a pointer to our bool variable (the window will have
                                                            // a closing button that will clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me")) show_another_window = false;
      ImGui::End();
    }

    // Rendering UI
    ImGui::Render();
    ImDrawData* main_draw_data   = ImGui::GetDrawData();
    const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
    // draw::set_clear_color(
    //     vk_win,
    //     clear_color.x * clear_color.w,
    //     clear_color.y * clear_color.w,
    //     clear_color.z * clear_color.w,
    //     clear_color.w);

    // if (!main_is_minimized) draw::render_frame(vk_win, main_draw_data);

    // Update and Render additional Platform Windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
    }
    // if (!main_is_minimized) draw::present_frame(vk_win);
#endif