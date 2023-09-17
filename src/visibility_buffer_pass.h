#pragma once

class Renderer;

class VisibilityBufferPass
{
public:
    VisibilityBufferPass(Renderer* renderer);

    ~VisibilityBufferPass();

    void render();

private:
    Renderer* _renderer;
};