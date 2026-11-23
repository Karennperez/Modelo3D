#ifndef BEZIER_H
#define BEZIER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// Curva de Bezier cubica (4 puntos de control).
// Sabe calcular puntos y tangentes sobre la curva y dibujarse como una linea.
// IMPORTANTE: crear el objeto DESPUES de gladLoadGL(), porque crea buffers de OpenGL.
class Bezier
{
public:
	// segmentos = cantidad de tramos rectos con los que se dibuja la curva
	Bezier(const glm::vec3& p0, const glm::vec3& p1,
		const glm::vec3& p2, const glm::vec3& p3, int segmentos = 100);

	// Los buffers de OpenGL no se pueden copiar
	Bezier(const Bezier&) = delete;
	Bezier& operator=(const Bezier&) = delete;

	// Punto de la curva para t en [0, 1]
	glm::vec3 punto(float t) const;

	// Derivada en t = direccion hacia donde avanza la curva (no esta normalizada)
	glm::vec3 tangente(float t) const;

	// Dibuja la curva como linea (el shader y la matriz model ya deben estar configurados)
	void draw() const;

	// Libera los buffers de OpenGL. Llamar antes de glfwTerminate().
	void liberar();

private:
	glm::vec3 m_p0, m_p1, m_p2, m_p3;
	GLuint m_vao = 0, m_vbo = 0;
	GLsizei m_numVertices = 0;
};

#endif
