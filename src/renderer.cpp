#include "renderer.h"
#include "camera.h"
#include "scene.h"
#include "rsg.h"

Renderer::Renderer()
{
    init_rsg();

    EzBufferDesc buffer_desc{};
    buffer_desc.size = sizeof(ViewBufferType);
    buffer_desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_desc.memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ez_create_buffer(buffer_desc, _view_buffer);
}

Renderer::~Renderer()
{
    uninit_rsg();

    if (_view_buffer)
        ez_destroy_buffer(_view_buffer);
    if (_color_rt)
        ez_destroy_texture(_color_rt);
    if(_depth_rt)
        ez_destroy_texture(_depth_rt);
}

void Renderer::set_scene(Scene* scene)
{
    if (_scene != scene)
    {
        _scene = scene;
        _scene_dirty = true;
    }
}

void Renderer::set_camera(Camera* camera)
{
    _camera = camera;
}

void Renderer::update_rendertarget()
{
    EzTextureDesc desc{};
    desc.width = _width;
    desc.height = _height;
    desc.format = VK_FORMAT_B8G8R8A8_UNORM;
    desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    if (_color_rt)
        ez_destroy_texture(_color_rt);
    ez_create_texture(desc, _color_rt);
    ez_create_texture_view(_color_rt, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

    desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
    desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (_depth_rt)
        ez_destroy_texture(_depth_rt);
    ez_create_texture(desc, _depth_rt);
    ez_create_texture_view(_depth_rt, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);
}

void Renderer::update_view_buffer()
{
    glm::mat4 proj_matrix = _camera->get_proj_matrix();
    glm::mat4 view_matrix = _camera->get_view_matrix();

    ViewBufferType view_buffer_type{};
    view_buffer_type.view_matrix = view_matrix;
    view_buffer_type.proj_matrix = proj_matrix;

    VkBufferMemoryBarrier2 barrier = ez_buffer_barrier(_view_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);

    ez_update_buffer(_view_buffer, sizeof(ViewBufferType), 0, &view_buffer_type);

    VkPipelineStageFlags2 stage_flags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkAccessFlags2 access_flags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier = ez_buffer_barrier(_view_buffer, stage_flags, access_flags);
    ez_pipeline_barrier(0, 1, &barrier, 0, nullptr);
}

void Renderer::render(EzSwapchain swapchain)
{
    if (!_scene || !_camera)
        return;

    if (swapchain->width == 0 || swapchain->height ==0)
        return;

    if (_width != swapchain->width || _height != swapchain->height)
    {
        _width = swapchain->width;
        _height = swapchain->height;
        update_rendertarget();
    }
    if (_scene_dirty)
    {
        _scene_dirty = false;
    }
    update_view_buffer();

    // Copy to swapchain
    EzTexture src_rt = _color_rt;
    VkImageMemoryBarrier2 copy_barriers[] = {
        ez_image_barrier(src_rt, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
        ez_image_barrier(swapchain, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
    };
    ez_pipeline_barrier(0, 0, nullptr, 2, copy_barriers);

    VkImageCopy copy_region = {};
    copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.srcSubresource.layerCount = 1;
    copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.dstSubresource.layerCount = 1;
    copy_region.extent = { swapchain->width, swapchain->height, 1 };
    ez_copy_image(src_rt, swapchain, copy_region);

    _frame_number++;
}
