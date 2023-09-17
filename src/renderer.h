#pragma once
#include <rhi/ez_vulkan.h>
#include <glm/glm.hpp>

class Scene;
class Camera;

struct ViewBufferType
{
    glm::mat4 view_matrix;
    glm::mat4 proj_matrix;
};

class Renderer
{
public:
    Renderer();

    ~Renderer();

    void render(EzSwapchain swapchain);

    void set_scene(Scene* scene);

    void set_camera(Camera* camera);

private:
    void update_rendertarget();

    void update_view_buffer();

    uint32_t _width = 0;
    uint32_t _height = 0;
    uint64_t _frame_number = 0;
    Scene* _scene;
    bool _scene_dirty = true;
    Camera* _camera;
    EzBuffer _view_buffer = VK_NULL_HANDLE;
    EzTexture _color_rt = VK_NULL_HANDLE;
    EzTexture _depth_rt = VK_NULL_HANDLE;
    friend class TriangleFilteringPass;
    TriangleFilteringPass* _triangle_filtering_pass = nullptr;
    friend class VisibilityBufferPass;
    VisibilityBufferPass* _visibility_buffer_pass = nullptr;
    friend class VisibilityBufferShadingPass;
    VisibilityBufferShadingPass* _visibility_buffer_shading_pass = nullptr;
};