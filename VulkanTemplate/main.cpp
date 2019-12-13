// https://vulkan-tutorial.com/Drawing_a_triangle

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

#include "jAssert.h"
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <string>
#include <fstream>

#define MULTIPLE_FRAME 1

class HelloTriangleApplication
{
public:
	static constexpr int32_t WIDTH = 800;
	static constexpr int32_t HEIGHT= 600;

#if MULTIPLE_FRAME
	static constexpr int32_t MAX_FRAMES_IN_FLIGHT = 2;
#endif // MULTIPLE_FRAME

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NDEBUG
	static constexpr bool enableValidationLayers = false;
#else
	static constexpr bool enableValidationLayers = true;
#endif

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		// messageSeverity
		// 1. VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : 진단 메시지
		// 2. VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : 리소스 생성 정보
		// 3. VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT : 에러일 필요 없는 정보지만 버그를 낼수 있는것
		// 4. VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT : Invalid 되었거나 크래시 날수 있는 경우
		if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
		}

		// messageType
		// VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT : 사양이나 성능과 관련되지 않은 이벤트 발생
		// VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : 사양 위반이나 발생 가능한 실수가 발생
		// VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT : 불칸을 잠재적으로 최적화 되게 사용하지 않은 경우

		// pCallbackData 의 중요 데이터 3종
		// pMessage : 디버그 메시지 문장열(null 로 끝남)
		// pObjects : 이 메시지와 연관된 불칸 오브젝트 핸들 배열
		// objectCount : 배열에 있는 오브젝트들의 개수

		// pUserData
		// callback 설정시에 전달한 포인터 데이터

		std::cerr << "validation layer : " << pCallbackData->pMessage << std::endl;

		// VK_TRUE 리턴시 VK_ERROR_VALIDATION_FAILED_EXT 와 validation layer message 가 중단된다.
		// 이것은 보통 validation layer 를 위해 사용하므로, 사용자는 항상 VK_FALSE 사용.
		return VK_FALSE;
	}

	static std::vector<char> ReadFile(const std::string& filename)
	{
		// 1. std::ios::ate : 파일의 끝에서 부터 읽기 시작한다. (파일의 끝에서 부터 읽어서 파일의 크기를 얻어 올수 있음)
		// 2. std::ios::binary : 바이너리 파일로서 파일을 읽음. (text transformations 을 피함)
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!ensure(file.is_open()))
			return std::vector<char>();

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);		// 파일의 맨 첨으로 이동
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void Run()
	{
		InitVulkan();
		MainLoop();
		Cleanup();
	}

private:
	void InitWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);		// static function 밖에 안됨.(멤버함수 호출불가)
	}

	void InitVulkan()
	{
		InitWindow();

		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFrameBuffers();
		CreateCommandPool();
		CreateCommandBuffers();
		CreateSyncObjects();
	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			DrawFrame();
		}

		// Logical device 가 작업을 모두 마칠때까지 기다렸다가 Destory를 진행할 수 있게 기다림.
		vkDeviceWaitIdle(device);
	}

	void Cleanup()
	{
		CleanupSwapChain();

#if MULTIPLE_FRAME
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}
#else
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
#endif // MULTIPLE_FRAME
		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers)
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	bool CreateInstance()
	{
		// Optional
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Must
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// add extension
		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		
		// check validation layer
		if (!ensure(!(enableValidationLayers && !CheckValidationLayerSupport())))
			return false;

		// add validation layer
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebutMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

		if (!ensure(result == VK_SUCCESS))
			return false;

		return true;
	}

	bool CreateSurface()
	{
		VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
		return ensure(result == VK_SUCCESS);
	}

	bool CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (!strcmp(layerName, layerProperties.layerName))
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
	}

	std::vector<const char*> GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);	// VK_EXT_debug_utils 임

		return extensions;
	}

	void SetupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		PopulateDebutMessengerCreateInfo(createInfo);
		createInfo.pUserData = nullptr;	// optional

		VkResult result = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
		check(result == VK_SUCCESS);
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo
		, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func)
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);

		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
			func(instance, debugMessenger, pAllocator);
	}

	void PopulateDebutMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	bool PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (!ensure(deviceCount > 0))
			return false;

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (!ensure(physicalDevice != VK_NULL_HANDLE))
			return false;

		return true;
	}

	bool IsDeviceSuitable(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// 현재는 Geometry Shader 지원 여부만 판단
		if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || !deviceFeatures.geometryShader)
			return false;

		QueueFamilyIndices indices = findQueueFamilies(device);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool IsComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
				if (presentSupport)
				{
					indices.presentFamily = i;
					indices.graphicsFamily = i;
				}
				break;
			}
			++i;
		}

		return indices;
	}

	bool CreateLogicalDevice()
	{
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;			// [0.0 ~ 1.0]
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;		// 현재는 여러 스레드에서 커맨드 버퍼를 각각 만들어서 
												// 메인스레드에서 모두 한번에 제출하기 때문에 큐가 한개 이상일 필요가 없다.
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

		createInfo.pEnabledFeatures = &deviceFeatures;

		// extension
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		// 최신 버젼에서는 validation layer는 무시되지만, 오래된 버젼을 호환을 위해 vkInstance와 맞춰줌
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (!ensure(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS))
			return false;

		// 현재는 Queue가 1개 뿐이므로 QueueIndex를 0
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

		return true;
	}

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : availableExtensions)
			requiredExtensions.erase(extension.extensionName);

		return requiredExtensions.empty();
	}

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		check(availableFormats.size() > 0);
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM
				&& availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}
		return availableFormats[0];
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		// 1. VK_PRESENT_MODE_IMMEDIATE_KHR : 어플리케이션에 의해 제출된 이미지가 즉시 전송되며, 찢어짐이 있을 수도 있음.
		// 2. VK_PRESENT_MODE_FIFO_KHR : 디스플레이가 갱신될 때(Vertical Blank 라 부름), 디스플레이가 이미지를 스왑체인큐 앞쪽에서 가져간다.
		//								그리고 프로그램은 그려진 이미지를 큐의 뒤쪽에 채워넣는다.
		//								만약 큐가 가득차있다면 프로그램은 기다려야만 하며, 이것은 Vertical Sync와 유사하다.
		// 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR : 이 것은 VK_PRESENT_MODE_FIFO_KHR와 한가지만 다른데 그것은 아래와 같음.
		//								만약 어플리케이션이 늦어서 마지막 Vertical Blank에 큐가 비어버렸다면, 다음 Vertical Blank를
		//								기다리지 않고 이미지가 도착했을때 즉시 전송한다. 이것 때문에 찢어짐이 나타날 수 있다.
		// 4. VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR의 또다른 변종인데, 큐가 가득차서 어플리케이션이 블록되야 하는경우
		//								블록하는 대신에 이미 큐에 들어가있는 이미지를 새로운 것으로 교체해버린다. 이것은 트리플버퍼링을
		//								구현하는데 사용될 수 있다. 트리플버퍼링은 더블 버퍼링에 Vertical Sync를 사용하는 경우 발생하는 
		//								대기시간(latency) 문제가 현저하게 줄일 수 있다.

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		// currentExtent == UINT32_MAX 면, 창의 너비를 minImageExtent와 maxImageExtent 사이에 적절한 사이즈를 선택할 수 있음.
		// currentExtent != UINT32_MAX 면, 윈도우 사이즈와 currentExtent 사이즈가 같음.
		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		//VkExtent2D actualExtent = { WIDTH, HEIGHT };

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

		actualExtent.width = std::max<uint32_t>(capabilities.minImageExtent.width, std::min<uint32_t>(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max<uint32_t>(capabilities.minImageExtent.height, std::min<uint32_t>(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;
	}

	bool CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		// SwapChain 개수 설정
		// 최소개수로 하게 되면, 우리가 렌더링할 새로운 이미지를 얻기 위해 드라이버가 내부 기능 수행을 기다려야 할 수 있으므로 min + 1로 설정.
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		// maxImageCount가 0이면 최대 개수에 제한이 없음
		if ((swapChainSupport.capabilities.maxImageCount > 0) && (imageCount > swapChainSupport.capabilities.maxImageCount))
			imageCount = swapChainSupport.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.minImageCount = imageCount;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;			// Stereoscopic 3D application(VR)이 아니면 항상 1
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	// 즉시 스왑체인에 그리기 위해서 이걸로 설정
																		// 포스트 프로세스 같은 처리를 위해 별도의 이미지를 만드는 것이면
																		// VK_IMAGE_USAGE_TRANSFER_DST_BIT 으로 하면됨.

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		// 그림은 Graphics Queue Family와 Present Queue Family가 다른경우 아래와 같이 동작한다.
		// - 이미지를 Graphics Queue에서 가져온 스왑체인에 그리고, Presentation Queue에 제출
		// 1. VK_SHARING_MODE_EXCLUSIVE : 이미지를 한번에 하나의 Queue Family 가 소유함. 소유권이 다른곳으로 전달될때 명시적으로 전달 해줘야함.
		//								이 옵션은 최고의 성능을 제공한다.
		// 2. VK_SHARING_MODE_CONCURRENT : 이미지가 여러개의 Queue Family 로 부터 명시적인 소유권 절달 없이 사용될 수 있다.
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;		// Optional
			createInfo.pQueueFamilyIndices = nullptr;	// Optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;		// 스왑체인에 회전이나 flip 처리 할 수 있음.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;					// 알파채널을 윈도우 시스템의 다른 윈도우와 블랜딩하는데 사용해야하는지 여부
																						// VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 는 알파채널 무시
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;			// 다른윈도우에 가려져서 보이지 않는 부분을 그리지 않을지에 대한 여부 VK_TRUE 면 안그림

		// 화면 크기 변경등으로 스왑체인이 다시 만들어져야 하는경우 여기에 oldSwapchain을 넘겨준다.
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (!ensure(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) == VK_SUCCESS))
			return false;

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		return true;
	}

	bool CreateImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;

			// RGBA 컴포넌트들에게 컬러 채널을 매핑할 수 있다.
			// 예를들어 VK_COMPONENT_SWIZZLE_R 을 모든 채널에 넣으면 R을 사용한 흑백 텍스쳐를 나타낼 수 있음.
			// 현재는 기본인 VK_COMPONENT_SWIZZLE_IDENTITY 를 설정해준다.
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			// SubresourceRange에 이미지의 목적과 이미지의 어떤 파트에 접근할 것인지를 명세한다.
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (!ensure(vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) == VK_SUCCESS))
				return false;
		}

		return true;
	}

	bool CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;	// 멀티샘플링은 하지 않고 있으므로 1개의 샘플만

		// 아래 2가지 옵션은 렌더링 전, 후에 attachment에 있는 데이터에 무엇을 할지 결정하는 부분.
		// 1). loadOp
		//		- VK_ATTACHMENT_LOAD_OP_LOAD : attachment에 있는 내용을 그대로 유지
		//		- VK_ATTACHMENT_LOAD_OP_CLEAR : attachment에 있는 내용을 constant 모두 값으로 설정함.
		//		- VK_ATTACHMENT_LOAD_OP_DONT_CARE : attachment에 있는 내용에 대해 어떠한 것도 하지 않음. 정의되지 않은 상태.
		// 2). storeOp
		//		- VK_ATTACHMENT_STORE_OP_STORE : 그려진 내용이 메모리에 저장되고 추후에 읽어질 수 있음.
		//		- VK_ATTACHMENT_STORE_OP_DONT_CARE : 렌더링을 수행한 후에 framebuffer의 내용이 어떻게 될지 모름(정의되지 않음).
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		// Texture나 Framebuffer의 경우 VkImage 객체로 특정 픽셀 형식을 표현함.
		// 그러나 메모리의 픽셀 레이아웃은 이미지로 수행하려는 작업에 따라서 변경될 수 있음.
		// 그래서 여기서는 수행할 작업에 적절한 레이아웃으로 image를 전환시켜주는 작업을 함.
		// 주로 사용하는 layout의 종류
		// 1). VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : Color attachment의 이미지
		// 2). VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : Swapchain으로 제출된 이미지
		// 3). VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Memory copy 연산의 Destination으로 사용된 이미지
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			// RenderPass가 시작되기 전에 어떤 Image 레이아웃을 가지고 있을지 여부를 지정
																			// VK_IMAGE_LAYOUT_UNDEFINED 은 이전 이미지 레이아웃을 무시한다는 의미.
																			// 주의할 점은 이미지의 내용이 보존되지 않습니다. 그러나 현재는 이미지를 Clear할 것이므로 보존할 필요가 없음.
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// RenderPass가 끝날때 자동으로 전환될 Image 레이아웃을 정의함
																			// 우리는 렌더링 결과를 스왑체인에 제출할 것이기 때문에 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 사용

		// Subpasses
		// 하나의 렌더링패스에는 여러개의 서브렌더패스가 존재할 수 있다. 예를 들어서 포스트프로세스를 처리할때 여러 포스트프로세스를 
		// 서브패스로 전달하여 하나의 렌더패스로 만들 수 있다. 이렇게 하면 불칸이 Operation과 메모리 대역폭을 아껴쓸 수도 있으므로 성능이 더 나아진다.
		// 포스프 프로세스 처리들 사이에 (GPU <-> Main memory에 계속해서 Framebuffer 내용을 올렸다 내렸다 하지 않고 계속 GPU에 올려두고 처리할 수 있음)
		// 현재는 1개의 서브패스만 사용함.
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;											// VkAttachmentDescription 배열의 인덱스
																					// fragment shader의 layout(location = 0) out vec4 outColor; 에 location=? 에 인덱스가 매칭됨.
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// 서브패스에서 어떤 이미지 레이아웃을 사용할것인지를 명세함.
																					// Vulkan에서 이 서브패스가 되면 자동으로 Image 레이아웃을 이것으로 변경함.
																					// 우리는 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 으로 설정하므로써 color attachment로 사용

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		// 1. Subpasses 들의 이미지 레이아웃 전환은 자동적으로 일어나며, 이런 전환은 subpass dependencies 로 관리 됨.
		// 현재 서브패스를 1개 쓰고 있지만 서브패스 앞, 뒤에 암묵적인 서브페이스가 있음.
		// 2. 2개의 내장된 dependencies 가 렌더패스 시작과 끝에 있음.
		//		- 렌더패스 시작에 있는 경우 정확한 타이밍에 발생하지 않음. 파이프라인 시작에서 전환이 발생한다고 가정되지만 우리가 아직 이미지를 얻지 못한 경우가 있을 수 있음.
		//		 이 문제를 해결하기 위해서 2가지 방법이 있음.
		//			1). imageAvailableSemaphore 의 waitStages를 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT 로 변경하여, 이미지가 사용가능하기 전에 렌더패스가 시작되지 못하도록 함.
		//			2). 렌더패스가 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 스테이지를 기다리게 함. (여기선 이거 씀)
		VkSubpassDependency dependency = {};
		// VK_SUBPASS_EXTERNAL : implicit subpass 인 before or after render pass 에서 발생하는 subpass
		// 여기서 dstSubpass 는 0으로 해주는데 현재 1개의 subpass 만 만들어서 0번 index로 설정함.
		// 디펜던시 그래프에서 사이클 발생을 예방하기 위해서 dstSubpass는 항상 srcSubpass 더 높아야한다. (VK_SUBPASS_EXTERNAL 은 예외)
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		// 수행되길 기다려야하는 작업과 이런 작업이 수행되야할 스테이지를 명세하는 부분.
		// 우리가 이미지에 접근하기 전에 스왑체인에서 읽기가 완료될때 까지 기다려야하는데, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 에서 가능.
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		// 기다려야하는 작업들은 Color Attachment 스테이지에 있고 Color Attachment를 읽고 쓰는 것과 관련되어 있음.
		// 이 설정으로 실제 이미지가 필요할때 혹은 허용될때까지 전환이 발생하는것을 방지함.
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (!ensure(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS))
			return false;

		return true;
	}

	bool CreateGraphicsPipeline()
	{
		// 1. Create Shader
		auto vertShaderCode = ReadFile("Shaders/vert.spv");
		auto fragShaderCode = ReadFile("Shaders/frag.spv");

		VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		// pSpecializationInfo 을 통해 쉐이더에서 사용하는 상수값을 설정해줄 수 있음. 이 상수 값에 따라 if 분기문에 없어지거나 하는 최적화가 일어날 수 있음.
		//vertShaderStageInfo.pSpecializationInfo

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStage[] = { vertShaderStageInfo, fragShaderStageInfo };
		// VkShaderModule 은 이 함수의 끝에서 Destroy 시킴

		// 2. Vertex Input
		// 1). Bindings : 데이터 사이의 간격과 버택스당 or 인스턴스당(인스턴싱 사용시) 데이터인지 여부
		// 2). Attribute descriptions : 버택스 쉐이더 전달되는 attributes 의 타입. 그것을 로드할 바인딩과 오프셋
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;	// Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;	// Optional

		// Input Assembly
		// primitiveRestartEnable 옵션이 VK_TRUE 이면, 인덱스버퍼의 특수한 index 0xFFFF or 0xFFFFFFFF 를 사용해서 line 과 triangle topology mode를 사용할 수 있다.
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// 3. Viewports and scissors
		// SwapChain의 이미지 사이즈가 이 클래스에 정의된 상수 WIDTH, HEIGHT와 다를 수 있다는 것을 기억 해야함.
		// 그리고 Viewports 사이즈는 SwapChain 크기 이하로 마추면 됨.
		// [minDepth ~ maxDepth] 는 [0.0 ~ 1.0] 이며 특별한 경우가 아니면 이 범위로 사용하면 된다.
		// Scissor Rect 영역을 설정해주면 영역내에 있는 Pixel만 레스터라이저를 통과할 수 있으며 나머지는 버려(Discard)진다.
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		// Viewport와 Scissor 를 여러개 설정할 수 있는 멀티 뷰포트를 사용할 수 있기 때문임
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// 4. Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;		// 이 값이 VK_TRUE 면 Near나 Far을 벗어나는 영역을 [0.0 ~ 1.0]으로 Clamp 시켜줌.(쉐도우맵에서 유용)
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	// 이 값이 VK_TRUE 면, 레스터라이저 스테이지를 통과할 수 없음. 즉 Framebuffer 로 결과가 넘어가지 않음.
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	// FILL, LINE, POINT 세가지가 있음
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;			// 쉐도우맵 용
		rasterizer.depthBiasConstantFactor = 0.0f;		// Optional
		rasterizer.depthBiasClamp = 0.0f;				// Optional
		rasterizer.depthBiasSlopeFactor = 0.0f;			// Optional

		// 5. Multisampling
		// 현재는 사용하지 않음
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;				// Optional
		multisampling.pSampleMask = nullptr;				// Optional
		multisampling.alphaToCoverageEnable = VK_FALSE;		// Optional
		multisampling.alphaToOneEnable = VK_FALSE;			// Optional

		// 6. Depth and stencil testing
		// 현재는 사용하지 않음
		// VkPipelineDepthStencilStateCreateInfo depthStencil = {};

		// 7. Color blending
		// 2가지 방식의 blending 이 있음
		// 1). 기존과 새로운 값을 섞어서 최종색을 만들어낸다.
		// 2). 기존과 새로운 값을 비트 연산으로 결합한다.
		/*
			// Blend 가 켜져있으면 대략 이런식으로 동작함
			if (blendEnable)
			{
				finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
				finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
			} else
			{
				finalColor = newColor;
			}

			finalColor = finalColor & colorWriteMask;
		*/
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;						// 현재 VK_FALSE라 새로운 값이 그대로 사용됨
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;				// Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;				// Optional

		// 일반적인 경우 블랜드는 아래와 같은 식을 사용한다.
		/*
			finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
			finalColor.a = newAlpha.a;
		*/
		// 이렇게 하려면 아래와 같이 설정해야 함.
		//colorBlendAttachment.blendEnable = VK_TRUE;
		//colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		//colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		//colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		// 2가지 blending 방식을 모두 끌 수도있는데 그렇게 되면, 변경되지 않은 fragment color 값이 그대로 framebuffer에 쓰여짐.
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

		// 2). 기존과 새로운 값을 비트 연산으로 결합한다.
		colorBlending.logicOpEnable = VK_FALSE;			// 모든 framebuffer에 사용하는 blendEnable을 VK_FALSE로 했다면 자동으로 logicOpEnable은 꺼진다.
		colorBlending.logicOp = VK_LOGIC_OP_COPY;		// Optional

		// 1). 기존과 새로운 값을 섞어서 최종색을 만들어낸다.
		colorBlending.attachmentCount = 1;						// framebuffer 개수에 맞게 설정해준다.
		colorBlending.pAttachments = &colorBlendAttachment;

		colorBlending.blendConstants[0] = 0.0f;		// Optional
		colorBlending.blendConstants[1] = 0.0f;		// Optional
		colorBlending.blendConstants[2] = 0.0f;		// Optional
		colorBlending.blendConstants[3] = 0.0f;		// Optional

		// 8. Dynamic state
		// 이전에 정의한 state에서 제한된 범위 내에서 새로운 pipeline을 만들지 않고 state를 변경할 수 있음. (viewport size, line width, blend constants)
		// 이것을 하고싶으면 Dynamic state를 만들어야 함. 이경우 Pipeline에 설정된 값은 무시되고, 매 렌더링시에 새로 설정해줘야 함.
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		// 현재는 사용하지 않음.
		//VkPipelineDynamicStateCreateInfo dynamicState = {};
		//dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		//dynamicState.dynamicStateCount = 2;
		//dynamicState.pDynamicStates = dynamicStates;

		// 9. Pipeline layout
		// 쉐이더에 전달된 Uniform 들을 명세하기 위한 오브젝트
		// 이 오브젝트는 프로그램 실행동안 계속해서 참조되므로 cleanup 에서 제거해줌
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;				// Optional
		pipelineLayoutInfo.pSetLayouts = nullptr;			// Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0;		// Optional		// 쉐이더에 값을 constant 값을 전달 할 수 있음. 이후에 배움
		pipelineLayoutInfo.pPushConstantRanges = nullptr;	// Optional
		if (!ensure(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS))
		{
			vkDestroyShaderModule(device, fragShaderModule, nullptr);
			vkDestroyShaderModule(device, vertShaderModule, nullptr);
			return false;
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		// Shader stage
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStage;

		// Fixed-function stage
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;		// Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr;			// Optional
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;		// index of subpass

		// Pipeline을 상속받을 수 있는데, 아래와 같은 장점이 있다.
		// 1). 공통된 내용이 많으면 파이프라인 설정이 저렴하다.
		// 2). 공통된 부모를 가진 파이프라인 들끼리의 전환이 더 빠를 수 있다.
		// BasePipelineHandle 이나 BasePipelineIndex 가 로 Pipeline 내용을 상속받을 수 있다.
		// 이 기능을 사용하려면 VkGraphicsPipelineCreateInfo의 flags 에 VK_PIPELINE_CREATE_DERIVATIVE_BIT  가 설정되어있어야 함
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;		// Optional
		pipelineInfo.basePipelineIndex = -1;					// Optional

		// 여기서 두번째 파라메터 VkPipelineCache에 VK_NULL_HANDLE을 넘겼는데, VkPipelineCache는 VkPipeline을 저장하고 생성하는데 재사용할 수 있음.
		// 또한 파일로드 저장할 수 있어서 다른 프로그램에서 다시 사용할 수도있다. VkPipelineCache를 사용하면 VkPipeline을 생성하는 시간을 
		// 굉장히 빠르게 할수있다. (듣기로는 대략 1/10 의 속도로 생성해낼 수 있다고 함)
		if (!ensure(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) == VK_SUCCESS))
		{
			vkDestroyShaderModule(device, fragShaderModule, nullptr);
			vkDestroyShaderModule(device, vertShaderModule, nullptr);
			return false;
		}

		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);

		return true;
	}

	bool CreateFrameBuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); ++i)
		{
			VkImageView attachments[] = { swapChainImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			
			// RenderPass와 같은 수와 같은 타입의 attachment 를 사용
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;			// 이미지 배열의 레이어수(3D framebuffer에 사용할 듯)

			if (!ensure(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) == VK_SUCCESS))
				return false;
		}

		return true;
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();

		// pCode 가 uint32_t* 형이라서 4 byte aligned 된 메모리를 넘겨줘야 함.
		// 다행히 std::vector의 default allocator가 가 메모리 할당시 4 byte aligned 을 이미 하고있어서 그대로 씀.
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		check(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS);

		// compiling or linking 과정이 graphics pipeline 이 생성되기 전까지 처리되지 않는다.
		// 그래픽스 파이프라인이 생성된 후 VkShaderModule은 즉시 소멸 가능.
		return shaderModule;
	}

	bool CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 커맨드 버퍼가 새로운 커맨드를 자주 다시 기록한다고 힌트를 줌.
		//											(메모리 할당 동작을 변경할 것임)
		// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 커맨드 버퍼들이 개별적으로 다시 기록될 수 있다.
		//													이 플래그가 없으면 모든 커맨드 버퍼들이 동시에 리셋되야 함.
		// 우리는 프로그램 시작시에 커맨드버퍼를 한번 녹화하고 계속해서 반복사용 할 것이므로 flags를 설정하지 않음.
		poolInfo.flags = 0;		// Optional

		if (!ensure(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS))
			return false;

		return true;
	}

	bool CreateCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;

		// VK_COMMAND_BUFFER_LEVEL_PRIMARY : 실행을 위해 Queue를 제출할 수 있으면 다른 커맨드버퍼로 부터 호출될 수 없다.
		// VK_COMMAND_BUFFER_LEVEL_SECONDARY : 직접 제출할 수 없으며, Primary command buffer 로 부터 호출될 수 있다.
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (!ensure(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) == VK_SUCCESS))
			return false;

		// Begin command buffers
		for (size_t i = 0; i < commandBuffers.size(); ++i)
		{
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 커맨드가 한번 실행된다음에 다시 기록됨
			// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : Single Render Pass 범위에 있는 Secondary Command Buffer.
			// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : 실행 대기중인 동안에 다시 서밋 될 수 있음.
			beginInfo.flags = 0;					// Optional

			// 이 플래그는 Secondary command buffer를 위해서만 사용하며, Primary command buffer 로 부터 상속받을 상태를 명시함.
			beginInfo.pInheritanceInfo = nullptr;	// Optional

			if (!ensure(vkBeginCommandBuffer(commandBuffers[i], &beginInfo) == VK_SUCCESS))
				return false;

			// Starting render pass
			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];

			// 렌더될 영역이며, 최상의 성능을 위해 attachment의 크기와 동일해야함.
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };		// CreateRenderPass 할때 사용한 VK_ATTACHMENT_LOAD_OP_CLEAR 를 위해 사용.
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			// 커맨드를 기록하는 명령어는 prefix로 모두 vkCmd 가 붙으며, 리턴값은 void 로 에러 핸들링은 따로 안함.
			// VK_SUBPASS_CONTENTS_INLINE : 렌더 패스 명령이 Primary 커맨드 버퍼에 포함되며, Secondary 커맨드 버퍼는 실행되지 않는다.
			// VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : 렌더 패스 명령이 Secondary 커맨드 버퍼에서 실행된다.
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Basic drawing commands
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			// Finishing up
			vkCmdEndRenderPass(commandBuffers[i]);

			if (!ensure(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS))
				return false;
		}

		return true;
	}

	bool CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

#if MULTIPLE_FRAME
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (!ensure(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) == VK_SUCCESS)
				|| !ensure(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) == VK_SUCCESS)
				|| !ensure(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) == VK_SUCCESS))
			{
				return false;
			}
		}
#else
		if (!ensure(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) == VK_SUCCESS)
			|| !ensure(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) == VK_SUCCESS))
		{
			return false;
		}
#endif // MULTIPLE_FRAME
		return true;
	}

	bool DrawFrame()
	{
		// 이 함수는 아래 3가지 기능을 수행함
		// 1. 스왑체인에서 이미지를 얻음
		// 2. Framebuffer 에서 Attachment로써 얻어온 이미지를 가지고 커맨드 버퍼를 실행
		// 3. 스왑체인 제출을 위해 이미지를 반환

		// 스왑체인을 동기화는 2가지 방법
		// 1. Fences		: vkWaitForFences 를 사용하여 프로그램에서 fences의 상태에 접근 할 수 있음.
		//						Fences는 어플리케이션과 렌더링 명령의 동기화를 위해 설계됨
		// 2. Semaphores	: 세마포어는 안됨.
		//						Semaphores 는 커맨드 Queue 내부 혹은 Queue 들 사이에 명령어 동기화를 위해서 설계됨

		// 현재는 Draw 와 presentation 커맨드를 동기화 하는 곳에 사용할 것이므로 세마포어가 적합.
		// 2개의 세마포어를 만들 예정
		// 1. 이미지를 획득해서 렌더링 준비가 완료된 경우 Signal(Lock 이 풀리는) 되는 것 (imageAvailableSemaphore)
		// 2. 렌더링을 마쳐서 Presentation 가능한 상태에서 Signal 되는 것 (renderFinishedSemaphore)

#if MULTIPLE_FRAME
		vkWaitForFences(device, 1, &inFlightFences[currenFrame], VK_TRUE, UINT64_MAX);
#endif // MULTIPLE_FRAME

		uint32_t imageIndex;
		// timeout 은 nanoseconds. UINT64_MAX 는 타임아웃 없음
#if MULTIPLE_FRAME
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currenFrame], VK_NULL_HANDLE, &imageIndex);

		// 이전 프레임에서 현재 사용하려는 이미지를 사용중에 있나? (그렇다면 펜스를 기다려라)
		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
			vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

		// 이 프레임에서 펜스를 사용한다고 마크 해둠
		imagesInFlight[imageIndex] = inFlightFences[currenFrame];
#else
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
#endif // MULTIPLE_FRAME

		// 여기서는 VK_SUCCESS or VK_SUBOPTIMAL_KHR 은 성공했다고 보고 계속함.
		// VK_ERROR_OUT_OF_DATE_KHR : 스왑체인이 더이상 서피스와 렌더링하는데 호환되지 않는 경우. (보통 윈도우 리사이즈 이후)
		// VK_SUBOPTIMAL_KHR : 스왑체인이 여전히 서피스에 제출 가능하지만, 서피스의 속성이 더이상 정확하게 맞지 않음.
		if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain();		// 이 경우 렌더링이 더이상 불가능하므로 즉시 스왑체인을 새로 만듬.
			return false;
		}
		else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR)
		{
			check(0);
			return false;
		}

		// Submitting the command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

#if MULTIPLE_FRAME
		VkSemaphore waitsemaphores[] = { imageAvailableSemaphores[currenFrame] };
#else
		VkSemaphore waitsemaphores[] = { imageAvailableSemaphore };
#endif // MULTIPLE_FRAME
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitsemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

#if MULTIPLE_FRAME
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currenFrame] };
#else
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
#endif // MULTIPLE_FRAME
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// SubmitInfo를 동시에 할수도 있음.
#if MULTIPLE_FRAME
		vkResetFences(device, 1, &inFlightFences[currenFrame]);		// 세마포어와는 다르게 수동으로 펜스를 unsignaled 상태로 재설정 해줘야 함

		// 마지막에 Fences 파라메터는 커맨드 버퍼가 모두 실행되고나면 Signaled 될 Fences.
		if (!ensure(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currenFrame]) == VK_SUCCESS))
			return false;
#else
		if (!ensure(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS))
			return false;
#endif // MULTIPLE_FRAME

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		// 여러개의 스왑체인에 제출하는 경우만 모든 스왑체인으로 잘 제출 되었나 확인하는데 사용
		// 1개인 경우 그냥 vkQueuePresentKHR 의 리턴값을 확인하면 됨.
		presentInfo.pResults = nullptr;			// Optional
		VkResult queuePresentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

		// 세마포어의 일관된 상태를 보장하기 위해서(세마포어 로직을 변경하지 않으려 노력한듯 함) vkQueuePresentKHR 이후에 framebufferResized 를 체크하는 것이 중요함.
		if ((queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR) || (queuePresentResult == VK_SUBOPTIMAL_KHR) || framebufferResized)
		{
			RecreateSwapChain();
			framebufferResized = false;
		}
		else if (queuePresentResult != VK_SUCCESS)
		{
			check(0);
			return false;
		}


		// CPU 가 큐에 작업을 제출하는 속도가 GPU 에서 소모하는 속도보다 빠른 경우 큐에 작업이 계속 쌓이거나 
		// 여러 프레임에 걸쳐 동시에 imageAvailableSemaphore 와 renderFinishedSemaphore를 재사용하게 되는 문제가 있음.
		// 1). 한프레임을 마치고 큐가 빌때까지 기다리는 것으로 해결할 수 있음. 한번에 1개의 프레임만 완성 가능(최적의 해결방법은 아님)
		// 2). 여러개의 프레임을 동시에 처리 할수있도록 확장. 동시에 진행될 수 있는 최대 프레임수를 지정해줌.
#if MULTIPLE_FRAME
		currenFrame = (currenFrame + 1) % MAX_FRAMES_IN_FLIGHT;
#else
		vkQueueWaitIdle(presentQueue);
#endif // MULTIPLE_FRAME

		return true;
	}

	void CleanupSwapChain()
	{
		// ImageViews and RenderPass 가 소멸되기전에 호출되어야 함
		for (auto framebuffer : swapChainFramebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		// Command buffer pool 을 다시 만들기 보다 있는 커맨드 버퍼 풀을 cleanup 하고 재사용 함.
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

		for (auto imageView : swapChainImageViews)
			vkDestroyImageView(device, imageView, nullptr);

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void RecreateSwapChain()
	{
		// 윈도우 최소화 된경우 창 사이즈가 0이 되는데 이경우는 Pause 상태로 뒀다가 윈도우 사이즈가 0이 아닐때 계속 진행하도록 함.
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		// 사용중인 리소스에 손을 댈수 없기 때문에 모두 사용될때까지 기다림
		vkDeviceWaitIdle(device);

		CleanupSwapChain();

		CreateSwapChain();
		CreateImageViews();			// Swapchain images 과 연관 있어서 다시 만듬
		CreateRenderPass();			// ImageView 와 연관 있어서 다시 만듬
		CreateGraphicsPipeline();	// 가끔 image format 이 다르기도 함.
									// Viewport나 Scissor Rectangle size 가 Graphics Pipeline 에 있으므로 재생성.
									// (DynamicState로 Viewport 와 Scissor 사용하고 변경점이 이것 뿐이면 재생성 피할수 있음)
		CreateFrameBuffers();		// Swapchain images 과 연관 있어서 다시 만듬
		CreateCommandBuffers();		// Swapchain images 과 연관 있어서 다시 만듬
	}


	GLFWwindow* window = nullptr;
	VkInstance instance;		// Instance는 Application과 Vulkan Library를 연결시켜줌, 드라이버에 어플리케이션 정보를 전달하기도 한다.

	// What validation layers do?
	// 1. 명세와는 다른 값의 파라메터를 사용하는 것을 감지
	// 2. 오브젝트들의 생성과 소멸을 추적하여 리소스 Leak을 감지
	// 3. Calls을 호출한 스레드를 추적하여, 스레드 안정성을 체크
	// 4. 모든 calls를 로깅하고, 그의 파라메터를 standard output으로 보냄
	// 5. 프로파일링과 리플레잉을 위해서 Vulkan calls를 추적
	//
	// Validation layer for LunarG
	// 1. Instance specific : instance 같은 global vulkan object와 관련된 calls 를 체크
	// 2. Device specific(deprecated) : 특정 GPU Device와 관련된 calls 를 체크.
	VkDebugUtilsMessengerEXT debugMessenger;

	// 물리 디바이스 - 물리 그래픽 카드를 선택
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	// Queue Families
	// 여러종류의 Queue type이 있을 수 있다. (ex. Compute or memory transfer related commands 만 만듬)
	// - 논리 디바이스(VkDevice) 생성시에 함께 생성시킴
	// - 논리 디바이스가 소멸될때 함께 알아서 소멸됨, 그래서 Cleanup 해줄필요가 없음.
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	// 논리 디바이스 생성
	VkDevice device;

	// Surface
	// VkInstance를 만들고 바로 Surface를 만들어야 한다. 물리 디바이스 선택에 영향을 미치기 때문
	// Window에 렌더링할 필요 없으면 그냥 만들지 않고 Offscreen rendering만 해도 됨. (OpenGL은 보이지 않는 창을 만들어야만 함)
	// 플랫폼 독립적인 구조이지만 Window의 Surface와 연결하려면 HWND or HMODULE 등을 사용해야 하므로 VK_KHR_win32_surface Extension을 사용해서 처리함.
	VkSurfaceKHR surface;

	// Swapchain
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;	// 스왑체인 이미지는 swapchain이 destroyed 될때 자동으로 cleanup 됨
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;

	// GraphicsPipieline
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	// Framebuffers
	std::vector<VkFramebuffer> swapChainFramebuffers;

	// Command buffers
	VkCommandPool commandPool;		// 커맨드 버퍼를 저장할 메모리 관리자로 커맨드 버퍼를 생성함.
	std::vector<VkCommandBuffer> commandBuffers;

	// Semaphores
#if MULTIPLE_FRAME
	// Semaphore 들은 GPU - GPU 간의 동기화를 맞춰줌. 여러개의 프레임이 동시에 만들어질 수 있게 함.
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	// CPU - GPU 간 동기화를 위해서 Fence를 사용함. MAX_FRAMES_IN_FLIGHT 보다 더 많은 수의 frame을 만들려고 시도하지 않게 해줌
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;		// 스왑체인 개수보다 MAX_FRAMES_IN_FLIGHT 가 더 많은 경우를 위해서 추가한 펜스
	size_t currenFrame = 0;
#else
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
#endif // MULTIPLE_FRAME

	bool framebufferResized = false;
};

int main()
{
	HelloTriangleApplication app;
	try
	{
		app.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
