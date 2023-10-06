#pragma once
#include <rhi/ez_vulkan.h>

class Renderer;

class VisibilityBufferShadingPass
{
public:
    VisibilityBufferShadingPass(Renderer* renderer);

    ~VisibilityBufferShadingPass();

    void render();

private:
    Renderer* _renderer;
    EzSampler _sampler = VK_NULL_HANDLE;
};