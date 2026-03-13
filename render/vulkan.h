#ifndef CUTS_RENDER_VULKAN_H
#define CUTS_RENDER_VULKAN_H

#include "render/render.h"
#include <vulkan/vulkan.h>

struct c_vulkan {
	VkInstance instance;
	struct {
		VkDevice dev;
		VkPhysicalDevice phdev;
		uint32_t q_family;
	} device;

	struct {
		PFN_vkGetMemoryFdPropertiesKHR 	vkGetMemoryFdPropertiesKHR;
		PFN_vkGetSemaphoreFdKHR 		vkGetSemaphoreFdKHR;
		PFN_vkImportSemaphoreFdKHR 		vkImportSemaphoreFdKHR;
	} proc;
};

struct c_vulkan_buffer {
	VkImage image;
};

struct c_vulkan *c_vulkan_init(int drm_fd);
void c_vulkan_free(struct c_vulkan *vk);
struct c_format *c_vulkan_query_formats(struct c_vulkan *vk, size_t *entries_n);
VkImage c_vulkan_import_dmabuf(struct c_vulkan *vk, struct c_dmabuf_params *params, struct c_format *format);

#endif
