#include <fstream>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

// Helper function to check a VkResult and abort if it is not VK_SUCCESS.
void CheckError(VkResult result, const char *operation) {
  if (result != VK_SUCCESS) {
    std::cerr << operation << " failed: " << result << "\n";
    abort();
  }
}

// Helper function to find a compute queue family index.
uint32_t FindComputeQueueFamilyIndex(VkPhysicalDevice physical_device) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                           queue_families.data());

  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      return i;
    }
  }
  return -1;
}

// Helper function to find a suitable memory type index.
uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  std::cerr << "failed to find suitable memory type\n";
  abort();
}

// Helper function to read a SPIR-V file.
std::vector<char> ReadFile(const char *filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "failed to open file\n";
    abort();
  }
  size_t file_size = (size_t)file.tellg();
  std::vector<char> buffer(file_size);
  file.seekg(0);
  file.read(buffer.data(), file_size);
  file.close();
  return buffer;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <shader.spv>\n";
    return 1;
  }
  auto shader_code = ReadFile(argv[1]);

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Vulkan Compute Sandbox";
  app_info.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  VkInstance instance;
  CheckError(vkCreateInstance(&create_info, nullptr, &instance),
             "creating instance");

  uint32_t device_count = 0;
  CheckError(vkEnumeratePhysicalDevices(instance, &device_count, nullptr),
             "enumerating physical devices");
  if (device_count == 0) {
    std::cerr << "No Vulkan devices found\n";
    return 1;
  }
  std::vector<VkPhysicalDevice> devices(device_count);
  CheckError(
      vkEnumeratePhysicalDevices(instance, &device_count, devices.data()),
      "enumerating physical devices");

  uint32_t queue_family_index = -1;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  for (const auto &device : devices) {
    queue_family_index = FindComputeQueueFamilyIndex(device);
    if (queue_family_index != -1) {
      physical_device = device;
      break;
    }
  }
  if (physical_device == VK_NULL_HANDLE) {
    std::cerr << "failed to find a GPU with a compute queue\n";
    return 1;
  }

  VkPhysicalDeviceProperties device_properties;
  vkGetPhysicalDeviceProperties(physical_device, &device_properties);
  std::cout << "Device name: " << device_properties.deviceName << "\n";
  std::cout << "Driver: " << std::hex << device_properties.driverVersion
            << std::dec << "\n";

  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  float queue_priority = 1.0f;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkPhysicalDeviceFeatures device_features{};
  VkDeviceCreateInfo device_create_info{};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pEnabledFeatures = &device_features;

  VkDevice device;
  CheckError(
      vkCreateDevice(physical_device, &device_create_info, nullptr, &device),
      "creating device");

  VkQueue compute_queue;
  vkGetDeviceQueue(device, queue_family_index, 0, &compute_queue);

  VkShaderModuleCreateInfo shader_module_create_info{};
  shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_create_info.codeSize = shader_code.size();
  shader_module_create_info.pCode =
      reinterpret_cast<const uint32_t *>(shader_code.data());

  VkShaderModule compute_shader_module;
  CheckError(vkCreateShaderModule(device, &shader_module_create_info, nullptr,
                                  &compute_shader_module),
             "creating shader module");

  const uint32_t kBufferSize = 16 * sizeof(uint32_t);
  VkBuffer in_buffer;
  VkBuffer out_buffer;
  VkDeviceMemory in_buffer_memory;
  VkDeviceMemory out_buffer_memory;

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = kBufferSize;
  buffer_info.usage =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  CheckError(vkCreateBuffer(device, &buffer_info, nullptr, &in_buffer),
             "creating in buffer");
  CheckError(vkCreateBuffer(device, &buffer_info, nullptr, &out_buffer),
             "creating out buffer");

  VkMemoryRequirements in_mem_requirements;
  vkGetBufferMemoryRequirements(device, in_buffer, &in_mem_requirements);
  VkMemoryRequirements out_mem_requirements;
  vkGetBufferMemoryRequirements(device, out_buffer, &out_mem_requirements);

  uint32_t mem_type_index =
      FindMemoryType(physical_device, in_mem_requirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = in_mem_requirements.size;
  alloc_info.memoryTypeIndex = mem_type_index;

  CheckError(vkAllocateMemory(device, &alloc_info, nullptr, &in_buffer_memory),
             "allocating in buffer memory");
  alloc_info.allocationSize = out_mem_requirements.size;
  CheckError(vkAllocateMemory(device, &alloc_info, nullptr, &out_buffer_memory),
             "allocating out buffer memory");

  CheckError(vkBindBufferMemory(device, in_buffer, in_buffer_memory, 0),
             "binding in buffer memory");
  CheckError(vkBindBufferMemory(device, out_buffer, out_buffer_memory, 0),
             "binding out buffer memory");

  // Supply values for in_buffer.
  void *in_data;
  CheckError(vkMapMemory(device, in_buffer_memory, 0, kBufferSize, 0, &in_data),
             "mapping in_buffer memory");
  uint32_t *in_values = (uint32_t *)in_data;
  in_values[0] = 100;
  in_values[1] = 42;
  in_values[2] = 102;
  in_values[3] = 103;
  vkUnmapMemory(device, in_buffer_memory);

  // Initialize out_buffer to zeros.
  void *out_data;
  CheckError(
      vkMapMemory(device, out_buffer_memory, 0, kBufferSize, 0, &out_data),
      "mapping out_buffer memory");
  memset(out_data, 0, kBufferSize);
  vkUnmapMemory(device, out_buffer_memory);

  VkDescriptorSetLayoutBinding in_layout_binding{};
  in_layout_binding.binding = 0;
  in_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  in_layout_binding.descriptorCount = 1;
  in_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding out_layout_binding{};
  out_layout_binding.binding = 1;
  out_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  out_layout_binding.descriptorCount = 1;
  out_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  std::vector<VkDescriptorSetLayoutBinding> bindings = {
      in_layout_binding,
      out_layout_binding,
  };
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
  layout_info.pBindings = bindings.data();

  VkDescriptorSetLayout descriptor_set_layout;
  CheckError(vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                         &descriptor_set_layout),
             "creating descriptor set layout");

  std::vector<VkDescriptorPoolSize> pool_sizes(2);
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 2;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[1].descriptorCount = 2;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = 1;

  VkDescriptorPool descriptor_pool;
  CheckError(
      vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool),
      "creating descriptor pool");

  VkDescriptorSetAllocateInfo set_alloc_info{};
  set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_alloc_info.descriptorPool = descriptor_pool;
  set_alloc_info.descriptorSetCount = 1;
  set_alloc_info.pSetLayouts = &descriptor_set_layout;

  VkDescriptorSet descriptor_set;
  CheckError(vkAllocateDescriptorSets(device, &set_alloc_info, &descriptor_set),
             "allocating descriptor sets");

  VkDescriptorBufferInfo in_buffer_info{};
  in_buffer_info.buffer = in_buffer;
  in_buffer_info.offset = 0;
  in_buffer_info.range = kBufferSize;

  VkDescriptorBufferInfo out_buffer_info{};
  out_buffer_info.buffer = out_buffer;
  out_buffer_info.offset = 0;
  out_buffer_info.range = kBufferSize;

  VkWriteDescriptorSet in_write{};
  in_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  in_write.dstSet = descriptor_set;
  in_write.dstBinding = 0;
  in_write.dstArrayElement = 0;
  in_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  in_write.descriptorCount = 1;
  in_write.pBufferInfo = &in_buffer_info;

  VkWriteDescriptorSet out_write{};
  out_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  out_write.dstSet = descriptor_set;
  out_write.dstBinding = 1;
  out_write.dstArrayElement = 0;
  out_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  out_write.descriptorCount = 1;
  out_write.pBufferInfo = &out_buffer_info;

  std::vector<VkWriteDescriptorSet> descriptor_writes = {in_write, out_write};
  vkUpdateDescriptorSets(device,
                         static_cast<uint32_t>(descriptor_writes.size()),
                         descriptor_writes.data(), 0, nullptr);

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

  VkPipelineLayout pipeline_layout;
  CheckError(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                    &pipeline_layout),
             "creating pipeline layout");

  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.stage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipeline_info.stage.module = compute_shader_module;
  pipeline_info.stage.pName = "main";

  VkPipeline compute_pipeline;
  CheckError(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                      nullptr, &compute_pipeline),
             "creating pipeline");

  VkCommandPoolCreateInfo cmd_pool_info{};
  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.queueFamilyIndex = queue_family_index;
  cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandPool command_pool;
  CheckError(
      vkCreateCommandPool(device, &cmd_pool_info, nullptr, &command_pool),
      "creating command pool");

  VkCommandBufferAllocateInfo cmd_buf_alloc_info{};
  cmd_buf_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_buf_alloc_info.commandPool = command_pool;
  cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_buf_alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  CheckError(
      vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, &command_buffer),
      "allocating command buffers");

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  CheckError(vkBeginCommandBuffer(command_buffer, &begin_info),
             "beginning command buffer");
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

  VkMemoryBarrier memory_barrier{};
  memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                       &memory_barrier, 0, nullptr, 0, nullptr);

  vkCmdDispatch(command_buffer, 1, 1, 1);

  memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  memory_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memory_barrier, 0,
                       nullptr, 0, nullptr);

  CheckError(vkEndCommandBuffer(command_buffer), "ending command buffer");

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  CheckError(vkQueueSubmit(compute_queue, 1, &submit_info, VK_NULL_HANDLE),
             "submitting queue");
  CheckError(vkQueueWaitIdle(compute_queue), "waiting for queue idle");

  // Read back buffer output.
  CheckError(
      vkMapMemory(device, out_buffer_memory, 0, kBufferSize, 0, &out_data),
      "mapping memory");
  uint32_t *out_values = (uint32_t *)out_data;
  std::cout << "Output data:\n";
  for (int i = 0; i < 16; ++i) {
    std::cout << out_values[i] << " ";
  }
  std::cout << "\n";
  vkUnmapMemory(device, out_buffer_memory);

  vkDestroyPipeline(device, compute_pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
  vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
  vkDestroyBuffer(device, in_buffer, nullptr);
  vkDestroyBuffer(device, out_buffer, nullptr);
  vkFreeMemory(device, in_buffer_memory, nullptr);
  vkFreeMemory(device, out_buffer_memory, nullptr);
  vkDestroyShaderModule(device, compute_shader_module, nullptr);
  vkDestroyCommandPool(device, command_pool, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);

  return 0;
}
