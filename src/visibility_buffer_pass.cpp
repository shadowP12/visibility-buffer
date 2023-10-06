#include "visibility_buffer_pass.h"
#include "triangle_filtering_pass.h"
#include "renderer.h"
#include "scene.h"
#include <rhi/shader_manager.h>

VisibilityBufferPass::VisibilityBufferPass(Renderer* renderer)
{
    _renderer = renderer;
}

VisibilityBufferPass::~VisibilityBufferPass()
{

}

void VisibilityBufferPass::render()
{
    EzBuffer vertex_buffer = _renderer->_scene->position_buffer;
    EzBuffer index_buffer = _renderer->_triangle_filtering_pass->get_index_buffer();
    EzBuffer draw_command_buffer = _renderer->_scene->draw_command_buffer;//_renderer->_triangle_filtering_pass->get_draw_command_buffer();
    uint32_t draw_count = _renderer->_triangle_filtering_pass->get_draw_count();

    ez_reset_pipeline_state();

    VkImageMemoryBarrier2 tex_barriers[2];
    tex_barriers[0] = ez_image_barrier(_renderer->_vb_rt, EZ_RESOURCE_STATE_RENDERTARGET);
    tex_barriers[1] = ez_image_barrier(_renderer->_depth_rt, EZ_RESOURCE_STATE_DEPTH_WRITE);
    VkBufferMemoryBarrier2 buf_barriers[2];
    buf_barriers[0] = ez_buffer_barrier(draw_command_buffer, EZ_RESOURCE_STATE_INDIRECT_ARGUMENT | EZ_RESOURCE_STATE_SHADER_RESOURCE);
    buf_barriers[1] = ez_buffer_barrier(index_buffer, EZ_RESOURCE_STATE_INDEX_BUFFER | EZ_RESOURCE_STATE_SHADER_RESOURCE);
    ez_pipeline_barrier(0, 2, buf_barriers, 2, tex_barriers);

    EzRenderingAttachmentInfo color_info{};
    color_info.texture = _renderer->_vb_rt;
    color_info.clear_value.color = {1.0f, 1.0f, 1.0f, 1.0f};

    EzRenderingAttachmentInfo depth_info{};
    depth_info.texture = _renderer->_depth_rt;
    depth_info.clear_value.depthStencil = {1.0f, 0};

    EzRenderingInfo rendering_info{};
    rendering_info.width = _renderer->_width;
    rendering_info.height = _renderer->_height;
    rendering_info.colors.push_back(color_info);
    rendering_info.depth.push_back(depth_info);
    ez_begin_rendering(rendering_info);

    ez_set_viewport(0, 0, (float)_renderer->_width, (float)_renderer->_height);
    ez_set_scissor(0, 0, (int32_t)_renderer->_width, (int32_t)_renderer->_height);

    ez_set_vertex_shader(ShaderManager::get()->get_shader("shader://visibility_buffer_pass.vert"));
    ez_set_fragment_shader(ShaderManager::get()->get_shader("shader://visibility_buffer_pass.frag"));

    ez_bind_buffer(0, _renderer->_view_buffer, _renderer->_view_buffer->size);

    ez_set_vertex_binding(0, 12);
    ez_set_vertex_attrib(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

    ez_set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    ez_bind_vertex_buffer(vertex_buffer);
    ez_bind_index_buffer(index_buffer, VK_INDEX_TYPE_UINT32);
    ez_draw_indexed_indirect(draw_command_buffer, 0, draw_count, sizeof(VkDrawIndexedIndirectCommand));

    ez_end_rendering();

    buf_barriers[0] = ez_buffer_barrier(draw_command_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    buf_barriers[1] = ez_buffer_barrier(index_buffer, EZ_RESOURCE_STATE_UNORDERED_ACCESS);
    ez_pipeline_barrier(0, 2, buf_barriers, 0, nullptr);
}