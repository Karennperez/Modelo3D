#include "Bezier.h"
#include <cstddef>

// Mismo formato de vertice que usa el shader del proyecto:
// location 0 = pos, 1 = color, 2 = normal, 3 = tex, 4 = tangent
namespace {
	struct VertexCurva {
		GLfloat pos[3];
		GLfloat color[4];
		GLfloat tex[2];
		GLfloat normal[3];
		GLfloat tangent[3];
	};
}

Bezier::Bezier(const glm::vec3& p0, const glm::vec3& p1,
	const glm::vec3& p2, const glm::vec3& p3, int segmentos)
	: m_p0(p0), m_p1(p1), m_p2(p2), m_p3(p3)
{
	if (segmentos < 1) segmentos = 1;

	// Se muestrea la curva en segmentos+1 puntos para dibujarla como GL_LINE_STRIP
	std::vector<VertexCurva> verts;
	verts.reserve(segmentos + 1);
	for (int i = 0; i <= segmentos; ++i) {
		glm::vec3 p = punto((float)i / (float)segmentos);
		VertexCurva v = {};
		v.pos[0] = p.x; v.pos[1] = p.y; v.pos[2] = p.z;
		v.color[0] = 1.0f; v.color[1] = 1.0f; v.color[2] = 0.0f; v.color[3] = 1.0f;  // amarillo
		v.normal[1] = 1.0f;
		v.tangent[0] = 1.0f;
		verts.push_back(v);
	}
	m_numVertices = (GLsizei)verts.size();

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VertexCurva), verts.data(), GL_STATIC_DRAW);

	GLsizei stride = (GLsizei)sizeof(VertexCurva);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexCurva, pos));     glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexCurva, color));   glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexCurva, normal));  glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexCurva, tex));     glEnableVertexAttribArray(3);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexCurva, tangent)); glEnableVertexAttribArray(4);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

glm::vec3 Bezier::punto(float t) const
{
	float u = 1.0f - t;
	return u * u * u * m_p0
		+ 3.0f * u * u * t * m_p1
		+ 3.0f * u * t * t * m_p2
		+ t * t * t * m_p3;
}

glm::vec3 Bezier::tangente(float t) const
{
	float u = 1.0f - t;
	return 3.0f * u * u * (m_p1 - m_p0)
		+ 6.0f * u * t * (m_p2 - m_p1)
		+ 3.0f * t * t * (m_p3 - m_p2);
}

void Bezier::draw() const
{
	glBindVertexArray(m_vao);
	glDrawArrays(GL_LINE_STRIP, 0, m_numVertices);
	glBindVertexArray(0);
}

void Bezier::liberar()
{
	if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
	if (m_vbo) { glDeleteBuffers(1, &m_vbo);      m_vbo = 0; }
}