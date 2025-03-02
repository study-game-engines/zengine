#pragma once
#include <ZEngineDef.h>
#include <Rendering/Specifications/ShaderSpecification.h>
#include <Rendering/Buffers/Framebuffer.h>
#include <Rendering/Swapchain.h>

namespace ZEngine::Rendering::Specifications
{
    struct VertexInputBindingSpecification
    {
        uint32_t Stride  = 0;
        uint32_t Rate    = 0;
        uint32_t Binding = 0;
    };

    struct VertexInputAttributeSpecification
    {
        uint32_t    Location = 0;
        uint32_t    Binding  = 0;
        uint32_t    Offset   = 0;
        ImageFormat Format   = ImageFormat::UNDEFINED;
    };

    struct GraphicRendererPipelineSpecification
    {
        bool                                           EnableBlending                     = false;
        bool                                           EnableDepthTest                    = true;
        bool                                           EnableStencilTest                  = false;
        bool                                           SwapchainAsRenderTarget            = false;
        std::string                                    DebugName                          = {};
        ShaderSpecification                            ShaderSpecification                = {};
        Ref<Rendering::Buffers::FramebufferVNext>      TargetFrameBuffer                  = {};
        Ref<Rendering::Swapchain>                      SwapchainRenderTarget              = {};
        std::vector<VertexInputBindingSpecification>   VertexInputBindingSpecifications   = {};
        std::vector<VertexInputAttributeSpecification> VertexInputAttributeSpecifications = {};
    };
} // namespace ZEngine::Rendering::Specifications