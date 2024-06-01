#include "scene.h"
#include "scene_importer.h"
#include "camera.h"
#include "camera_controller.h"
#include "renderer.h"
#include <core/path.h>
#include <core/io/file_access.h>
#include <input/input_events.h>
#include <rhi/ez_vulkan.h>
#include <rhi/rhi_shader_mgr.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

static void window_size_callback(GLFWwindow* window, int w, int h)
{
}

static void cursor_position_callback(GLFWwindow* window, double pos_x, double pos_y)
{
    MouseEvent mouse_event;
    mouse_event.type = MouseEvent::Type::MOVE;
    mouse_event.x = (float)pos_x;
    mouse_event.y = (float)pos_y;
    Input::get_mouse_event().broadcast(mouse_event);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    double pos_x, pos_y;
    glfwGetCursorPos(window, &pos_x, &pos_y);

    if (action == 0)
    {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::Type::UP;
        mouse_event.x = (float)pos_x;
        mouse_event.y = (float)pos_y;
        mouse_event.button = button;
        Input::get_mouse_event().broadcast(mouse_event);
    }
    else if (action == 1)
    {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::Type::DOWN;
        mouse_event.x = (float)pos_x;
        mouse_event.y = (float)pos_y;
        mouse_event.button = button;
        Input::get_mouse_event().broadcast(mouse_event);
    }
}

static void mouse_scroll_callback(GLFWwindow* window, double offset_x, double offset_y)
{
    MouseEvent mouse_event;
    mouse_event.type = MouseEvent::Type::WHEEL;
    mouse_event.offset_x = (float)offset_x;
    mouse_event.offset_y = (float)offset_y;
    Input::get_mouse_event().broadcast(mouse_event);
}

int main()
{
    // Path settings
    Path::register_protocol("content", std::string(PROJECT_DIR) + "/content/");
    Path::register_protocol("scene", std::string(PROJECT_DIR) + "/content/scene/");
    Path::register_protocol("shader", std::string(PROJECT_DIR) + "/content/shader/");

    ez_init();
    rhi_shader_mgr_init();
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* glfw_window = glfwCreateWindow(1024, 768, "visibility-buffer", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(glfw_window, window_size_callback);
    glfwSetCursorPosCallback(glfw_window, cursor_position_callback);
    glfwSetMouseButtonCallback(glfw_window, mouse_button_callback);
    glfwSetScrollCallback(glfw_window, mouse_scroll_callback);
    EzSwapchain swapchain = VK_NULL_HANDLE;
    ez_create_swapchain(glfwGetWin32Window(glfw_window), swapchain);

    Camera* camera = new Camera();
    camera->set_aspect(800.0f/600.0f);
    camera->set_translation(glm::vec3(1.28223431f, 13.497385f, -5.47421837f));
    camera->set_euler(glm::vec3(-1.66900015f, -0.0499999598f, 0.0f));
    CameraController* camera_controller = new CameraController();
    camera_controller->set_camera(camera);
    Scene* scene = load_scene("scene://dragon/dragon.gltf");
    Renderer* renderer = new Renderer();
    renderer->set_scene(scene);
    renderer->set_camera(camera);

    while (!glfwWindowShouldClose(glfw_window))
    {
        glfwPollEvents();

        EzSwapchainStatus swapchain_status = ez_update_swapchain(swapchain);

        if (swapchain_status == EzSwapchainStatus::NotReady)
            continue;

        if (swapchain_status == EzSwapchainStatus::Resized)
        {
            camera->set_aspect(swapchain->width/swapchain->height);
        }

        ez_acquire_next_image(swapchain);

        renderer->render(swapchain);

        VkImageMemoryBarrier2 present_barrier[] = { ez_image_barrier(swapchain, EZ_RESOURCE_STATE_PRESENT) };
        ez_pipeline_barrier(0, 0, nullptr, 1, present_barrier);

        ez_present(swapchain);

        ez_submit();
    }

    delete renderer;
    delete scene;
    delete camera;
    delete camera_controller;

    ez_destroy_swapchain(swapchain);
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    rhi_shader_mgr_terminate();
    ez_flush();
    ez_terminate();
    return 0;
}