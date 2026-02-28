#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>

#include "render/vulkan.h"
#include "util/log.h"
#include "util/helpers.h"


static int check_ext(const char *ext, VkExtensionProperties *exts, uint32_t exts_n) {
  for (uint32_t i = 0; i < exts_n; i++) {
    if (STREQ(exts[i].extensionName, ext)) return 1;
  }
  return 0;
}

__attribute__((unused)) static VkBool32 vulkan_debug(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                             VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                            void*                                       pUserData) {
  return VK_TRUE;
};

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


static VkPhysicalDevice c_vulkan_get_phdev(VkInstance instance, int drm_fd) {
  int res;
  uint32_t dev_n = 0;
  if (vkEnumeratePhysicalDevices(instance, &dev_n, NULL) != VK_SUCCESS || dev_n == 0) {
    c_log(C_LOG_ERROR, "vkEnumeratePhysicalDevices failed or number of physical devices is 0");
    return NULL;
  }

  VkPhysicalDevice devs[dev_n];
  if ((res = vkEnumeratePhysicalDevices(instance, &dev_n, devs)) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkEnumeratePhysicalDevices failed");
    return NULL;
  }
  assert(res != VK_INCOMPLETE);

  for (size_t i = 0; i < dev_n; i++) {
    VkPhysicalDevice phdev = devs[i];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phdev, &props);
    phdev_log(&props);

    uint32_t ext_count = 0;

    if (vkEnumerateDeviceExtensionProperties(phdev, NULL, &ext_count, NULL) != VK_SUCCESS || ext_count == 0) {
      c_log(C_LOG_ERROR, "vkEnumerateDeviceExtensionProperties failed or number of device extensions is 0");
      return NULL;
    }
    
    VkExtensionProperties exts[ext_count];
    if (vkEnumerateDeviceExtensionProperties(phdev, NULL, &ext_count, exts) != VK_SUCCESS) {
      c_log(C_LOG_ERROR, "vkEnumerateDeviceExtensionProperties failed");
      return NULL;
    }

    if (!check_ext(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME, exts, ext_count)) {
      c_log(C_LOG_INFO, "VK_EXT_physical_device_drm not supported");
      return NULL;
    }
    int driver_prop_support = check_ext(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, exts, ext_count);

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

    for (size_t i = 0; i < ext_count; i++) {
      c_log(C_LOG_INFO, "Vulkan device extension: %s", exts[i].extensionName);
    }

  }

  c_log(C_LOG_ERROR, "Couldn't find physical device matching opened DRM device");
  return NULL;
}

static int c_vulkan_instance_create(VkInstance *instance) {
  PFN_vkEnumerateInstanceVersion vkEnumInstanceVer = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
  uint32_t ver;
  if (vkEnumInstanceVer(&ver) != VK_SUCCESS || ver < VK_VERSION_1_1) {
    c_log(C_LOG_ERROR, "Unsupported vulkan version");
    return -1;
  }

  uint32_t ext_count = 0;
  if (vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL) != VK_SUCCESS || ext_count == 0) {
    c_log(C_LOG_ERROR, "vkEnumerateInstanceExtensionProperties failed or number of instance extensions is 0");
    return -1;
  }
  
  VkExtensionProperties exts[ext_count];
  if (vkEnumerateInstanceExtensionProperties(NULL, &ext_count, exts) != VK_SUCCESS) {
    c_log(C_LOG_ERROR, "vkEnumerateInstanceExtensionProperties failed");
    return -1;
  }

  uint32_t enabled_exts_n = 0;
  const char *enabled_exts[1] = {0};

  // int debug_utils = check_ext(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, exts, ext_count);
  // if (debug_utils) {
  //   enabled_exts[enabled_exts_n++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  // }

  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pEngineName = "cuts-renderer";
  app_info.engineVersion = 1;
  app_info.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo instance_info = {0};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount = enabled_exts_n;
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

  for (size_t i = 0; i < ext_count; i++) {
    c_log(C_LOG_INFO, "Vulkan instance extensions: %s", exts[i].extensionName);
  }

  return 0;
}

void c_vulkan_free(struct c_vulkan *vk) {
  if (vk->instance) {
    vkDestroyInstance(vk->instance, NULL);
  }
  free(vk);
}

struct c_vulkan *c_vulkan_init(int drm_fd) {
  struct c_vulkan *vk = calloc(1, sizeof(*vk));
  if (!vk) {
    c_log(C_LOG_ERROR, "calloc failed");
    return NULL;
  }
  if (c_vulkan_instance_create(&vk->instance) == -1) goto err;
  if (!c_vulkan_get_phdev(vk->instance, drm_fd)) goto err;

  return vk;

err:
  c_vulkan_free(vk);
  return NULL;
}
