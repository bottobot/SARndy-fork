#include "FractalRenderer.h"
#include <iostream>  // For debugging with print statements
#include <fstream>
#include <sstream>
#include <complex>   // For handling complex numbers (Mandelbrot uses these)

FractalRenderer::FractalRenderer(int width, int height)
    : width(width), height(height), maxIterations(1000) {
    // Allocate memory for the fractal image
    fractalImage = new unsigned char[width * height * 3];  // 3 bytes per pixel (RGB)
}

FractalRenderer::~FractalRenderer() {
    // Free allocated memory
    delete[] fractalImage;
}

// Shader loading utility
GLuint FractalRenderer::loadShader(GLenum shaderType, const char* filePath) {
    std::ifstream shaderFile(filePath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderCode = shaderStream.str();
    const char* shaderSource = shaderCode.c_str();

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Error compiling shader: " << filePath << "\n" << infoLog << std::endl;
    }

    return shader;
}

// Method to generate the fractal
void FractalRenderer::generateFractal() {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double realPart = (x - width / 2.0) * 4.0 / width;
            double imagPart = (y - height / 2.0) * 4.0 / height;
            std::complex<double> c(realPart, imagPart);
            std::complex<double> z(0, 0);

            int iteration = 0;
            while (std::abs(z) <= 2 && iteration < maxIterations) {
                z = z * z + c;
                iteration++;
            }

            int pixelIndex = (y * width + x) * 3;
            if (iteration == maxIterations) {
                fractalImage[pixelIndex] = 0;
                fractalImage[pixelIndex + 1] = 0;
                fractalImage[pixelIndex + 2] = 0;
            } else {
                fractalImage[pixelIndex] = (iteration % 256);
                fractalImage[pixelIndex + 1] = (iteration % 256);
                fractalImage[pixelIndex + 2] = (iteration % 256);
            }
        }
    }
}

void FractalRenderer::initOpenGLContext() {
    shaderProgram = glCreateProgram();
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, "FractalShader.vs");
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, "FractalShader.fs");

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Error linking shader program:\n" << infoLog << std::endl;
    }

    // Generate and bind the texture for fractal data
    glGenTextures(1, &fractalTexture);
    glBindTexture(GL_TEXTURE_2D, fractalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Generate framebuffer to render fractal to the texture
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fractalTexture, 0);

    // Check for framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Framebuffer is not complete!" << std::endl;
    }

    // Initialize VAO for fullscreen quad
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
}

// Method to render fractal to the texture
void FractalRenderer::generateFractal() {
    // Bind the framebuffer so that the fractal is rendered to the texture
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);

    // Clear the texture
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the fractal shader program
    glUseProgram(shaderProgram);

    // Bind VAO and render the fractal onto the texture
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Unbind framebuffer to return to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Access the fractal texture
GLuint FractalRenderer::getFractalTexture() {
    return fractalTexture;  // Return the ID of the fractal texture
}

void FractalRenderer::bindFractalTexture() {
    glBindTexture(GL_TEXTURE_2D, fractalTexture);  // Bind the texture for use
}
