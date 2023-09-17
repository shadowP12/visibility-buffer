#pragma once

class Renderer;

class VisibilityBufferShadingPass
{
public:
    VisibilityBufferShadingPass(Renderer* renderer);

    ~VisibilityBufferShadingPass();

    void render();

private:
    Renderer* _renderer;
};