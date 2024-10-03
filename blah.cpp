#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include<vulkan/vulkan_beta.h>

#include <iostream>        // for reporting and propagating errors
#include <stdexcept>       // for reporting and propagating errors
#include <cstdlib>         // provides EXIT_SUCCESS and EXIT_FAILURE macros
#include <vector>
#include <cstring>         // used for strcmp()
#include <optional>        // used in struct QueueFamilyIndices

const uint32_t WIDTH = 800;  // screen width
const uint32_t HEIGHT = 600; // screen height

// enable Vulkan SDK standard diagnostics (validation) layers
const std::vector<const char*> validationLayers =
{
  "VK_LAYER_KHRONOS_validation"
};

// if compiled in debug mode: enable validation layers
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif


// look-up proxy function required for setupDebugMessenger()
VkResult CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
  const VkAllocationCallbacks* pAllocator,
  VkDebugUtilsMessengerEXT* pDebugMessenger)
{
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) 
  {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  }
  else
  {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}


// proxy function required for cleanup()
void DestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT debugMessenger,
  const VkAllocationCallbacks* pAllocator)
{
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) 
  {
    func(instance, debugMessenger, pAllocator);
  }
}


struct QueueFamilyIndices
{
  std::optional<uint32_t> graphicsFamily;

  bool isComplete()
  {
    return graphicsFamily.has_value();
  }
};


class HelloTriangleApplication 
{
public:
  void run()
  {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  GLFWwindow* window; // window variable
  VkInstance instance; // vulkan instance variable
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // graphics card
  VkDevice device; // logical device
  VkQueue graphicsQueue;


  void initWindow()
  {
    glfwInit(); // initialize GLFW

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // do not create OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // disable resizing windows (for now)

    // window initizialize. param4: optionally specify monitor. param5: only relevant to OpenGL
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }


  void initVulkan()
  {
    createInstance(); // initialize Vulkan library
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
  }


  void mainLoop()
  {
    while (!glfwWindowShouldClose(window)) // keep application running until window closed or error 
    {
      glfwPollEvents();
    }
  }


  void cleanup()
  {
    vkDestroyDevice(device, nullptr);

    if (enableValidationLayers)
    {
      DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
  }


  void createInstance()
  {
    // enable validation layers 
    if (enableValidationLayers && !checkValidationLayerSupport()) 
    {
      throw std::runtime_error("validation layers requested, but not available!");
    }

    // (optional) create struct with info about application
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // tell Vulkan driver which global extensions and validation layers to use
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // get required extensions for validation layers message callback
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;  // macOS specific fix

    // include validation layer names (if enabled) in VkInstanceCreateInfo instantiation
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)
    {
      createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
      createInfo.ppEnabledLayerNames = validationLayers.data();

      populateDebugMessengerCreateInfo(debugCreateInfo);
      createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    }
    else
    {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
    }

    // create vulkan instance and check if created successfully
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) 
    {
      throw std::runtime_error("failed to create instance!");
    }
  }


  // needed to debug any issues in the vkCreateInstance and vkDestroyInstance calls
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) 
  {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
  }


  void setupDebugMessenger()
  {
    if (!enableValidationLayers) return;

    // details about the messenger and its callback
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) 
    {
      throw std::runtime_error("failed to set up debug messenger!");
    }
  }


  // select graphics card
  // NOTE: possible to select "best" graphics card (see code in obsidian notes)
  void pickPhysicalDevice()
  {
    // list available graphics cards
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
      throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // allocate array to store available devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // find suitable devices
    for (const auto& device : devices) 
    {
      if (isDeviceSuitable(device)) 
      {
        physicalDevice = device;
        break;
      }
    }

    if (physicalDevice == VK_NULL_HANDLE) 
    {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
  }


  void createLogicalDevice() 
  {
    // describes the number of queues for a single queue family
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;

    // assign priorities to queues to influence the scheduling of command buffer execution
    // between 0.0 and 1.0
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // specify the set of device features to use
    VkPhysicalDeviceFeatures deviceFeatures{};

    // TEST: device extensions
    std::vector<const char*> deviceExtensions;
    if (enableValidationLayers) 
    {
      deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME); // needed for macOS
    }

    // create logical device
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());;
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) 
    {
      createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
      createInfo.ppEnabledLayerNames = validationLayers.data();
    } 
    else 
    {
      createInfo.enabledLayerCount = 0;
    }

    // instantiate logical device
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) 
    {
      throw std::runtime_error("failed to create logical device!");
    }

    // retrieve queue handles for each queue family
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  }

  // find suitable device
  bool isDeviceSuitable(VkPhysicalDevice device) 
  {
    QueueFamilyIndices indices = findQueueFamilies(device);

    return indices.isComplete();
  }


  // queue family lookup function. check which queue families are supported by the device
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) 
  {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT
    int i = 0;
    for (const auto& queueFamily : queueFamilies) 
    {
      if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
      {
        indices.graphicsFamily = i;
      }

      if (indices.isComplete()) 
      {
        break;
      }

      i++;
    }

    return indices;
  }

  // validation layer message callback
  // return the required list of extensions based on whether validation layers are enabled or not
  // NOTE: `extensions` are for VkInstance
  std::vector<const char*> getRequiredExtensions() 
  {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) 
    {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

      // macOS specific fix
      extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // required for VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    }

    return extensions;
  }


  // check if all of the requested validation layers are available
  bool checkValidationLayerSupport()
  {
    // list all available layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    //return false;

    // check if all of the layers in validationLayers exist in the availableLayers list
    for (const char* layerName : validationLayers) 
    {
      bool layerFound = false;

      for (const auto& layerProperties : availableLayers) 
      {
        if (strcmp(layerName, layerProperties.layerName) == 0) 
        {
          layerFound = true;
          break;
        }
      }

      if (!layerFound) 
      {
        return false;
      }
    }

    return true;
  }


  // debug callback function
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // specifies the severity of the message
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // message details
    void* pUserData) 
  {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
  }
};


int main()
{
  HelloTriangleApplication app;

  try
  {
    app.run();
  } 
  catch (const std::exception& e) 
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
