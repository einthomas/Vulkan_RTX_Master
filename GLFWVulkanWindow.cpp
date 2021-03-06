// Based on https://vulkan-tutorial.com

#include "GLFWVulkanWindow.h"
#include "vulkanutil.h"

#include "Renderer.h"

float GLFWVulkanWindow::cameraSpeed = 200.0f;

void GLFWVulkanWindow::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Tell GLFW to not create an OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void GLFWVulkanWindow::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createCommandPool();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
}

void GLFWVulkanWindow::initRenderer() {
    renderer = new VulkanRenderer(this);
    renderer->startVisibilityThread();
}

void GLFWVulkanWindow::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
}

void GLFWVulkanWindow::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available");
    }

    // Optional application info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    std::vector<const char *> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};
    instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    createInfo.enabledExtensionCount = (uint32_t) instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    // Get list of supported extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    // Get extension details
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    // Print supported extensions
    /*
    std::cout << "available extensions:" << std::endl;
    std::cout << extensionCount << std::endl;
    for (const auto& extension : extensions) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }
    */

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance");
    }
}

bool GLFWVulkanWindow::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

/*
 * Select suitable GPU
 */
void GLFWVulkanWindow::pickPhysicalDevice() {
    // Get number of physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support");
    }

    // Get physical devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            //msaaSamples = VulkanUtil::getMaxUsableMSAASampleCount(physicalDevice);
            msaaSamples = VK_SAMPLE_COUNT_1_BIT;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU");
    }
}

bool GLFWVulkanWindow::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // Check if there are supported image formats and presentation modes
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupportDetails.formats.empty() && !swapChainSupportDetails.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

/*
 * Check if all required extensions are available on the passed physical device
 */
bool GLFWVulkanWindow::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices GLFWVulkanWindow::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && !indices.graphicsFamily.has_value()) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport && !indices.presentFamily.has_value()) {
            indices.presentFamily = i;
        }

        // Find a separate compute queue family
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT && !indices.computeFamily.has_value() &&
            i != indices.graphicsFamily && i != indices.presentFamily
        ) {
            indices.computeFamily = i;
        }

        // Find a separate transfer queue family
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT && !indices.transferFamily.has_value() &&
            i != indices.graphicsFamily && i != indices.presentFamily && i != indices.computeFamily
        ) {
            indices.transferFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
        i++;
    }

    return indices;
}

void GLFWVulkanWindow::createLogicalDevice() {
    // Create a queue for all queue families
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value(), indices.transferFamily.value() };
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Specify device features
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.fillModeNonSolid = VK_TRUE;      // WIREFRAME
    deviceFeatures.wideLines = VK_TRUE;
    deviceFeatures.multiViewport = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;

    // Create logical device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device");
    }

    // Get queue handles
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);


    VkPhysicalDeviceIDProperties vkPhysicalDeviceIDProperties = {};
    vkPhysicalDeviceIDProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    vkPhysicalDeviceIDProperties.pNext = NULL;

    VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2 = {};
    vkPhysicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkPhysicalDeviceProperties2.pNext = &vkPhysicalDeviceIDProperties;

    PFN_vkGetPhysicalDeviceProperties2 fpGetPhysicalDeviceProperties2;
    fpGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2");
    if (fpGetPhysicalDeviceProperties2 == NULL) {
        throw std::runtime_error("Vulkan: Proc address for \"vkGetPhysicalDeviceProperties2KHR\" not found.\n");
    }

    fpGetPhysicalDeviceProperties2(physicalDevice, &vkPhysicalDeviceProperties2);

    memcpy(deviceUUID.data(), vkPhysicalDeviceIDProperties.deviceUUID,  VK_UUID_SIZE);
}

void GLFWVulkanWindow::createSwapChain() {
    SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupportDetails.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupportDetails.presentModes);
    swapChainImageSize = chooseSwapExtent(swapChainSupportDetails.capabilities);

    // Specify min number of image in the swap chain that can be rendered to
    imageCount = swapChainSupportDetails.capabilities.minImageCount;
    if (swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.maxImageCount) {
        imageCount = swapChainSupportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainImageSize;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    if (indices.graphicsFamily != indices.presentFamily) {
        // If the graphics queue family is different from the presentation queue family, draw images from
        // the graphics queue and submit them on the presentation queue
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform;    // Apply no special pre-transformation
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;   // Clip pixels that are occluded by other windows
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = swapChainImageSize;
}

SwapChainSupportDetails GLFWVulkanWindow::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Query supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // Query supported presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

/*
 * Choose SRGB and B8G8R8A8 format
 */
VkSurfaceFormatKHR GLFWVulkanWindow::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR GLFWVulkanWindow::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

/*
 * Set the resolution of the swap chain images.
 */
VkExtent2D GLFWVulkanWindow::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { WIDTH, HEIGHT };
        // Clamp actualExtent
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

/*
 * Create an image view for every image in the swap chain.
 */
void GLFWVulkanWindow::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // Image will be used as color target without any mipmapping levels or multiple layers
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view");
        }
    }
}

void GLFWVulkanWindow::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear framebuffer before rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;    // No stencil buffer is used
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;   // No stencil buffer is used
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // We don't care about the layout before rendering
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Multisamples images cannot be presented directly
    // Describe subpasses (may be used for multiple post-processing subpasses during a single render pass)
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve = {};    // The multisampled image is resolved to this attachment
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorAttachmentResolveReference = {};
    colorAttachmentResolveReference.attachment = 2;
    colorAttachmentResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
    subpassDescription.pResolveAttachments = &colorAttachmentResolveReference;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {
        colorAttachment, depthAttachment, colorAttachmentResolve
    };
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass");
    }
}

/*
 * Create a framebuffer for all image (views) of the swap chain.
 */
void GLFWVulkanWindow::createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImageView,
            depthImageView,
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());;
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer");
        }
    }
}

void GLFWVulkanWindow::mainLoop() {
    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, GLFWVulkanWindow::keyCallback);

    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        deltaTime = currentTime - lastFrame;
        lastFrame = currentTime;

        glfwPollEvents();

        float cameraSpeed = this->cameraSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            renderer->cameraPos += cameraSpeed * renderer->cameraForward;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            renderer->cameraPos -= cameraSpeed * renderer->cameraForward;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            renderer->cameraPos += glm::normalize(glm::cross(renderer->cameraForward, renderer->cameraUp)) * cameraSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            renderer->cameraPos -= glm::normalize(glm::cross(renderer->cameraForward, renderer->cameraUp)) * cameraSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            renderer->cameraPos -= renderer->cameraUp * cameraSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            renderer->cameraPos += renderer->cameraUp * cameraSpeed;
        }

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }
        renderer->startNextFrame(imageIndex, swapChainFramebuffers[imageIndex], commandBuffers[imageIndex], renderPass);
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Wait on imageAvailableSemaphore in the stage that write to the color attachment
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];   // Submit command buffer that binds the current swap chain image

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; // Semaphore to wait on before presenting

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;   // Swapchain to present images to

        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);
}

void GLFWVulkanWindow::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = VulkanUtil::findQueueFamilies(physicalDevice, VK_QUEUE_GRAPHICS_BIT);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void GLFWVulkanWindow::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void GLFWVulkanWindow::createCommandBuffers() {
    commandBuffers.resize(swapChainFramebuffers.size());    // One command buffer per swapchain image

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void GLFWVulkanWindow::createDepthResources() {
    VkFormat format = findDepthFormat();
    VulkanUtil::createImage(
        physicalDevice, device, swapChainExtent.width, swapChainExtent.height, msaaSamples, format,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory
    );
    depthImageView = VulkanUtil::createImageView(
        device, depthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

void GLFWVulkanWindow::createColorResources() {
    VkFormat format = swapChainImageFormat;
    VulkanUtil::createImage(
        physicalDevice, device, swapChainExtent.width, swapChainExtent.height, msaaSamples, format,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory
    );
    colorImageView = VulkanUtil::createImageView(
        device, colorImage, format, VK_IMAGE_ASPECT_COLOR_BIT
    );
}

VkFormat GLFWVulkanWindow::findSupportedFormat(
    const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features
) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat GLFWVulkanWindow::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void GLFWVulkanWindow::cleanup() {
    vkDestroyImageView(device, colorImageView, nullptr);
    vkDestroyImage(device, colorImage, nullptr);
    vkFreeMemory(device, colorImageMemory, nullptr);

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    // Destroy framebuffers
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // Destroy graphics pipeline
    vkDestroyPipeline(device, pipeline, nullptr);

    // Destroy pipeline layout
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // Destroy render pass
    vkDestroyRenderPass(device, renderPass, nullptr);

    // Destroy image views
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    // Destroy swap chain (swap chain images don't have to be destroyed explicitly
    // since they are created by the swap chain)
    vkDestroySwapchainKHR(device, swapChain, nullptr);

    // Destroy virtual device
    vkDestroyDevice(device, nullptr);

    // Destroy window surface
    vkDestroySurfaceKHR(instance, surface, nullptr);

    // Destroy Vulkan instance
    vkDestroyInstance(instance, nullptr);

    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}

void GLFWVulkanWindow::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
    // https://learnopengl.com/
    GLFWVulkanWindow *vulkanWindow = static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window));

    if (vulkanWindow->firstMouse) {
        vulkanWindow->lastX = xpos;
        vulkanWindow->lastY = ypos;
        vulkanWindow->firstMouse = false;
    }

    float xoffset = xpos - vulkanWindow->lastX;
    float yoffset = vulkanWindow->lastY - ypos;
    vulkanWindow->lastX = xpos;
    vulkanWindow->lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    vulkanWindow->yaw += xoffset;
    vulkanWindow->pitch += yoffset;

    if (vulkanWindow->pitch > 89.0f) {
        vulkanWindow->pitch = 89.0f;
    } else if (vulkanWindow->pitch < -89.0f) {
        vulkanWindow->pitch = -89.0f;
    }

    glm::vec3 direction;
    direction.x = cos(glm::radians(vulkanWindow->yaw)) * cos(glm::radians(vulkanWindow->pitch));
    direction.y = sin(glm::radians(vulkanWindow->pitch));
    direction.z = sin(glm::radians(vulkanWindow->yaw)) * cos(glm::radians(vulkanWindow->pitch));
    vulkanWindow->renderer->cameraForward = glm::normalize(direction);
}

void GLFWVulkanWindow::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_V) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->toggleShadedRendering();
        }
        if (key == GLFW_KEY_G) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->toggleViewCellRendering();
        }
        if (key == GLFW_KEY_T) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->toggleRayRendering();
        }
        if (key == GLFW_KEY_R) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->showMaxErrorDirection();
        }

        if (key == GLFW_KEY_ESCAPE) {
            if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwSetCursorPosCallback(window, GLFWVulkanWindow::mouseCallback);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursorPosCallback(window, nullptr);
            }
        }

        if (key == GLFW_KEY_C) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->nextViewCell();
        }
        if (key == GLFW_KEY_F) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->nextCorner();
        }
        if (key == GLFW_KEY_N) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->alignCameraWithViewCellNormal();
        }

        if (key == GLFW_KEY_Q) {
            static_cast<GLFWVulkanWindow*>(glfwGetWindowUserPointer(window))->renderer->printCamera();
        }

        if (key == GLFW_KEY_1) {
            cameraSpeed *= 0.5f;
        }
        if (key == GLFW_KEY_2) {
            cameraSpeed *= 1.5f;
        }
    }
}
