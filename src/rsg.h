#pragma once

#include <rhi/ez_vulkan.h>

class RenderingServerGlobals
{
public:
    static EzBuffer quad_buffer;
    static EzBuffer cube_buffer;
};

#define RSG RenderingServerGlobals

void init_rsg();
void uninit_rsg();