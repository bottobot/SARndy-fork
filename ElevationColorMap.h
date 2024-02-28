#ifndef ELEVATIONCOLORMAP_INCLUDED
#define ELEVATIONCOLORMAP_INCLUDED

#include <GL/gl.h>
#include <GL/GLColorMap.h>
#include <GL/GLTextureObject.h>

#include "Types.h"

/* Forward declarations: */
class DepthImageRenderer;

class ElevationColorMap :public GLColorMap, public GLTextureObject
{
	/* Elements: */
private:
	GLfloat texturePlaneEq[4]; // Texture mapping plane equation in GLSL-compatible format
	GLuint fractalTexture; // Texture object for the fractal pattern

	/* Constructors and destructors: */
public:
	ElevationColorMap(const char* heightMapName); // Creates an elevation color map by loading the given height map file

	/* Methods from GLObject: */
	virtual void initContext(GLContextData& contextData) const;

	/* New methods: */
	void load(const char* heightMapName); // Overrides elevation color map by loading the given height map file
	void generateFractalTexture(int size); // Generates a fractal pattern and stores it in a texture
	void calcTexturePlane(const Plane& basePlane); // Calculates the texture mapping plane for the given base plane equation
	void calcTexturePlane(const DepthImageRenderer* depthImageRenderer); // Calculates the texture mapping plane for the given depth image renderer
	void bindTexture(GLContextData& contextData) const; // Binds the elevation color map texture object to the currently active texture unit
	void uploadTexturePlane(GLint location) const; // Uploads the texture mapping plane equation into the GLSL 4-vector at the given uniform location
};

#endif
