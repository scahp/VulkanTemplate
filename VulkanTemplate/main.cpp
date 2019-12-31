// https://vulkan-tutorial.com/en/Texture_mapping/Images Transition barrier masks

#include <pch.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <string>
#include <fstream>
#include <array>
#include <chrono>

#include "jAssert.h"
#include "jSimpleType.h"
#include "Camera.h"

#define MULTIPLE_FRAME 1
#define VALIDATION_LAYER_VERBOSE 0

struct jVertex
{
	jSimpleVec2 pos;
	jSimpleVec3 color;
	jSimpleVec2 texCoord;		// UV 는 좌상단이 (0, 0), 우하단이 (1, 1) 좌표임.

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		// 모든 데이터가 하나의 배열에 있어서 binding index는 0
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(jVertex);

		// VK_VERTEX_INPUT_RATE_VERTEX : 각각 버택스 마다 다음 데이터로 이동
		// VK_VERTEX_INPUT_RATE_INSTANCE : 각각의 인스턴스 마다 다음 데이터로 이동
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	};

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;

		//float: VK_FORMAT_R32_SFLOAT
		//vec2 : VK_FORMAT_R32G32_SFLOAT
		//vec3 : VK_FORMAT_R32G32B32_SFLOAT
		//vec4 : VK_FORMAT_R32G32B32A32_SFLOAT
		//ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
		//uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
		//double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(jVertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(jVertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(jVertex, texCoord);

		return attributeDescriptions;
	}
};

// Alignment
// 1. 스칼라 타입은 4 bytes align 필요.				Scalars have to be aligned by N(= 4 bytes given 32 bit floats).
// 2. vec2 타입은 8 bytes align 필요.				A vec2 must be aligned by 2N(= 8 bytes)
// 3. vec3 or vec4 타입은 16 bytes align 필요.		A vec3 or vec4 must be aligned by 4N(= 16 bytes)
// 4. 중첩 구조(stuct 같은 것이 멤버인 경우)의 경우 멤버의 기본정렬을 16 bytes 배수로 올림하여 align 되어야 함. 
//													A nested structure must be aligned by the base alignment of its members rounded up to a multiple of 16.
//		예로 C++ 와 Shader 쪽에서 아래와 같이 선언 되어있다고 하자.
//		C++ : struct Foo { Vector2 v; }; struct jUniformBufferObject { Foo f1; Foo f2; };
//		Shader : struct Foo { vec2 v; }; layout(binding = 0) uniform jUniformBufferObject { Foo f1; Foo f2; };
//		Foo는 8 bytes 이지만 C++ 에서 아래와 같이 align을 16으로 맞춰줘야 한다. 이유는 중첩구조(struct 형태의 멤버변수)이기 때문.
//			-> 이렇게 해줘야 함. struct jUniformBufferObject { Foo f1; alignas(16) Foo f2; };
// 5. mat4 타입은 vec4 처럼 16 bytes align 필요.		A mat4 matrix must have the same alignment as a vec4.
// 자세한 설명 : https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
// 아래와 같은 경우 맨 처음 등장하는 Vector2 때문에 뒤에나오는 Matrix 가 16 Bytes align 되지 못함.
//struct jUniformBufferObject		|		layout(binding = 0) uniform UniformBufferObject
//{									|		{
//	Vector2 foo;					|			vec2 foo;
//	Matrix Model;					|			mat4 model;
//	Matrix View;					|			mat4 view;
//	Matrix Proj;					|			mat4 proj
//};								|		};
// 이 경우 아래와 같이 alignas(16) 을 써줘서 model 부터 16 bytes align 하면 정상작동 할 수 있음.
//struct jUniformBufferObject		|		layout(binding = 0) uniform UniformBufferObject
//{									|		{
//	Vector2 foo;					|			vec2 foo;
//	alignas(16)Matrix Model;		|			mat4 model;
//	Matrix View;					|			mat4 view;
//	Matrix Proj;					|			mat4 proj
//};								|		};
struct jUniformBufferObject
{
	Matrix Model;
	Matrix View;
	Matrix Proj;
};

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
		//, "VK_LAYER_LUNARG_api_dump"		// display api call
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	const std::vector<jVertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
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

		CreateInstance();			// 1
		SetupDebugMessenger();		// 2
		CreateSurface();			// 3
		PickPhysicalDevice();		// 4
		CreateLogicalDevice();		// 5
		CreateSwapChain();			// 6
		CreateImageViews();			// 7
		CreateRenderPass();			// 8
		CreateDescriptorSetLayout();// 9
		CreateGraphicsPipeline();	// 10
		CreateFrameBuffers();		// 11
		CreateCommandPool();		// 12
		CreateTextureImage();		// 13
		CreateTextureImageView();	// 14
		CreateTextureSampler();		// 15
		CreateVertexBuffer();		// 16
		CreateIndexBuffer();		// 17
		CreateUniformBuffers();		// 18
		CreateDescriptorPool();		// 19
		CreateDescriptorSets();		// 20
		CreateCommandBuffers();		// 21
		CreateSyncObjects();		// 22
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

		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);

		vkDestroyImage(device, textureImage, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexBufferMemory, nullptr);

		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

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
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
#if VALIDATION_LAYER_VERBOSE
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
#endif
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

		VkPhysicalDeviceFeatures deviceFeatures = {};
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

		return indices.IsComplete() && extensionsSupported && swapChainAdequate && deviceFeatures.samplerAnisotropy;
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
		deviceFeatures.samplerAnisotropy = VK_TRUE;		// VkSampler 가 Anisotropy 를 사용할 수 있도록 하기 위해 true로 설정

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
			swapChainImageViews[i] = CreateImageView(swapChainImages[i], swapChainImageFormat);

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

	bool CreateDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;

		// VkShaderStageFlagBits 에 값을 | 연산으로 조합가능
		//  - VK_SHADER_STAGE_ALL_GRAPHICS 로 설정하면 모든곳에서 사용
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;	// Optional (이미지 샘플링 시에서만 사용)

		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;	// sampler 를 fragment shader에 붙임.

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (!ensure(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) == VK_SUCCESS))
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

		auto bindingDescription = jVertex::GetBindingDescription();
		auto attributeDescription = jVertex::GetAttributeDescriptions();

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

		// 3. Input Assembly
		// primitiveRestartEnable 옵션이 VK_TRUE 이면, 인덱스버퍼의 특수한 index 0xFFFF or 0xFFFFFFFF 를 사용해서 line 과 triangle topology mode를 사용할 수 있다.
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// 4. Viewports and scissors
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

		// 5. Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;		// 이 값이 VK_TRUE 면 Near나 Far을 벗어나는 영역을 [0.0 ~ 1.0]으로 Clamp 시켜줌.(쉐도우맵에서 유용)
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	// 이 값이 VK_TRUE 면, 레스터라이저 스테이지를 통과할 수 없음. 즉 Framebuffer 로 결과가 넘어가지 않음.
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	// FILL, LINE, POINT 세가지가 있음
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		//rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;			// 쉐도우맵 용
		rasterizer.depthBiasConstantFactor = 0.0f;		// Optional
		rasterizer.depthBiasClamp = 0.0f;				// Optional
		rasterizer.depthBiasSlopeFactor = 0.0f;			// Optional

		// 6. Multisampling
		// 현재는 사용하지 않음
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;				// Optional
		multisampling.pSampleMask = nullptr;				// Optional
		multisampling.alphaToCoverageEnable = VK_FALSE;		// Optional
		multisampling.alphaToOneEnable = VK_FALSE;			// Optional

		// 7. Depth and stencil testing
		// 현재는 사용하지 않음
		// VkPipelineDepthStencilStateCreateInfo depthStencil = {};

		// 8. Color blending
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

		// 9. Dynamic state
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

		// 10. Pipeline layout
		// 쉐이더에 전달된 Uniform 들을 명세하기 위한 오브젝트
		// 이 오브젝트는 프로그램 실행동안 계속해서 참조되므로 cleanup 에서 제거해줌
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
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

	bool CreateTextureImage()
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("Textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;

		if (!ensure(pixels))
			return false;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);

		if (!ensure(CreateImage(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), VK_FORMAT_R8G8B8A8_UNORM
			, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT
									| VK_IMAGE_USAGE_SAMPLED_BIT	// image를 shader 에서 접근가능하게 하고 싶은 경우
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory)))
		{
			return false;
		}

		if (!TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
			return false;
		CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		
		// 이제 쉐이더에 읽기가 가능하게 하기위해서 아래와 같이 적용.
		if (TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
			return false;

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		return true;
	}

	VkImageView CreateImageView(VkImage image, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo = {};

		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;

		// SubresourceRange에 이미지의 목적과 이미지의 어떤 파트에 접근할 것인지를 명세한다.
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		// RGBA 컴포넌트들에게 컬러 채널을 매핑할 수 있다.
		// 예를들어 VK_COMPONENT_SWIZZLE_R 을 모든 채널에 넣으면 R을 사용한 흑백 텍스쳐를 나타낼 수 있음.
		// 현재는 기본인 VK_COMPONENT_SWIZZLE_IDENTITY 를 설정해준다.
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		VkImageView imageView;
		ensure(vkCreateImageView(device, &viewInfo, nullptr, &imageView) == VK_SUCCESS);

		return imageView;
	}

	bool CreateTextureImageView()
	{
		textureImageView = CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
		return true;
	}

	bool CreateTextureSampler()
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		// UV가 [0~1] 범위를 벗어는 경우 처리
		// VK_SAMPLER_ADDRESS_MODE_REPEAT : 반복해서 출력, UV % 1
		// VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : 반복하지만 거울에 비치듯 반대로 출력
		// VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : 범위 밖은 가장자리의 색으로 모두 출력
		// VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE : 범위 밖은 반대편 가장자리의 색으로 모두 출력
		// VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : 단색으로 설정함. (samplerInfo.borderColor)
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;

		// 이게 true 이면 UV 좌표가 [0, texWidth], [0, texHeight] 가 됨. false 이면 [0, 1] 범위
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		// compareEnable이 ture 이면, 텍셀을 특정 값과 비교한 뒤 그 결과를 필터링 연산에 사용한다.
		// Percentage-closer filtering(PCF) 에 주로 사용됨.
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (!ensure(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) == VK_SUCCESS))
			return false;

		return true;
	}

	bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage
		, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;

		// VK_IMAGE_TILING_LINEAR : 텍셀이 Row-major 순서로 저장. pixels array 처럼.
		// VK_IMAGE_TILING_OPTIMAL : 텍셀이 최적의 접근을 위한 순서로 저장
		// image의 memory 안에 있는 texel에 직접 접근해야한다면, VK_IMAGE_TILING_LINEAR 를 써야함.
		// 그러나 지금 staging image가 아닌 staging buffer를 사용하기 때문에 VK_IMAGE_TILING_OPTIMAL 를 쓰면됨.
		imageInfo.tiling = tiling;

		// VK_IMAGE_LAYOUT_UNDEFINED : GPU에 의해 사용될 수 없으며, 첫번째 transition 에서 픽셀들을 버릴 것임.
		// VK_IMAGE_LAYOUT_PREINITIALIZED : GPU에 의해 사용될 수 없으며, 첫번째 transition 에서 픽셀들이 보존 될것임.
		// VK_IMAGE_LAYOUT_GENERAL : 성능은 좋지 않지만 image를 input / output 둘다 의 용도로 사용하는 경우 씀.
		// 첫번째 전환에서 텍셀이 보존되어야 하는 경우는 거의 없음.
		//	-> 이런 경우는 image를 staging image로 하고 VK_IMAGE_TILING_LINEAR를 쓴 경우이며, 이때는 Texel 데이터를
		//		image에 업로드하고, image를 transfer source로 transition 하는 경우가 됨.
		// 하지만 지금의 경우는 첫번째 transition에서 image는 transfer destination 이 된다. 그래서 기존 데이터를 유지
		// 할 필요가 없다 그래서 VK_IMAGE_LAYOUT_UNDEFINED 로 설정한다.
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;

		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;	// Multisampling 안하므로 샘플 개수는 1개
		imageInfo.flags = 0;		// Optional
									// Sparse image 에 대한 정보를 설정가능
									// Sparse image는 특정 영역의 정보를 메모리에 담아두는 것임. 예를들어 3D image의 경우
									// 복셀영역의 air 부분의 표현을 위해 많은 메모리를 할당하는것을 피하게 해줌.

		if (!ensure(vkCreateImage(device, &imageInfo, nullptr, &image) == VK_SUCCESS))
			return false;

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
		if (!ensure(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) == VK_SUCCESS))
			return false;

		vkBindImageMemory(device, image, textureImageMemory, 0);

		return true;
	}

	VkCommandBuffer BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void EndSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		// 명령 완료를 기다리기 위해서 2가지 방법이 있는데, Fence를 사용하는 방법(vkWaitForFences)과 Queue가 Idle이 될때(vkQueueWaitIdle)를 기다리는 방법이 있음.
		// fence를 사용하는 방법이 여러개의 전송을 동시에 하고 마치는 것을 기다릴 수 있게 해주기 때문에 그것을 사용함.
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	bool TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		// Layout Transition 에는 image memory barrier 사용
		// Pipeline barrier는 리소스들 간의 synchronize 를 맞추기 위해 사용 (버퍼를 읽기전에 쓰기가 완료되는 것을 보장받기 위해)
		// Pipeline barrier는 image layout 간의 전환과 VK_SHARING_MODE_EXCLUSIVE를 사용한 queue family ownership을 전달하는데에도 사용됨

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;		// 현재 image 내용이 존재하던말던 상관없다면 VK_IMAGE_LAYOUT_UNDEFINED 로 하면 됨
		barrier.newLayout = newLayout;

		// 아래 두필드는 Barrier를 사용해 Queue family ownership을 전달하는 경우에 사용됨.
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = image;

		// subresourcerange 는 image에서 영향을 받는 것과 부분을 명세함.
		// mimapping 이 없으므로 levelCount와 layercount 를 1로
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;	// TODO
		barrier.dstAccessMask = 0;	// TODO

		// Barrier 는 동기화를 목적으로 사용하므로, 이 리소스와 연관되는 어떤 종류의 명령이 이전에 일어나야 하는지와
		// 어떤 종류의 명령이 Barrier를 기다려야 하는지를 명시해야만 한다. vkQueueWaitIdle 을 사용하지만 그래도 수동으로 해줘야 함.

		// Undefined -> transfer destination : 이 경우 기다릴 필요없이 바로 쓰면됨. Undefined 라 다른 곳에서 딱히 쓰거나 하는것이 없음.
		// Transfer destination -> frag shader reading : frag 쉐이더에서 읽기 전에 transfer destination 에서 쓰기가 완료 됨이 보장되어야 함. 그래서 shader 에서 읽기 가능.
		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
		{
			// 이전 작업을 기다릴 필요가 없어서 srcAccessMask에 0, sourceStage 에 가능한 pipeline stage의 가장 빠른 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	// VK_ACCESS_TRANSFER_WRITE_BIT 는 Graphics 나 Compute stage에 실제 stage가 아닌 pseudo-stage 임.

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
		{
			check(!"Unsupported layout transition!");
			return false;
		}
		
		// 현재 싱글 커맨드버퍼 서브미션은 암시적으로 VK_ACCESS_HOST_WRITE_BIT 동기화를 함.

		// 모든 종류의 Pipeline barrier 가 같은 함수로 submit 함.
		vkCmdPipelineBarrier(commandBuffer
			, sourceStage		// 	- 이 barrier 를 기다릴 pipeline stage. 
								//		만약 barrier 이후 uniform 을 읽을 경우 VK_ACCESS_UNIFORM_READ_BIT 과 
								//		파이프라인 스테이지에서 유니폼 버퍼를 읽을 가장 빠른 쉐이더 지정
								//		(예를들면, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT - 이 barrier가 uniform을 수정했고, Fragment shader에서 uniform을 처음 읽는거라면)
			, destinationStage	// 	- 0 or VK_DEPENDENCY_BY_REGION_BIT(지금까지 쓰여진 리소스 부분을 읽기 시작할 수 있도록 함)
			, 0
			// 아래 3가지 부분은 이번에 사용할 memory, buffer, image  barrier 의 개수가 배열을 중 하나를 명시
			, 0, nullptr
			, 0, nullptr
			, 1, &barrier
		);

		EndSingleTimeCommands(commandBuffer);

		return true;
	}

	void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;

		// 아래 2가지는 얼마나 많은 pixel이 들어있는지 설명, 둘다 0, 0이면 전체
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		// 아래 부분은 이미지의 어떤 부분의 픽셀을 복사할지 명세
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(commandBuffer, buffer, image
			, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL		// image가 현재 어떤 레이아웃으로 사용되는지 명세
			, 1, &region);

		EndSingleTimeCommands(commandBuffer);
	}

	bool CreateVertexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		// VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 이 버퍼가 메모리 전송 연산의 소스가 될 수 있음.
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		//// 마지막 파라메터 0은 메모리 영역의 offset 임.
		//// 이 값이 0이 아니면 memRequirements.alignment 로 나눠야 함. (align 되어있다는 의미)
		//vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

		void* data;
		// size 항목에 VK_WHOLE_SIZE  를 넣어서 모든 메모리를 잡을 수도 있음.
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		// Map -> Unmap 했다가 메모리에 데이터가 즉시 반영되는게 아님
		// 바로 사용하려면 아래 2가지 방법이 있음.
		// 1. VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 사용 (항상 반영, 약간 느릴 수도)
		// 2. 쓰기 이후 vkFlushMappedMemoryRanges 호출, 읽기 이후 vkInvalidateMappedMemoryRanges 호출
		// 위의 2가지 방법을 사용해도 이 데이터가 GPU에 바로 보인다고 보장할 수는 없지만 다음 vkQueueSubmit 호출 전에는 완료될 것을 보장함.

		// VK_BUFFER_USAGE_TRANSFER_DST_BIT : 이 버퍼가 메모리 전송 연산의 목적지가 될 수 있음.
		// DEVICE LOCAL 메모리에 VertexBuffer를 만들었으므로 이제 vkMapMemory 같은 것은 할 수 없음.
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

		CopyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		return true;
	}

	bool CreateIndexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

		CopyBuffer(stagingBuffer, indexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		return true;
	}

	bool CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		// 임시 커맨드 버퍼를 통해서 메모리를 전송함.
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 커맨드버퍼를 1번만 쓰고, 복사가 다 될때까지 기다리기 위해서 사용

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;		// Optional
		copyRegion.dstOffset = 0;		// Optional
		copyRegion.size = size;			// 여기서는 VK_WHOLE_SIZE 사용 불가
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(commandBuffer);

		return true;
	}

	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}

		check(0);	// failed to find sutable memory type!
		return 0;
	}

	bool CreateUniformBuffers()
	{
		VkDeviceSize bufferSize = sizeof(jUniformBufferObject);
	
		uniformBuffers.resize(swapChainImages.size());
		uniformBuffersMemory.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		}
		return true;
	}

	bool CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size());

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
		poolInfo.flags = 0;		// Descriptor Set을 만들고나서 더 이상 손대지 않을거라 그냥 기본값 0으로 설정

		if (!ensure(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS))
			return false;

		return true;
	}

	bool CreateDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(swapChainImages.size());
		if (!ensure(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) == VK_SUCCESS))
			return false;

		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			VkDescriptorBufferInfo bufferInfo = {};
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(jUniformBufferObject);		// 전체 사이즈라면 VK_WHOLE_SIZE 이거 가능

			VkDescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;		// 현재는 Buffer 기반 Desriptor 이므로 이것을 사용
			descriptorWrites[0].pImageInfo = nullptr;			// Optional	(Image Data 기반에 사용)
			descriptorWrites[0].pTexelBufferView = nullptr;		// Optional (Buffer View 기반에 사용)

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size())
				, descriptorWrites.data(), 0, nullptr);
		}

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

			VkBuffer vertexBuffers[] = { vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

			//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);			// VertexBuffer 만 있는 경우 호출
			vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

		UpdateUniformBuffer(imageIndex);

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

		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
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
		CreateUniformBuffers();
		CreateDescriptorPool();
		CreateDescriptorSets();
		CreateCommandBuffers();		// Swapchain images 과 연관 있어서 다시 만듬
	}

	bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;

		// OR 비트연산자를 사용해 다양한 버퍼용도로 사용할 수 있음.
		bufferInfo.usage = usage;

		// swapchain과 마찬가지로 버퍼또한 특정 queue family가 소유하거나 혹은 여러 Queue에서 공유됨
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (!ensure(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) == VK_SUCCESS))
			return false;

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (!ensure(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) == VK_SUCCESS))
			return false;

		// 마지막 파라메터 0은 메모리 영역의 offset 임.
		// 이 값이 0이 아니면 memRequirements.alignment 로 나눠야 함. (align 되어있다는 의미)
		vkBindBufferMemory(device, buffer, bufferMemory, 0);

		return true;
	}

	void UpdateUniformBuffer(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		jUniformBufferObject ubo = {};
		ubo.Model.SetIdentity();
		ubo.View.SetIdentity();
		ubo.Proj.SetIdentity();
		ubo.Model = Matrix::MakeRotate(Vector(0.0f, 0.0f, 1.0f), time * DegreeToRadian(90.0f)).GetTranspose();
		ubo.View = jCameraUtil::CreateViewMatrix(Vector(2.0f, 2.0f, 2.0f), Vector(0.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 1.0f)).GetTranspose();
		ubo.Proj = jCameraUtil::CreatePerspectiveMatrix(static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height)
			, DegreeToRadian(45.0f), 10.0f, 0.1f).GetTranspose();
		ubo.Proj.m[1][1] *= -1;

		//ubo.Model.SetTranslate({ 0.2f, 0.2f,0.2f });
		//ubo.Model = ubo.Model.MakeRotateZ(time * DegreeToRadian(90.0f));

		void* data;
		vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
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
	VkDescriptorSetLayout descriptorSetLayout;
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

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	// Descriptor : 쉐이더가 버퍼나 이미지 같은 리소스에 자유롭게 접근하는 방법. 디스크립터의 사용방법은 아래 3가지로 구성됨.
	//	1. Pipeline 생성 도중 Descriptor Set Layout 명세
	//	2. Descriptor Pool로 Descriptor Set 생성
	//	3. Descriptor Set을 렌더링 하는 동안 묶어 주기.
	//
	// Descriptor set layout	: 파이프라인을 통해 접근할 리소스 타입을 명세함
	// Descriptor set			: Descriptor 에 묶일 실제 버퍼나 이미지 리소스를 명세함.
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;		// DescriptorPool 이 소멸될때 자동으로 소멸되므로 따로 소멸시킬 필요없음.

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
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
