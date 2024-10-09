#ifndef FRACTALRENDERER_H
#define FRACTALRENDERER_H

#include <GL/glew.h>

class FractalRenderer {
public:
    FractalRenderer(int width, int height);
    ~FractalRenderer();

    void generateFractal();  // Fractal generation method
    void displayFractal();   // Render/display method
    void initOpenGLContext(); // OpenGL initialization

    // New methods for fractal texture management:
    void bindFractalTexture();  // Bind the fractal texture for use in other rendering
    GLuint getFractalTexture(); // Get texture ID

private:
    int width, height, maxIterations;
    unsigned char* fractalImage;
    GLuint shaderProgram;
    GLuint framebuffer, texture;  // Texture and framebuffer for offscreen rendering

    GLuint vao;  // Vertex Array Object for rendering the fullscreen quad
    GLuint loadShader(GLenum shaderType, const char* filePath);

    // New texture for fractal data:
    GLuint fractalTexture;  // OpenGL texture that will store the fractal

    // Fractal-related parameters (e.g., zoom, center position)
    float zoomLevel = 1.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
};

#endif // FRACTALRENDERER_H
