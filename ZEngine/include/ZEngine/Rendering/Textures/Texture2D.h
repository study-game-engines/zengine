#pragma once
#include <future>
#include <Rendering/Textures/Texture.h>
#include <Rendering/Buffers/Image2DBuffer.h>
#include <Rendering/Specifications/TextureSpecification.h>

namespace ZEngine::Rendering::Textures
{

    class Texture2D : public Texture
    {
    public:
        Texture2D() = default;
        virtual ~Texture2D();

        static Ref<Texture2D>              Create(Specifications::TextureSpecification& spec);
        static Ref<Texture2D>              Create(uint32_t width = 1, uint32_t height = 1);
        static Ref<Texture2D>              Create(uint32_t width, uint32_t height, float r, float g, float b, float a);
        static Ref<Texture2D>              Read(std::string_view filename);
        static Ref<Texture2D>              ReadCubemap(std::string_view filename);
        static std::future<Ref<Texture2D>> ReadAsync(std::string_view filename);

        virtual Hardwares::BufferImage&       GetBuffer() override;
        virtual const Hardwares::BufferImage& GetBuffer() const override;
        Ref<Buffers::Image2DBuffer>           GetImage2DBuffer() const;

        virtual void Dispose() override;

    protected:
        static void FillAsVulkanImage(Ref<Texture2D>& texture, const Specifications::TextureSpecification& specification);

    private:
        Ref<Buffers::Image2DBuffer> m_image_2d_buffer;
    };
} // namespace ZEngine::Rendering::Textures
