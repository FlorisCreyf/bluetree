/* Plant Genererator
 * Copyright (C) 2017  Floris Creyf
 *
 * Plant Genererator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Plant Genererator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SHARED_RESOURCES_H
#define SHARED_RESOURCES_H

#define GL_GLEXT_PROTOTYPES

#include <QOpenGLFunctions>

enum Shader {
	Model = 0,
	Point = 1,
	Wire = 2,
	Line = 3,
	Flat = 4
};

class SharedResources : protected QOpenGLFunctions {
public:
	enum Texture {
		DotTexture
	};

	void initialize();
	GLuint getShader(Shader shader);
	GLuint getTexture(Texture texture);

private:
	GLuint programs[5];
	GLuint textures[1];

	void createPrograms();
	void createTextures();

	bool isCompiled(GLuint name, const char *filename);
	bool openFile(const char *filename, GLchar *&buffer, int &size);
	GLuint buildShader(GLenum type, const char *filename);
	GLuint buildProgram(GLuint *shaders, int size);
};

#endif /* SHARED_RESOURCES_H */