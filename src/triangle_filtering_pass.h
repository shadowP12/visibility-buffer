#pragma once

class Renderer;

class TriangleFilteringPass
{
public:
    TriangleFilteringPass(Renderer* renderer);

    ~TriangleFilteringPass();

    void render();

private:
    Renderer* _renderer;
};