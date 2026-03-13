#define _GNU_SOURCE
#define CUTS_USE_VULKAN

#include <stdlib.h>
#include <assert.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include <sys/stat.h>
#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

#include "render/vulkan.h"
#include "render/render.h"
#include "backend/drm.h"
#include "util/log.h"
#include "util/helpers.h"

static const struct {
    uint32_t drm;
    VkFormat vk;
  } fmts[] = {
    {DRM_FORMAT_XRGB8888, VK_FORMAT_B8G8R8A8_UNORM},
    {DRM_FORMAT_ARGB8888, VK_FORMAT_B8G8R8A8_UNORM},
    // {DRM_FORMAT_NV12,     VK_FORMAT_G8_B8R8_2PLANE_420_UNORM},
    // {DRM_FORMAT_YUV420,   VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM},
  };

static int check_ext(const char *ext, VkExtensionProperties *exts, uint32_t n_exts) {
  for (uint32_t i = 0; i < n_exts; i++) {
    if (STREQ(exts[i].extensionName, ext)) return 1;
  }
  return 0;
}

static VkFormat get_vk_format(uint32_t drm_format) {
  for (size_t i = 0; i < LENGTH(fmts); i++) {
    if (fmts[i].drm == drm_format) return fmts[i].vk;
  }

  assert(1 == 0);
  return 0;
}

__attribute__((unused)) static VkBool32 vulkan_debug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                             VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                            void*                                       pUserData) {
  return VK_TRUE;
};

static const VkImageUsageFlagBits image_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

static const char *phdev_type_str(VkPhysicalDeviceType type) {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      return "OTHER";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "INTEGRATED_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "DISCRETE_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "VIRTUAL_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "CPU";
    default:
      return "UNKNOWN";
  }
}

static void phdev_log(VkPhysicalDeviceProperties *props) {
  uint32_t vk_major = VK_VERSION_MAJOR(props->apiVersion);
  uint32_t vk_minor = VK_VERSION_MINOR(props->apiVersion);
  uint32_t vk_patch = VK_VERSION_PATCH(props->apiVersion);

  uint32_t driver_major = VK_VERSION_MAJOR(props->driverVersion);
  uint32_t driver_minor = VK_VERSION_MINOR(props->driverVersion);
  uint32_t driver_patch = VK_VERSION_PATCH(props->driverVersion);

  c_log(C_LOG_INFO, "Found physical device: %s (%s) %04x:%04x. "
        "Vulkan version: %d.%d.%d. "
        "Driver version: %d.%d.%d", 
        props->deviceName, phdev_type_str(props->deviceType), props->vendorID, props->deviceID,
        vk_major, vk_minor, vk_patch,
        driver_major, driver_minor, driver_patch
        );
}


static int check_dmabuf_support(VkPhysicalDevice phdev, VkFormat format, uint64_t modifier, struct c_format *out) {
  VkPhysicalDeviceImageDrmFormatModifierInfoEXT img_mod_info = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
		.drmFormatModifier = modifier,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkPhysicalDeviceExternalImageFormatInfo e_img_info = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
		.pNext = &img_mod_info,
	};

  VkPhysicalDeviceImageFormatInfo2 img_info = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
    .type = VK_IMAGE_TYPE_2D,
    .format = format,
    .usage = image_usage,
    .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
    .pNext = &e_img_info,
  };

  VkImageFormatProperties2 img_props = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
  }; 

  if (vkGetPhysicalDeviceImageFormatProperties2(phdev, &img_info, &img_props) == VK_SUCCESS) {
    if (out) {
      out->max_width = img_props.imageFormatProperties.maxExtent.width;
      out->max_height = img_props.imageFormatProperties.maxExtent.height;
    }
    return 1;
  };

  return 0;
}

struct c_format *c_vulkan_query_formats(struct c_vulkan *vk, size_t *entries_n) {
  for (size_t f = 0; f < LENGTH(fmts); f++) {
    VkDrmFormatModifierPropertiesListEXT mod_prop = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };

    VkFormatProperties2 fmt_prop = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &mod_prop,
    };

    vkGetPhysicalDeviceFormatProperties2(vk->device.phdev, fmts[f].vk, &fmt_prop);

    VkDrmFormatModifierPropertiesEXT drm_mod_props[mod_prop.drmFormatModifierCount];
    mod_prop.pDrmFormatModifierProperties = drm_mod_props;

    vkGetPhysicalDeviceFormatProperties2(vk->device.phdev, fmts[f].vk, &fmt_prop);

    for (size_t m = 0; m < mod_prop.drmFormatModifierCount; m++) {
      VkDrmFormatModifierPropertiesEXT drm_mod = drm_mod_props[m];
      if (drm_mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT && 
        check_dmabuf_support(vk->device.phdev, fmts[f].vk, drm_mod.drmFormatModifier, NULL)) {
        (*entries_n)++;
      }
    }

  }
  
  struct c_format *table = malloc(*entries_n * sizeof(*table));

  size_t i = 0;
  for (size_t f = 0; f < LENGTH(fmts); f++) {
    char *format_name = drmGetFormatName(fmts[f].drm);
    c_log(C_LOG_INFO, "format=%s", format_name);
    free(format_name);

    VkDrmFormatModifierPropertiesListEXT mod_prop = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    };

    VkFormatProperties2 fmt_prop = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &mod_prop,
    };

    vkGetPhysicalDeviceFormatProperties2(vk->device.phdev, fmts[f].vk, &fmt_prop);

    VkDrmFormatModifierPropertiesEXT drm_mod_props[mod_prop.drmFormatModifierCount];
    mod_prop.pDrmFormatModifierProperties = drm_mod_props;

    vkGetPhysicalDeviceFormatProperties2(vk->device.phdev, fmts[f].vk, &fmt_prop);

    for (size_t m = 0; m < mod_prop.drmFormatModifierCount; m++) {
      VkDrmFormatModifierPropertiesEXT drm_mod = drm_mod_props[m];
      struct c_format *entry = &table[i];
      if (drm_mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT &&
        check_dmabuf_support(vk->device.phdev, fmts[f].vk, drm_mod.drmFormatModifier, entry)) {
        entry->drm_format = fmts[f].drm;
        entry->modifier = drm_mod.drmFormatModifier;
        entry->n_planes = drm_format_num_planes(entry->drm_format);
        entry->supports_disjoint = drm_mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_DISJOINT_BIT;
        
        char *modifier_name = drmGetFormatModifierName(drm_mod.drmFormatModifier);
        c_log(C_LOG_INFO, "   modifier=%s", modifier_name);
        free(modifier_name);

        i++;
      }
    }
  }

  return table;
}

static int vk_device_create(struct c_vulkan *vk, VkExtensionProperties *exts, size_t ext_n) {
  const char *required_exts[32] = {0};
  size_t n_required_exts = 0;
  required_exts[n_required_exts++] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME;
  required_exts[n_required_exts++] = VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME;

  for (size_t i = 0; i < n_required_exts; i++) {
    if (!check_ext(required_exts[i], exts, ext_n)) {
      c_log(C_LOG_ERROR, "%s is not supported", required_exts[i]);
      return -1;
    }
  }

  VkPhysicalDeviceExternalSemaphoreInfo semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR,
  };

  VkExternalSemaphoreProperties semaphore_props = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
  };
  vkGetPhysicalDeviceExternalSemaphoreProperties(vk->device.phdev, &semaphore_info, &semaphore_props);

  if (!(semaphore_props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT)) {
    c_log(C_LOG_ERROR, "Semaphores are not exportable");
    return -1;
  }

  if (!(semaphore_props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT)) {
    c_log(C_LOG_ERROR, "Semaphores are not importable");
    return -1;
  }

  {
    uint32_t q_family_n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk->device.phdev, &q_family_n, NULL);
    
    VkQueueFamilyProperties q_family_props[q_family_n];
    vkGetPhysicalDeviceQueueFamilyProperties(vk->device.phdev, &q_family_n, q_family_props);

    for (size_t i = 0; i < q_family_n; i++) {
      if (q_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        vk->device.q_family = i;
        break;
      }
    }
  }

  const float q_priority[1] = {1.0f};
  VkDeviceQueueCreateInfo q_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = vk->device.q_family,
    .queueCount = 1,
    .pQueuePriorities = q_priority,
  };

  VkPhysicalDeviceSynchronization2Features sync2_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
    .synchronization2 = VK_TRUE,
  };

  VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
    .timelineSemaphore = VK_TRUE,
    .pNext = &sync2_features,
  };

  VkDeviceCreateInfo dev_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &timeline_semaphore_features,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &q_info,
    .ppEnabledExtensionNames = required_exts,
    .enabledExtensionCount = n_required_exts,
  };

  if (vkCreateDevice(vk->device.phdev, &dev_info, NULL, &vk->device.dev) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkCreateDevice failed");
    return -1;
  }

#define load_proc(name) vk->proc.name = (void *)vkGetDeviceProcAddr(vk->device.dev, #name)

  load_proc(vkGetMemoryFdPropertiesKHR);
  load_proc(vkGetSemaphoreFdKHR);
  load_proc(vkImportSemaphoreFdKHR);

  return 0;
}

static int c_vulkan_device_create(struct c_vulkan *vk, int drm_fd) {
  int res;
  uint32_t dev_n = 0;
  if (vkEnumeratePhysicalDevices(vk->instance, &dev_n, NULL) != VK_SUCCESS || dev_n == 0) {
    c_log(C_LOG_ERROR, "vkEnumeratePhysicalDevices failed or number of physical devices is 0");
    return -1;
  }

  VkPhysicalDevice devs[dev_n];
  if ((res = vkEnumeratePhysicalDevices(vk->instance, &dev_n, devs)) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkEnumeratePhysicalDevices failed");
    return -1;
  }
  assert(res != VK_INCOMPLETE);

  struct stat st;
  if (fstat(drm_fd, &st) != 0) {
    c_log(C_LOG_ERROR, "fstat failed: %s", strerror(errno));
    return -1;
  }

  dev_t drm_rdev = st.st_rdev;

  for (size_t i = 0; i < dev_n; i++) {
    VkPhysicalDevice phdev = devs[i];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phdev, &props);
    phdev_log(&props);

    uint32_t ext_n = 0;

    if (vkEnumerateDeviceExtensionProperties(phdev, NULL, &ext_n, NULL) != VK_SUCCESS || ext_n == 0) {
      c_log(C_LOG_ERROR, "vkEnumerateDeviceExtensionProperties failed or number of device extensions is 0");
      return -1;
    }
    
    VkExtensionProperties exts[ext_n];
    if (vkEnumerateDeviceExtensionProperties(phdev, NULL, &ext_n, exts) != VK_SUCCESS) {
      c_log(C_LOG_ERROR, "vkEnumerateDeviceExtensionProperties failed");
      return -1;
    }

    if (!check_ext(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME, exts, ext_n)) {
      c_log(C_LOG_INFO, "VK_EXT_physical_device_drm is not supported");
      return -1;
    }
    int driver_prop_support = check_ext(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, exts, ext_n);

    VkPhysicalDeviceProperties2 props2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };

    VkPhysicalDeviceDrmPropertiesEXT drm_props = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT,
    };

    VkPhysicalDeviceDriverPropertiesKHR driver_props = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR 
    };
    
    if (driver_prop_support)
      drm_props.pNext = &driver_props;
    
    props2.pNext = &drm_props;

    vkGetPhysicalDeviceProperties2(phdev, &props2);

    if (driver_prop_support) {
      c_log(C_LOG_INFO, "Driver name: %s %s", driver_props.driverName, driver_props.driverInfo);
    }

    for (size_t i = 0; i < ext_n; i++) {
      c_log(C_LOG_INFO, "Vulkan device extension: %s", exts[i].extensionName);
    }

    dev_t render_rdev = makedev(drm_props.renderMajor, drm_props.renderMinor);
    dev_t primary_rdev = makedev(drm_props.primaryMajor, drm_props.primaryMinor);
    if (drm_rdev == render_rdev || drm_rdev == primary_rdev) {
      c_log(C_LOG_INFO, "Found matching physical device for %s", props.deviceName);
      vk->device.phdev = phdev;
      return vk_device_create(vk, exts, ext_n);
    }

  }

  c_log(C_LOG_ERROR, "Couldn't find physical device matching opened DRM device");
  return -1;
}

static int c_vulkan_instance_create(VkInstance *instance) {
  PFN_vkEnumerateInstanceVersion vkEnumInstanceVer = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
  uint32_t ver;
  if (vkEnumInstanceVer(&ver) != VK_SUCCESS || ver < VK_VERSION_1_1) {
    c_log(C_LOG_ERROR, "Unsupported vulkan version");
    return -1;
  }

  uint32_t ext_n = 0;
  if (vkEnumerateInstanceExtensionProperties(NULL, &ext_n, NULL) != VK_SUCCESS || ext_n == 0) {
    c_log(C_LOG_ERROR, "vkEnumerateInstanceExtensionProperties failed or number of instance extensions is 0");
    return -1;
  }
  
  VkExtensionProperties exts[ext_n];
  if (vkEnumerateInstanceExtensionProperties(NULL, &ext_n, exts) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkEnumerateInstanceExtensionProperties failed");
    return -1;
  }

  uint32_t n_enabled_exts = 0;
  const char *enabled_exts[1] = {0};

  // int debug_utils = check_ext(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, exts, ext_n);
  // if (debug_utils) {
  //   enabled_exts[n_enabled_exts++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  // }

  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pEngineName = "cuts-renderer";
  app_info.engineVersion = 1;
  app_info.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo instance_info = {0};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount = n_enabled_exts;
  instance_info.ppEnabledExtensionNames = enabled_exts;

  VkDebugUtilsMessengerCreateInfoEXT debug_info = {0};
  debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_info.messageSeverity = 
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  debug_info.messageType = 		
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  if (vkCreateInstance(&instance_info, NULL, instance) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkCreateInstance failed");
    return -1;
  }

  for (size_t i = 0; i < ext_n; i++) {
    c_log(C_LOG_INFO, "Vulkan instance extensions: %s", exts[i].extensionName);
  }

  return 0;
}

static int is_dmabuf_disjoint(struct c_dmabuf_plane *planes, uint32_t n_planes) {
  struct stat st1;
  assert(fstat(planes[0].fd, &st1) == 0);

  for (size_t i = 0; i < n_planes; i++) {
    struct stat st;
    assert(fstat(planes[i].fd, &st) == 0);
    if (st1.st_ino != st.st_ino) return 1;
  }

  return 0;
}

static int find_mem_type(VkPhysicalDevice phdev, VkMemoryPropertyFlags type_bits, uint32_t req) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(phdev, &props);

  for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
    if (type_bits & (1 << i))
      if ((props.memoryTypes[i].propertyFlags & req) == req) return i;
  }

  return -1;
}

VkImage c_vulkan_import_dmabuf(struct c_vulkan *vk, struct c_dmabuf_params *params, struct c_format *format) {

  VkExternalMemoryImageCreateInfo external_info = {
    .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };

  VkSubresourceLayout plane_layouts[params->n_planes];

  for (size_t i = 0; i < params->n_planes; i++) {
    VkSubresourceLayout *layout = &plane_layouts[i];
    layout->offset = params->planes[i].offset;
    layout->rowPitch = params->planes[i].stride;
  }

  VkImageDrmFormatModifierExplicitCreateInfoEXT mod_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
    .drmFormatModifier = params->modifier,
    .drmFormatModifierPlaneCount = params->n_planes,
    .pPlaneLayouts = plane_layouts,
    .pNext = &external_info,
  };

  VkImageCreateInfo img_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = &mod_info,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = get_vk_format(format->drm_format),
    .extent = {format->max_width, format->max_height, 1},
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
    .usage = image_usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  size_t n_mem = 1;

  int disjoint = is_dmabuf_disjoint(params->planes, params->n_planes);
  if (disjoint) {
    if (!format->supports_disjoint) {
      char *format_name = drmGetFormatName(params->drm_format);
      c_log(C_LOG_ERROR, "%s (0x%08"PRIx32") format doesn't support disjoint", format_name, params->drm_format);
      free(format_name);
      return NULL;
    }

    img_info.flags = VK_IMAGE_CREATE_DISJOINT_BIT;
    n_mem = params->n_planes;
  }

  VkImage img;
  if (vkCreateImage(vk->device.dev, &img_info, NULL, &img) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkCreateImage failed");
    return NULL; 
  }

  VkDeviceMemory dev_mems[n_mem];
  for (size_t i = 0; i < n_mem; i++) {
    VkMemoryFdPropertiesKHR memfd_props = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };

    if (vk->proc.vkGetMemoryFdPropertiesKHR(vk->device.dev, 
                                        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, 
                                            params->planes[i].fd, &memfd_props) != VK_SUCCESS) {

      c_log(C_LOG_ERROR, "vkGetMemoryFdPropertiesKHR failed");
      goto err_img;
    }

    VkImagePlaneMemoryRequirementsInfo mem_plane_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
      .planeAspect = 1 << (i + 7),
    };

    VkImageMemoryRequirementsInfo2 mem_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .image = img,
    };

    if (disjoint)
      mem_info.pNext = &mem_plane_info;


    VkMemoryDedicatedRequirements d_reqs = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
    };

    VkMemoryRequirements2 mem_reqs = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
      .pNext = &d_reqs,
    };

    vkGetImageMemoryRequirements2(vk->device.dev, &mem_info, &mem_reqs);

    int mem_idx = find_mem_type(vk->device.phdev, mem_reqs.memoryRequirements.memoryTypeBits & memfd_props.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_idx == -1) {
      c_log(C_LOG_ERROR, "failed to find valid memory type index");
      goto err_img;
    }

    int dup_fd = fcntl(params->planes[i].fd, F_DUPFD_CLOEXEC, 0);
    if (dup_fd == -1) {
      c_log(C_LOG_ERROR, "failed to fcntl(F_DUPFD_CLOEXEC): %s", strerror(errno));
      goto err_img;
    }

    VkMemoryDedicatedAllocateInfo d_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .image = img,
    };

    VkImportMemoryFdInfoKHR import_fd_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      .fd = dup_fd,
    };

    if (d_reqs.requiresDedicatedAllocation)
      import_fd_info.pNext = &d_info;

    VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .memoryTypeIndex = mem_idx,
      .allocationSize = mem_reqs.memoryRequirements.size,
      .pNext = &import_fd_info,
    };

    if (vkAllocateMemory(vk->device.dev, &alloc_info, NULL, &dev_mems[i]) != VK_SUCCESS) {
      c_log(C_LOG_ERROR, "vkAllocateMemory failed");
      goto err_fd;
    }


    VkBindImageMemoryInfo binfo = {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = img,
      .memory = dev_mems[i],
      .memoryOffset = 0,
    };

    VkBindImagePlaneMemoryInfo pinfo = {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
      .planeAspect = mem_plane_info.planeAspect,
    };

    if (disjoint) {
      binfo.pNext = &pinfo;
    }

    if (vkBindImageMemory2(vk->device.dev, 1, &binfo) != VK_SUCCESS) {
      c_log(C_LOG_ERROR, "vkBindImageMemory2 failed");
      goto err_fd;
    };

    continue;

  err_fd:
    close(dup_fd);
    goto err_img;

  }

  return img;

err_img:
    vkDestroyImage(vk->device.dev, img, NULL);
    for (size_t i = 0; i < n_mem; i++) {
      vkFreeMemory(vk->device.dev, dev_mems[i], NULL);  
    }
    return NULL;

};

void c_vulkan_free(struct c_vulkan *vk) {
  if (vk->device.dev)
    vkDestroyDevice(vk->device.dev, NULL);
  
  if (vk->instance) 
    vkDestroyInstance(vk->instance, NULL);

  free(vk);
}

struct c_vulkan *c_vulkan_init(int drm_fd) {
  struct c_vulkan *vk = calloc(1, sizeof(*vk));
  if (!vk) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }

  if (c_vulkan_instance_create(&vk->instance) == -1) goto err;
  if (c_vulkan_device_create(vk, drm_fd) == -1) goto err;

  return vk;

err:
  c_vulkan_free(vk);
  return NULL;
}
