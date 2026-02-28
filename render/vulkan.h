#ifndef CUTS_RENDER_VULKAN_H
#define CUTS_RENDER_VULKAN_H

#include <vulkan/vulkan.h>

struct c_vulkan_instance {
	VkInstance instance;
};

struct c_vulkan_device {
	VkDevice device;
};

struct c_vulkan {
	VkInstance instance;
	// struct c_vulkan_instance *instance;		
	struct c_vulkan_device *device;		
};

struct c_vulkan *c_vulkan_init(int drm_fd);
void c_vulkan_free(struct c_vulkan *vk);

#endif
