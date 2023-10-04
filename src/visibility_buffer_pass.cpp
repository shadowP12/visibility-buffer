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
    ez_reset_pipeline_state();

    VkImageMemoryBarrier2 barriers[1];
    barriers[0] = ez_image_barrier(_renderer->_vb_rt, EZ_RESOURCE_STATE_RENDERTARGET);
    ez_pipeline_barrier(0, 0, nullptr, 1, barriers);

    EzRenderingAttachmentInfo color_info{};
    color_info.texture = _renderer->_vb_rt;
    color_info.clear_value.color = {1.0f, 1.0f, 1.0f, 1.0f};
    EzRenderingInfo rendering_info{};
    rendering_info.width = _renderer->_width;
    rendering_info.height = _renderer->_height;
    rendering_info.colors.push_back(color_info);
    ez_begin_rendering(rendering_info);

    ez_set_viewport(0, 0, (float)_renderer->_width, (float)_renderer->_height);
    ez_set_scissor(0, 0, (int32_t)_renderer->_width, (int32_t)_renderer->_height);

    ez_set_vertex_shader(ShaderManager::get()->get_shader("shader://visibility_buffer_pass.vert"));
    ez_set_fragment_shader(ShaderManager::get()->get_shader("shader://visibility_buffer_pass.frag"));

    ez_bind_buffer(0, _renderer->_view_buffer, _renderer->_view_buffer->size);

    ez_set_vertex_binding(0, 12);
    ez_set_vertex_attrib(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

    ez_set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    EzBuffer vertex_buffer = _renderer->_scene->position_buffer;
    EzBuffer index_buffer = _renderer->_triangle_filtering_pass->get_index_buffer();
    EzBuffer draw_command_buffer = _renderer->_triangle_filtering_pass->get_draw_command_buffer();
    uint32_t draw_count = _renderer->_triangle_filtering_pass->get_draw_count();
    ez_bind_vertex_buffer(vertex_buffer);
    ez_bind_index_buffer(index_buffer, VK_INDEX_TYPE_UINT16);
    ez_draw_indexed_indirect(draw_command_buffer, 0, draw_count, sizeof(VkDrawIndexedIndirectCommand));

    ez_end_rendering();
}