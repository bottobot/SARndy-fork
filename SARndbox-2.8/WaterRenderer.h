#ifndef WATERRENDERER_H
#define WATERRENDERER_H

#include <GL/glew.h>

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    void render();  // Main rendering method
    void setFractalTexture(GLuint fractalTexture);  // Method to accept fractal texture

private:
    GLuint waterShader;  // Shader program for water rendering
    GLuint fractalTexture;  // Texture ID for fractal

    // Existing members and methods
    void bindWaterShader();  // Helper method to bind the shader program
};

#endif // WATERRENDERER_H
