#include <pch.h>
#include <Rendering/Renderers/Contracts/RendererDataContract.h>
#include <Rendering/Renderers/SceneRenderer.h>
#include <Rendering/Renderers/GraphicRenderer.h>
#include <Rendering/Specifications/GraphicRendererPipelineSpecification.h>

using namespace ZEngine::Rendering::Specifications;

namespace ZEngine::Rendering::Renderers
{
    void SceneRenderer::Initialize(const Ref<Buffers::UniformBufferSet>& camera)
    {
        const auto& renderer_info = Renderers::GraphicRenderer::GetRendererInformation();

        m_upload_once_per_frame_count = renderer_info.FrameCount;
        m_last_drawn_vertices_count.resize(renderer_info.FrameCount, 0);
        m_last_drawn_index_count.resize(renderer_info.FrameCount, 0);

        /*
         * Render Passes definition
         */

        /*
         * Cubmap Pass
         */
        m_CubemapSBVertex   = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_CubemapSBIndex    = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_CubemapSBDrawData = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        for (uint32_t frame_index = 0; frame_index < renderer_info.FrameCount; ++frame_index)
        {
            m_cubemap_indirect_buffer.emplace_back(CreateRef<Buffers::IndirectBuffer>());
        }

        m_environment_map                                                          = Textures::Texture2D::ReadCubemap("Settings/EnvironmentMaps/piazza_bologni_4k.hdr");
        Specifications::GraphicRendererPipelineSpecification cubemap_pipeline_spec = {};
        cubemap_pipeline_spec.DebugName                                            = "Cubemap-Pipeline";
        // cubemap_pipeline_spec.TargetFrameBuffer                                    = GraphicRenderer::GetRenderTarget(RenderTarget::ENVIROMENT_CUBEMAP);
        cubemap_pipeline_spec.TargetFrameBuffer                 = GraphicRenderer::GetRenderTarget(RenderTarget::FRAME_OUTPUT);
        cubemap_pipeline_spec.ShaderSpecification               = {.VertexFilename = "Shaders/Cache/cubemap_vertex.spv", .FragmentFilename = "Shaders/Cache/cubemap_fragment.spv"};
        RenderPasses::RenderPassSpecification cubemap_pass_spec = {};
        cubemap_pass_spec.Pipeline                              = Pipelines::GraphicPipeline::Create(cubemap_pipeline_spec);
        m_cubemap_pass                                          = RenderPasses::RenderPass::Create(cubemap_pass_spec);

        m_cubemap_pass->SetInput("UBCamera", camera);
        m_cubemap_pass->SetInput("VertexSB", m_CubemapSBVertex);
        m_cubemap_pass->SetInput("IndexSB", m_CubemapSBIndex);
        m_cubemap_pass->SetInput("DrawDataSB", m_CubemapSBDrawData);
        m_cubemap_pass->SetInput("CubemapTexture", m_environment_map);
        m_cubemap_pass->Verify();
        m_cubemap_pass->Bake();

        /*
         * Infinite Grid
         */
        m_GridSBVertex   = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_GridSBIndex    = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_GridSBDrawData = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        for (uint32_t frame_index = 0; frame_index < renderer_info.FrameCount; ++frame_index)
        {
            m_infinite_grid_indirect_buffer.emplace_back(CreateRef<Buffers::IndirectBuffer>());
        }
        Specifications::GraphicRendererPipelineSpecification infinite_grid_spec = {};
        infinite_grid_spec.DebugName                                            = "Infinite-Grid-Pipeline";
        infinite_grid_spec.TargetFrameBuffer                                    = GraphicRenderer::GetRenderTarget(RenderTarget::FRAME_OUTPUT);
        infinite_grid_spec.ShaderSpecification = {.VertexFilename = "Shaders/Cache/infinite_grid_vertex.spv", .FragmentFilename = "Shaders/Cache/infinite_grid_fragment.spv"};
        RenderPasses::RenderPassSpecification grid_color_pass = {};
        grid_color_pass.Pipeline                              = Pipelines::GraphicPipeline::Create(infinite_grid_spec);
        m_infinite_grid_pass                                  = RenderPasses::RenderPass::Create(grid_color_pass);

        m_infinite_grid_pass->SetInput("UBCamera", camera);
        m_infinite_grid_pass->SetInput("VertexSB", m_GridSBVertex);
        m_infinite_grid_pass->SetInput("IndexSB", m_GridSBIndex);
        m_infinite_grid_pass->SetInput("DrawDataSB", m_GridSBDrawData);
        m_infinite_grid_pass->Verify();
        m_infinite_grid_pass->Bake();
        /*
         * Final Color
         */
        m_SBVertex       = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_SBIndex        = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_SBDrawData     = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_SBTransform    = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        m_SBMaterialData = CreateRef<Buffers::StorageBufferSet>(renderer_info.FrameCount);
        for (uint32_t frame_index = 0; frame_index < renderer_info.FrameCount; ++frame_index)
        {
            m_indirect_buffer.emplace_back(CreateRef<Buffers::IndirectBuffer>());
        }
        Specifications::FrameBufferSpecificationVNext framebuffer_spec           = {};
        framebuffer_spec.ClearColor                                              = false;
        framebuffer_spec.ClearDepth                                              = false;
        framebuffer_spec.AttachmentSpecifications                                = {ImageFormat::R8G8B8A8_UNORM, ImageFormat::DEPTH_STENCIL_FROM_DEVICE};
        framebuffer_spec.InputColorAttachment[0]                                 = m_infinite_grid_pass->GetOutputColor(0);
        framebuffer_spec.InputColorAttachment[1]                                 = m_infinite_grid_pass->GetOutputDepth();
        Specifications::GraphicRendererPipelineSpecification final_pipeline_spec = {};
        final_pipeline_spec.DebugName                                            = "Standard-Pipeline";
        final_pipeline_spec.TargetFrameBuffer                                    = Buffers::FramebufferVNext::Create(framebuffer_spec);
        final_pipeline_spec.ShaderSpecification          = {.VertexFilename = "Shaders/Cache/final_color_vertex.spv", .FragmentFilename = "Shaders/Cache/final_color_fragment.spv"};
        RenderPasses::RenderPassSpecification color_pass = {};
        color_pass.DebugName                             = "Final-Color-Attachment";
        color_pass.Pipeline                              = Pipelines::GraphicPipeline::Create(final_pipeline_spec);
        m_final_color_output_pass                        = RenderPasses::RenderPass::Create(color_pass);

        m_final_color_output_pass->SetInput("UBCamera", camera);
        m_final_color_output_pass->SetInput("VertexSB", m_SBVertex);
        m_final_color_output_pass->SetInput("IndexSB", m_SBIndex);
        m_final_color_output_pass->SetInput("DrawDataSB", m_SBDrawData);
        m_final_color_output_pass->SetInput("TransformSB", m_SBTransform);
        m_final_color_output_pass->SetInput("MatSB", m_SBMaterialData);
        m_final_color_output_pass->Verify();
        m_final_color_output_pass->Bake();
    }

    void SceneRenderer::Deinitialize()
    {
        m_cubemap_pass->Dispose();
        m_environment_map->Dispose();
        m_CubemapSBVertex->Dispose();
        m_CubemapSBIndex->Dispose();
        m_CubemapSBDrawData->Dispose();
        for (auto& buffer : m_cubemap_indirect_buffer)
        {
            buffer->Dispose();
        }

        m_infinite_grid_pass->Dispose();
        m_GridSBVertex->Dispose();
        m_GridSBIndex->Dispose();
        m_GridSBDrawData->Dispose();
        for (auto& buffer : m_infinite_grid_indirect_buffer)
        {
            buffer->Dispose();
        }

        m_final_color_output_pass->Dispose();

        m_SBVertex->Dispose();
        m_SBIndex->Dispose();
        m_SBDrawData->Dispose();
        m_SBTransform->Dispose();
        m_SBMaterialData->Dispose();
        for (auto& buffer : m_indirect_buffer)
        {
            buffer->Dispose();
        }

        m_global_texture_buffer_collection.clear();
        m_global_texture_buffer_collection.shrink_to_fit();
    }

    void SceneRenderer::StartScene(const glm::vec4& camera_position, const glm::mat4& camera_view, const glm::mat4& camera_projection)
    {
        m_camera_position   = camera_position;
        m_camera_view       = camera_view;
        m_camera_projection = camera_projection;
    }

    void SceneRenderer::StartScene(Buffers::CommandBuffer* const command_buffer)
    {
        command_buffer->Begin();
    }

    void SceneRenderer::StartScene(const glm::vec3& camera_position, const glm::mat4& camera_view, const glm::mat4& camera_projection)
    {
        this->StartScene(glm::vec4(camera_position, 1.0f), camera_view, camera_projection);
    }

    void SceneRenderer::RenderScene(const Ref<Rendering::Scenes::SceneRawData>& scene_data, uint32_t current_frame_index)
    {
        if (m_upload_once_per_frame_count > 0)
        {
            /*
             * Uploading Cubemap
             */
            {
                auto& vertex_buffer    = *m_CubemapSBVertex;
                auto& index_buffer     = *m_CubemapSBIndex;
                auto& draw_data_buffer = *m_CubemapSBDrawData;

                vertex_buffer[current_frame_index].SetData(m_cubemap_vertex_data);
                index_buffer[current_frame_index].SetData(m_cubemap_index_data);
                draw_data_buffer[current_frame_index].SetData(m_cubmap_draw_data);
                m_cubemap_indirect_buffer[current_frame_index]->SetData(m_cubemap_indirect_commmand);
            }

            /*
             * Uploading Infinite Grid
             */
            {
                auto& vertex_storage    = *m_GridSBVertex;
                auto& index_storage     = *m_GridSBIndex;
                auto& draw_data_storage = *m_GridSBDrawData;

                vertex_storage[current_frame_index].SetData(m_grid_vertices);
                index_storage[current_frame_index].SetData(m_grid_indices);
                draw_data_storage[current_frame_index].SetData(m_grid_drawData);
                m_infinite_grid_indirect_buffer[current_frame_index]->SetData(m_grid_indirect_commmand);
            }

            m_cubemap_pass->MarkDirty();
            m_infinite_grid_pass->MarkDirty();

            --m_upload_once_per_frame_count;
        }

        const auto& sceneNodeMeshMap = scene_data->SceneNodeMeshMap;
        /*
         * Composing Transform Data
         */
        std::vector<glm::mat4> tranform_collection = {};
        for (const auto& sceneNodeMeshPair : sceneNodeMeshMap)
        {
            tranform_collection.push_back(scene_data->GlobalTransformCollection[sceneNodeMeshPair.first]);
        }
        auto& transform_storage = *m_SBTransform;
        transform_storage[current_frame_index].SetData(tranform_collection);

        /*
         * Scenes Textures
         */
        if (m_last_uploaded_buffer_image_count != scene_data->TextureCollection->Size())
        {
            auto& scene_textures_collection = scene_data->TextureCollection;
            m_final_color_output_pass->SetInput("TextureArray", scene_textures_collection);
            m_last_uploaded_buffer_image_count = scene_textures_collection->Size();

            m_final_color_output_pass->MarkDirty();
        }
        /*
         * Scene Draw data
         */
        if ((m_last_drawn_vertices_count[current_frame_index] == scene_data->Vertices.size()) || (m_last_drawn_index_count[current_frame_index] == scene_data->Indices.size()))
        {
            return;
        }

        std::vector<DrawData>             draw_data_collection = {};
        std::vector<Meshes::MeshMaterial> material_collection  = {};

        uint32_t data_index = 0;
        for (const auto& sceneNodeMeshPair : sceneNodeMeshMap)
        {
            /*
             * Composing DrawData
             */
            DrawData& draw_data      = draw_data_collection.emplace_back(DrawData{.Index = data_index});
            draw_data.TransformIndex = data_index;
            draw_data.VertexOffset   = sceneNodeMeshPair.second.VertexOffset;
            draw_data.IndexOffset    = sceneNodeMeshPair.second.IndexOffset;
            draw_data.VertexCount    = sceneNodeMeshPair.second.VertexCount;
            draw_data.IndexCount     = sceneNodeMeshPair.second.IndexCount;
            /*
             * Material data
             */
            material_collection.push_back(scene_data->SceneNodeMaterialMap[sceneNodeMeshPair.first]);
            draw_data.MaterialIndex = material_collection.size() - 1;

            data_index++;
        }
        /*
         * Uploading Geometry data
         */
        auto& vertex_storage = *m_SBVertex;
        auto& index_storage  = *m_SBIndex;
        vertex_storage[current_frame_index].SetData(scene_data->Vertices);
        index_storage[current_frame_index].SetData(scene_data->Indices);
        /*
         * Uploading Drawing data
         */
        auto& draw_data_storage = *m_SBDrawData;
        draw_data_storage[current_frame_index].SetData(draw_data_collection);
        /*
         * Uploading Material data
         */
        auto& material_data_storage = *m_SBMaterialData;
        material_data_storage[current_frame_index].SetData(material_collection);
        /*
         * Uploading Indirect Commands
         */
        std::vector<VkDrawIndirectCommand> draw_indirect_commmand = {};
        draw_indirect_commmand.resize(draw_data_collection.size());
        for (uint32_t i = 0; i < draw_indirect_commmand.size(); ++i)
        {
            draw_indirect_commmand[i] = {
                .vertexCount   = draw_data_collection[i].IndexCount,
                .instanceCount = 1,
                .firstVertex   = 0,
                .firstInstance = i,
            };
        }

        m_indirect_buffer[current_frame_index]->SetData(draw_indirect_commmand);

        /*
         * Caching last vertex/index count per frame
         */
        m_last_drawn_vertices_count[current_frame_index] = scene_data->Vertices.size();
        m_last_drawn_index_count[current_frame_index]    = scene_data->Indices.size();

        /*
         * Mark RenderPass dirty and should re-upadte inputs
         */
        m_final_color_output_pass->MarkDirty();
    }

    void SceneRenderer::EndScene(Buffers::CommandBuffer* const command_buffer, uint32_t current_frame_index)
    {
        command_buffer->BeginRenderPass(m_cubemap_pass);
        command_buffer->BindDescriptorSets(current_frame_index);
        command_buffer->DrawIndirect(m_cubemap_indirect_buffer[current_frame_index]);
        command_buffer->EndRenderPass();

        // command_buffer->BeginRenderPass(m_infinite_grid_pass);
        // command_buffer->BindDescriptorSets(current_frame_index);
        // command_buffer->DrawIndirect(m_infinite_grid_indirect_buffer[current_frame_index]);
        // command_buffer->EndRenderPass();

        // command_buffer->BeginRenderPass(m_final_color_output_pass);
        // command_buffer->BindDescriptorSets(current_frame_index);
        // command_buffer->DrawIndirect(m_indirect_buffer[current_frame_index]);
        // command_buffer->EndRenderPass();

        command_buffer->End();
    }

    void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
    {
        m_cubemap_pass->ResizeRenderTarget(width, height);
        m_infinite_grid_pass->ResizeRenderTarget(width, height);
        m_final_color_output_pass->ResizeRenderTarget(width, height);
    }

} // namespace ZEngine::Rendering::Renderers
