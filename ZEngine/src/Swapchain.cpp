#include <pch.h>
#include <ZEngineDef.h>
#include <Rendering/Swapchain.h>
#include <Rendering/Specifications/AttachmentSpecification.h>
#include <Hardwares/VulkanDevice.h>
#include <random>

using namespace ZEngine::Rendering::Specifications;

namespace ZEngine::Rendering
{
    Swapchain::Swapchain(void* native_window, bool is_surface_from_device)
    {
        std::random_device                                rd;
        std::mt19937                                      gen(rd());
        std::uniform_int_distribution<unsigned long long> dis(std::numeric_limits<std::uint64_t>::min(), std::numeric_limits<std::uint64_t>::max());
        m_identifier = dis(gen);

        m_is_surface_from_device = is_surface_from_device;
        m_native_window          = native_window;

        /* Create Attachment */
        Specifications::AttachmentSpecification attachment_specification = {};
        attachment_specification.BindPoint                               = PipelineBindPoint::GRAPHIC;
        // Color attachment Description
        attachment_specification.ColorsMap[0]                 = {};
        attachment_specification.ColorsMap[0].Format          = ImageFormat::FORMAT_FROM_DEVICE;
        attachment_specification.ColorsMap[0].Load            = LoadOperation::CLEAR;
        attachment_specification.ColorsMap[0].Store           = StoreOperation::STORE;
        attachment_specification.ColorsMap[0].Initial         = ImageLayout::UNDEFINED;
        attachment_specification.ColorsMap[0].Final           = ImageLayout::PRESENT_SRC;
        attachment_specification.ColorsMap[0].ReferenceLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        // Attachment Dependencies
        attachment_specification.DependenciesMap[0]               = {};
        attachment_specification.DependenciesMap[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
        attachment_specification.DependenciesMap[0].dstSubpass    = 0;
        attachment_specification.DependenciesMap[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        attachment_specification.DependenciesMap[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        attachment_specification.DependenciesMap[0].srcAccessMask = 0;
        attachment_specification.DependenciesMap[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        m_attachment                                              = Renderers::RenderPasses::Attachment::Create(attachment_specification);

        Create();
    }

    Swapchain::~Swapchain()
    {
        Dispose();
        m_attachment->Dispose();

        m_acquired_semaphore_collection.clear();
        m_acquired_semaphore_collection.shrink_to_fit();

        m_render_complete_semaphore_collection.clear();
        m_render_complete_semaphore_collection.shrink_to_fit();

        m_frame_signal_fence_collection.clear();
        m_frame_signal_fence_collection.shrink_to_fit();

        if (!m_is_surface_from_device)
        {
            auto instance = Hardwares::VulkanDevice::GetNativeInstanceHandle();
            ZENGINE_DESTROY_VULKAN_HANDLE(instance, vkDestroySurfaceKHR, m_surface, nullptr)
        }
        m_native_window = nullptr; // we are not responsible of the cleanup
    }

    void Swapchain::Resize()
    {
        Dispose();
        Create();
    }

    void Swapchain::Present()
    {
        Primitives::Fence* signal_fence = m_frame_signal_fence_collection[m_current_frame_index].get();
        signal_fence->Wait(UINT64_MAX);
        signal_fence->Reset();

        Primitives::Semaphore* signal_semaphore = m_acquired_semaphore_collection[m_current_frame_index].get();
        ZENGINE_VALIDATE_ASSERT(signal_semaphore->GetState() != Primitives::SemaphoreState::Submitted, "")

        uint32_t frame_index{UINT32_MAX};
        VkResult acquire_image_result =
            vkAcquireNextImageKHR(Hardwares::VulkanDevice::GetNativeDeviceHandle(), m_handle, UINT64_MAX, signal_semaphore->GetHandle(), VK_NULL_HANDLE, &frame_index);
        signal_semaphore->SetState(Primitives::SemaphoreState::Submitted);

        if (acquire_image_result == VK_SUBOPTIMAL_KHR || acquire_image_result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Resize();
            return;
        }

        Primitives::Semaphore* render_complete_semaphore = m_render_complete_semaphore_collection[m_current_frame_index].get();
        if (!Hardwares::VulkanDevice::Present(m_handle, &frame_index, signal_semaphore, render_complete_semaphore,  signal_fence))
        {
            Resize();
            return;
        }

        auto frame_count      = (int32_t) m_image_collection.size();
        m_current_frame_index = (m_current_frame_index + 1) % frame_count;
        Hardwares::VulkanDevice::SetCurrentFrameIndex(m_current_frame_index);
    }

    uint32_t Swapchain::GetMinImageCount() const
    {
        return m_min_image_count;
    }

    uint32_t Swapchain::GetImageCount() const
    {
        return m_image_count;
    }

    VkRenderPass Swapchain::GetRenderPass() const
    {
        if (m_attachment)
        {
            return m_attachment->GetHandle();
        }
        return VK_NULL_HANDLE;
    }

    VkFramebuffer Swapchain::GetCurrentFramebuffer()
    {
        std::lock_guard lock(m_image_mutex);
        ZENGINE_VALIDATE_ASSERT(m_current_frame_index < m_framebuffer_collection.size(), "Index out of range while accessing framebuffer collection")
        return m_framebuffer_collection[m_current_frame_index];
    }

    uint32_t Swapchain::GetCurrentFrameIndex()
    {
        std::lock_guard lock(m_image_mutex);
        ZENGINE_VALIDATE_ASSERT(m_current_frame_index >= 0  && m_current_frame_index < m_framebuffer_collection.size(), "Index out of range")
        return m_current_frame_index;
    }

    uint64_t Swapchain::GetIdentifier() const
    {
        return m_identifier;
    }

    VkSwapchainKHR Swapchain::GetHandle() const
    {
        return m_handle;
    }

    void Swapchain::Create()
    {
        auto native_device   = Hardwares::VulkanDevice::GetNativeDeviceHandle();
        auto physical_device = Hardwares::VulkanDevice::GetNativePhysicalDeviceHandle();

        if (m_surface == VK_NULL_HANDLE)
        {
            if (m_is_surface_from_device)
            {
                m_surface        = Hardwares::VulkanDevice::GetSurface();
                m_surface_format = Hardwares::VulkanDevice::GetSurfaceFormat();
            }
            else
            {
                auto instance = Hardwares::VulkanDevice::GetNativeInstanceHandle();
                ZENGINE_VALIDATE_ASSERT(
                    glfwCreateWindowSurface(instance, reinterpret_cast<GLFWwindow*>(m_native_window), nullptr, &m_surface) == VK_SUCCESS, "Failed Window Surface from GLFW");

                uint32_t                        format_count              = 0;
                std::vector<VkSurfaceFormatKHR> surface_format_collection = {};
                vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, nullptr);
                if (format_count != 0)
                {
                    surface_format_collection.resize(format_count);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, surface_format_collection.data());
                }

                m_surface_format = (surface_format_collection.size() > 0) ? surface_format_collection[0] : VkSurfaceFormatKHR{};
                for (const VkSurfaceFormatKHR& format_khr : surface_format_collection)
                {
                    // default is: VK_FORMAT_B8G8R8A8_SRGB
                    // but Imgui wants : VK_FORMAT_B8G8R8A8_UNORM ...
                    if ((format_khr.format == VK_FORMAT_B8G8R8A8_UNORM) && (format_khr.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
                    {
                        m_surface_format = format_khr;
                        break;
                    }
                }
            }
        }

        // Surface capabilities
        VkExtent2D extent = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, m_surface, &m_capabilities);
        if (m_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            extent = m_capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(m_native_window), &width, &height);

            extent        = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            extent.width  = std::clamp(extent.width, m_capabilities.minImageExtent.width, m_capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, m_capabilities.minImageExtent.height, m_capabilities.maxImageExtent.height);
        }

        m_min_image_count = std::clamp(m_min_image_count, m_capabilities.minImageCount + 1, m_capabilities.maxImageCount);

        VkSwapchainCreateInfoKHR swapchain_create_info = {};
        swapchain_create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface                  = m_surface;
        swapchain_create_info.minImageCount            = m_min_image_count;
        swapchain_create_info.imageFormat              = m_surface_format.format;
        swapchain_create_info.imageColorSpace          = m_surface_format.colorSpace;
        swapchain_create_info.imageExtent              = extent;
        swapchain_create_info.imageArrayLayers         = m_capabilities.maxImageArrayLayers;
        swapchain_create_info.preTransform             = m_capabilities.currentTransform;

        std::unordered_set<uint32_t> device_family_indices = {
            Hardwares::VulkanDevice::GetQueue(QueueType::GRAPHIC_QUEUE).FamilyIndex, Hardwares::VulkanDevice::GetQueue(QueueType::TRANSFER_QUEUE).FamilyIndex};

        m_queue_family_index_collection = std::vector<uint32_t>{device_family_indices.begin(), device_family_indices.end()};

        if (m_queue_family_index_collection.size() > 1)
        {
            swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.queueFamilyIndexCount = m_queue_family_index_collection.size();
            swapchain_create_info.pQueueFamilyIndices   = m_queue_family_index_collection.data();
        }
        else
        {
            swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            swapchain_create_info.queueFamilyIndexCount = 0;
            swapchain_create_info.pQueueFamilyIndices   = nullptr;
        }
        swapchain_create_info.imageUsage     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.clipped        = VK_TRUE;

        ZENGINE_VALIDATE_ASSERT(vkCreateSwapchainKHR(native_device, &swapchain_create_info, nullptr, &m_handle) == VK_SUCCESS, "Failed to create Swapchain")

        /*Swapchain Images & ImageView*/
        uint32_t image_count{0};
        ZENGINE_VALIDATE_ASSERT(vkGetSwapchainImagesKHR(native_device, m_handle, &image_count, nullptr) == VK_SUCCESS, "Failed to get Images count from Swapchain")

        m_image_count = image_count;
        m_image_collection.resize(image_count);
        m_image_view_collection.resize(image_count);
        m_framebuffer_collection.resize(image_count);
        m_acquired_semaphore_collection.resize(image_count, nullptr);
        m_render_complete_semaphore_collection.resize(image_count, nullptr);
        m_frame_signal_fence_collection.resize(image_count, nullptr);

        ZENGINE_VALIDATE_ASSERT(vkGetSwapchainImagesKHR(native_device, m_handle, &image_count, m_image_collection.data()) == VK_SUCCESS, "Failed to get Images from Swapchain")

        /*Transition Image from Undefined to Present_src*/
        auto command_buffer = Hardwares::VulkanDevice::BeginInstantCommandBuffer(Rendering::QueueType::GRAPHIC_QUEUE);
        {
            for (int i = 0; i < m_image_collection.size(); ++i)
            {
                Specifications::ImageMemoryBarrierSpecification barrier_spec = {};
                barrier_spec.ImageHandle                                     = m_image_collection[i];
                barrier_spec.OldLayout                                       = Specifications::ImageLayout::UNDEFINED;
                barrier_spec.NewLayout                                       = Specifications::ImageLayout::PRESENT_SRC;
                barrier_spec.ImageAspectMask                                 = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier_spec.SourceAccessMask                                = 0;
                barrier_spec.DestinationAccessMask                           = VK_ACCESS_MEMORY_READ_BIT;
                barrier_spec.SourceStageMask                                 = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier_spec.DestinationStageMask                            = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier_spec.LayerCount                                      = 1;

                Primitives::ImageMemoryBarrier barrier{barrier_spec};
                command_buffer->TransitionImageLayout(barrier);
            }
        }
        Hardwares::VulkanDevice::EndInstantCommandBuffer(command_buffer);

        for (size_t i = 0; i < m_image_view_collection.size(); ++i)
        {
            m_image_view_collection[i] = Hardwares::VulkanDevice::CreateImageView(m_image_collection[i], m_surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        /*Swapchain framebuffer*/
        for (size_t i = 0; i < m_framebuffer_collection.size(); i++)
        {
            auto attachments            = std::vector<VkImageView>{m_image_view_collection[i]};
            m_framebuffer_collection[i] = Hardwares::VulkanDevice::CreateFramebuffer(attachments, m_attachment->GetHandle(), extent.width, extent.height);
        }

        /*Swapchain semaphore*/
        for (size_t i = 0; i < m_acquired_semaphore_collection.size(); i++)
        {
            m_acquired_semaphore_collection[i] = CreateRef<Primitives::Semaphore>();
        }

        for (size_t i = 0; i < m_render_complete_semaphore_collection.size(); i++)
        {
            m_render_complete_semaphore_collection[i] = CreateRef<Primitives::Semaphore>();
        }

        for (size_t i = 0; i < m_frame_signal_fence_collection.size(); i++)
        {
            m_frame_signal_fence_collection[i] = CreateRef<Primitives::Fence>(true);
        }
    }

    void Swapchain::Dispose()
    {
        for (VkImageView image_view : m_image_view_collection)
        {
            if (image_view)
            {
                Hardwares::VulkanDevice::EnqueueForDeletion(DeviceResourceType::IMAGEVIEW, image_view);
            }
        }

        for (VkFramebuffer framebuffer : m_framebuffer_collection)
        {
            if (framebuffer)
            {
                Hardwares::VulkanDevice::EnqueueForDeletion(DeviceResourceType::FRAMEBUFFER, framebuffer);
            }
        }

        m_image_view_collection.clear();
        m_image_collection.clear();
        m_framebuffer_collection.clear();

        m_image_view_collection.shrink_to_fit();
        m_image_collection.shrink_to_fit();
        m_framebuffer_collection.shrink_to_fit();

        m_acquired_semaphore_collection.clear();
        m_render_complete_semaphore_collection.clear();
        m_frame_signal_fence_collection.clear();

        m_acquired_semaphore_collection.shrink_to_fit();
        m_render_complete_semaphore_collection.shrink_to_fit();
        m_frame_signal_fence_collection.shrink_to_fit();

        auto device = Hardwares::VulkanDevice::GetNativeDeviceHandle();
        ZENGINE_DESTROY_VULKAN_HANDLE(device, vkDestroySwapchainKHR, m_handle, nullptr)
    }
}