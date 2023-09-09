#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera
{
public:
    Camera();

    glm::mat4 get_proj_matrix();

    glm::mat4 get_view_matrix();

    glm::mat4 get_transform();

    void set_fov(float fov);

    void set_near_far(float near, float far);

    float get_near() const { return _near; }

    float get_far() const { return _far; }

    void set_aspect(float aspect);

    void set_translation(const glm::vec3& translation);

    glm::vec3 get_translation(){ return _translation; }

    void set_scale(const glm::vec3& scale);

    glm::vec3 get_scale(){ return _scale; }

    void set_euler(const glm::vec3& euler);

    glm::vec3 get_euler() { return _euler; }

private:
    void _update_transform();

    bool _proj_dirty = true;
    bool _transform_dirty = true;
    float _fov = 45.0f;
    float _near = 0.1f;
    float _far = 100.0f;
    float _aspect = 1.0f;
    glm::vec3 _translation;
    glm::vec3 _scale;
    glm::vec3 _euler;
    glm::mat4 _proj_matrix;
    glm::mat4 _view_matrix;
    glm::mat4 _transform;
};

