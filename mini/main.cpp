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
#include <cstdio>

static void glfw_error_callback(int error, const char* description) {
  log_error("GLFW Error %d: %s", error, description);
}

static u64 read_file(Linear_Allocator& arena, char** buffer, const char* file_name) {
  assert(buffer);
  FILE* fp = nullptr;
  fopen_s(&fp, file_name, "rb");
  defer { fclose(fp); };



  if(!fp) return 0;

  fseek(fp, 0L, SEEK_END);
  u64 sz = ftell(fp);
  // fseek(fp, 0L, SEEK_SET);
  rewind(fp);
  *buffer = arena.push_array_no_init<char>(sz);
  sz = fread(*buffer, 1, sz, fp);
  return sz;
}

static void copy_image_to_image(
    VkCommandBuffer cmd,
    VkImage src_image,
    VkImage destination,
    VkExtent2D src_size,
    VkExtent2D dst_size) {
  VkImageBlit2 blit_region = {};
  blit_region.sType        = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
  blit_region.pNext        = nullptr;

  blit_region.srcOffsets[1].x = src_size.width;
  blit_region.srcOffsets[1].y = src_size.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_size.width;
  blit_region.dstOffsets[1].y = dst_size.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount     = 1;
  blit_region.srcSubresource.mipLevel       = 0;

  blit_region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount     = 1;
  blit_region.dstSubresource.mipLevel       = 0;

  VkBlitImageInfo2 blit_info = {};
  blit_info.sType            = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
  blit_info.pNext            = nullptr;
  blit_info.dstImage         = destination;
  blit_info.dstImageLayout   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage         = src_image;
  blit_info.srcImageLayout   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter           = VK_FILTER_LINEAR;
  blit_info.regionCount      = 1;
  blit_info.pRegions         = &blit_region;

  vkCmdBlitImage2(cmd, &blit_info);
}

static void transition_image(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout) {
  VkImageMemoryBarrier2 image_barrier = {};
  image_barrier.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  image_barrier.pNext                 = nullptr;

  image_barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  image_barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  image_barrier.oldLayout = old_layout;
  image_barrier.newLayout = new_layout;

  VkImageAspectFlags aspectMask     = (image_barrier.newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageSubresourceRange sub_image = {};
  sub_image.aspectMask              = aspectMask;
  sub_image.baseMipLevel            = 0;
  sub_image.levelCount              = VK_REMAINING_MIP_LEVELS;
  sub_image.baseArrayLayer          = 0;
  sub_image.layerCount              = VK_REMAINING_ARRAY_LAYERS;
  image_barrier.subresourceRange    = sub_image;
  image_barrier.image               = image;

  VkDependencyInfo dependency_info = {};
  dependency_info.sType            = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency_info.pNext            = nullptr;

  dependency_info.imageMemoryBarrierCount = 1;
  dependency_info.pImageMemoryBarriers    = &image_barrier;
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
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

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


  char* buffer = nullptr;
  u64 buffer_size = read_file(temp_allocator, &buffer, "kernel/gradient.comp.spv");
  assert(buffer_size != 0);

  VkShaderModuleCreateInfo comp_shader_create_info = {};
  comp_shader_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  comp_shader_create_info.pNext                    = nullptr;
  comp_shader_create_info.codeSize                 = buffer_size;
  comp_shader_create_info.pCode                    = (u32*)buffer;

  VkShaderModule comp_shader_module = { 0 };
  VK_CHECK(vkCreateShaderModule(
      device.logical,
      &comp_shader_create_info,
      device.allocator_callbacks,
      &comp_shader_module));
  defer { vkDestroyShaderModule(device.logical, comp_shader_module, device.allocator_callbacks); };

  temp_allocator.clear();
  
  const u32 num_bindings = 1;
  VkDescriptorSetLayoutBinding binding[num_bindings];
  binding[0].binding = 0;
  binding[0].descriptorCount = 1;
  binding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layout_create_info = {};
  layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_create_info.pNext = nullptr;
  layout_create_info.pBindings = binding;
  layout_create_info.bindingCount = ARRAY_SIZE(binding);
  layout_create_info.flags = 0;
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDescriptorSetLayout(device.logical, &layout_create_info, device.allocator_callbacks, &layout));
  defer { vkDestroyDescriptorSetLayout(device.logical, layout, device.allocator_callbacks); };


  VkDescriptorPoolSize descriptor_pool_size[1];
  descriptor_pool_size[0].descriptorCount = 1;
  descriptor_pool_size[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 10;
	pool_info.poolSizeCount = ARRAY_SIZE(descriptor_pool_size);
	pool_info.pPoolSizes = descriptor_pool_size;

  VkDescriptorPool pool;
  VK_CHECK(vkCreateDescriptorPool(device.logical, &pool_info, device.allocator_callbacks, &pool));
  defer { vkDestroyDescriptorPool(device.logical, pool, device.allocator_callbacks); };
  // vkResetDescriptorPool(device.logical, pool, 0);

  // VkPipeline pipeline;
  VkPipelineLayoutCreateInfo compute_layout_create_info{};
	compute_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	compute_layout_create_info.pNext = nullptr;
	compute_layout_create_info.pSetLayouts = &layout;
	compute_layout_create_info.setLayoutCount = 1;

  VkPipelineLayout compute_layout;
	VK_CHECK(vkCreatePipelineLayout(device.logical, &compute_layout_create_info, device.allocator_callbacks, &compute_layout));
  defer {vkDestroyPipelineLayout(device.logical, compute_layout, device.allocator_callbacks); };

  VkPipelineShaderStageCreateInfo pipeline_stage_info{};
	pipeline_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline_stage_info.pNext = nullptr;
	pipeline_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline_stage_info.module = comp_shader_module;
	pipeline_stage_info.pName = "main";

	VkComputePipelineCreateInfo compute_pipeline_create_info{};
	compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	compute_pipeline_create_info.pNext = nullptr;
	compute_pipeline_create_info.layout = compute_layout;
	compute_pipeline_create_info.stage = pipeline_stage_info;
	
  VkPipeline compute_pipeline;
	VK_CHECK(vkCreateComputePipelines(device.logical, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &compute_pipeline));
  defer { vkDestroyPipeline(device.logical, compute_pipeline, device.allocator_callbacks); };

  struct Frame_Data {
    VkCommandPool command_pool;
    VkFence fence;
    VkCommandBuffer command_buffer;
    VkFramebuffer framebuffer;
    Image render_target;
    VkDescriptorSet set;
  };

  u64 acc = 0;

  // initialize_frame_data
  const auto num_images = surface.num_images; // used to test if something changed.
  auto frame_data       = frame_allocator.push_array_no_init<Frame_Data>(surface.num_images);

  VkFormat rt_format               = VK_FORMAT_R16G16B16A16_SFLOAT;
  VkExtent3D rt_extent             = { (u32)surface.width, (u32)surface.height, 1 };
  VkImageUsageFlags rt_usage_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rt_create_info = {};
  rt_create_info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  rt_create_info.pNext             = nullptr;
  rt_create_info.imageType         = VK_IMAGE_TYPE_2D;
  rt_create_info.format            = rt_format;
  rt_create_info.extent            = rt_extent;
  rt_create_info.mipLevels         = 1;
  rt_create_info.arrayLayers       = 1;
  // for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
  rt_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  // optimal tiling, which means the image is stored on the best gpu format
  rt_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  rt_create_info.usage  = rt_usage_flags;

  VmaAllocationCreateInfo rt_alloc_info = {};
  rt_alloc_info.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
  rt_alloc_info.requiredFlags           = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VmaAllocationInfo alloc_info = {};

  for (auto i = 0; i < num_images; ++i) {
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags                   = 0;
    command_pool_create_info.queueFamilyIndex        = device.queue_family;
    VK_CHECK(vkCreateCommandPool(
        device.logical,
        &command_pool_create_info,
        device.allocator_callbacks,
        &frame_data[i].command_pool));

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool                 = frame_data[i].command_pool;
    command_buffer_allocate_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.logical, &command_buffer_allocate_info, &frame_data[i].command_buffer));

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device.logical, &fence_create_info, device.allocator_callbacks, &frame_data[i].fence));
    VK_CHECK(vmaCreateImage(
        device.allocator,
        &rt_create_info,
        &rt_alloc_info,
        &frame_data[i].render_target.image,
        &frame_data[i].render_target.allocation,
        &alloc_info));
    // print alloc_info out here.

    VkImageViewCreateInfo rt_view_create_info = {};
    rt_view_create_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    rt_view_create_info.pNext                 = nullptr;

    rt_view_create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    rt_view_create_info.image                           = frame_data[i].render_target.image;
    rt_view_create_info.format                          = rt_format;
    rt_view_create_info.subresourceRange.baseMipLevel   = 0;
    rt_view_create_info.subresourceRange.levelCount     = 1;
    rt_view_create_info.subresourceRange.baseArrayLayer = 0;
    rt_view_create_info.subresourceRange.layerCount     = 1;
    rt_view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    VK_CHECK(vkCreateImageView(
        device.logical,
        &rt_view_create_info,
        device.allocator_callbacks,
        &frame_data[i].render_target.view));
    frame_data[i].render_target.extent = rt_extent;
    frame_data[i].render_target.format = rt_format;
    
    VkDescriptorSetAllocateInfo set_alloc_info = {};
    set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.pNext = nullptr;
    set_alloc_info.descriptorPool = pool;
    set_alloc_info.descriptorSetCount = 1;
    set_alloc_info.pSetLayouts = &layout;
    VK_CHECK(vkAllocateDescriptorSets(device.logical, &set_alloc_info, &frame_data[i].set));

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_info.imageView = frame_data[i].render_target.view;
    
    VkWriteDescriptorSet draw_image_write = {};
    draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    draw_image_write.pNext = nullptr;
    
    draw_image_write.dstBinding = 0;
    draw_image_write.dstSet = frame_data[i].set;
    draw_image_write.descriptorCount = 1;
    draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    draw_image_write.pImageInfo = &image_info;
    vkUpdateDescriptorSets(device.logical, 1, &draw_image_write, 0, nullptr);
  }

  defer {
    for (auto i = 0; i < num_images; ++i) {
      vkDestroyCommandPool(device.logical, frame_data[i].command_pool, device.allocator_callbacks);
      vkDestroyImageView(device.logical, frame_data[i].render_target.view, device.allocator_callbacks);
      vmaDestroyImage(device.allocator, frame_data[i].render_target.image, frame_data[i].render_target.allocation);
      vkDestroyFence(device.logical, frame_data[i].fence, device.allocator_callbacks);
    }
  };

  u8 sync_idx = 0;
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto& current_frame = frame_data[surface.frame_idx];
    VK_CHECK(vkWaitForFences(device.logical, 1, &current_frame.fence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(device.logical, 1, &current_frame.fence));

    VkResult result = vkAcquireNextImageKHR(
        device.logical,
        surface.swapchain,
        UINT64_MAX,
        surface.image_avail[sync_idx],
        nullptr,
        &surface.frame_idx);
    if (result != VK_SUCCESS) {
      // may need to resize here.
      continue;
    }

    vkResetCommandPool(device.logical, current_frame.command_pool, 0);

    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags                    = 0;
    command_buffer_begin_info.pInheritanceInfo         = nullptr;

    VK_CHECK(vkBeginCommandBuffer(current_frame.command_buffer, &command_buffer_begin_info));

    transition_image(
        current_frame.command_buffer,
        current_frame.render_target.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(current_frame.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(current_frame.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_layout, 0, 1, &current_frame.set, 0, nullptr);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(current_frame.command_buffer, (u32)ceil(current_frame.render_target.extent.width / 16.0), (u32)ceil(current_frame.render_target.extent.height / 16.0), 1);
    // // make a clear-color from frame number. This will flash with a 120 frame period.
    // VkClearColorValue clear_value = { { 0.0f, 0.0f, (float)fabs(sin(acc / 120.f)), 1.0f } };

    // VkImageSubresourceRange clear_range = {};
    // clear_range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
    // clear_range.baseMipLevel            = 0;
    // clear_range.levelCount              = VK_REMAINING_MIP_LEVELS;
    // clear_range.baseArrayLayer          = 0;
    // clear_range.layerCount              = VK_REMAINING_ARRAY_LAYERS;

    // // clear image
    // vkCmdClearColorImage(
    //     current_frame.command_buffer,
    //     current_frame.render_target.image,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     &clear_value,
    //     1,
    //     &clear_range);

    transition_image(
        current_frame.command_buffer,
        current_frame.render_target.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image(
        current_frame.command_buffer,
        surface.images[surface.frame_idx],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkExtent2D render_extent = { current_frame.render_target.extent.width, current_frame.render_target.extent.height };

    VkExtent2D surface_extent = { (u32)surface.width, (u32)surface.height };
    copy_image_to_image(
        current_frame.command_buffer,
        current_frame.render_target.image,
        surface.images[surface.frame_idx],
        render_extent,
        surface_extent);

    transition_image(
        current_frame.command_buffer,
        surface.images[surface.frame_idx],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(current_frame.command_buffer));

    VkCommandBufferSubmitInfo command_buffer_submit_info = {};
    command_buffer_submit_info.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit_info.commandBuffer             = current_frame.command_buffer;
    command_buffer_submit_info.deviceMask                = 0;
    command_buffer_submit_info.pNext                     = nullptr;

    VkSemaphoreSubmitInfo wait_semaphore_submit_info = {};
    wait_semaphore_submit_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_semaphore_submit_info.semaphore             = surface.image_avail[sync_idx];
    wait_semaphore_submit_info.deviceIndex           = 0;
    wait_semaphore_submit_info.value                 = 1;
    wait_semaphore_submit_info.stageMask             = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfo signal_semaphore_submit_info = {};
    signal_semaphore_submit_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_semaphore_submit_info.semaphore             = surface.render_done[sync_idx];
    signal_semaphore_submit_info.deviceIndex           = 0;
    signal_semaphore_submit_info.value                 = 1;
    signal_semaphore_submit_info.stageMask             = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    VkSubmitInfo2 submit_info            = {};
    submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount   = 1;
    submit_info.pCommandBufferInfos      = &command_buffer_submit_info;
    submit_info.pWaitSemaphoreInfos      = &wait_semaphore_submit_info;
    submit_info.waitSemaphoreInfoCount   = 1;
    submit_info.pSignalSemaphoreInfos    = &signal_semaphore_submit_info;
    submit_info.signalSemaphoreInfoCount = 1;
    VK_CHECK(vkQueueSubmit2(device.queue, 1, &submit_info, current_frame.fence));

    VkPresentInfoKHR present_info   = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pImageIndices      = &surface.frame_idx;
    present_info.pSwapchains        = &surface.swapchain;
    present_info.swapchainCount     = 1;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &surface.render_done[sync_idx];
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
