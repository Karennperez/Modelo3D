// cout/cerr para mensajes en consola
#include <iostream>
// carga las funciones de OpenGL
#include <glad/glad.h>
// ventana, contexto y entrada (teclado/mouse)
#include <GLFW/glfw3.h>
// vectores y matrices
#include <glm/glm.hpp>
// translate, rotate, scale
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <cstddef>
#include <vector>
#include <cstdint>
#include <cfloat>
#include <cmath>

// compila la implementacion de stb_image en este archivo
#define STB_IMAGE_IMPLEMENTATION
// carga de imagenes (jpg/png)
#include "stb/stb_image.h"

// compila la implementacion de tinygltf en este archivo
#define TINYGLTF_IMPLEMENTATION
// no vuelve a incluir stb_image
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
// no usa stb_image_write
#define TINYGLTF_NO_STB_IMAGE_WRITE
// lector de modelos glTF
#include <tiny_gltf.h>

#include "Shaders.h"
#include "Origin.h"
#include "Camara.h"
#include "Bezier.h"

// formato de cada vertice que se envia a la GPU
struct VertexTriangle {
	GLfloat pos[3];
	GLfloat color[4];   // RGBA
	GLfloat tex[2];     //Coordenadas de textura
	GLfloat normal[3];
	GLfloat tangent[3];
};

// alias del nombre del vertice
using Vertex3angle = VertexTriangle;

// Una "primitiva" del modelo: un VAO listo para dibujar con su textura
struct GltfPrimitive {
	GLuint vao = 0, vbo = 0, ebo = 0;
	// cantidad de indices a dibujar
	GLsizei count = 0;
	GLuint tex = 0;
};
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
	{
		// ajusta el area de dibujo al nuevo tamano de la ventana
		glViewport(0, 0, width, height);
	}

	// Carga una imagen en una textura ya creada
	static void loadTexture(const char* path, GLuint texture)
	{
		// voltea la imagen para que coincida con el eje Y de OpenGL
		stbi_set_flip_vertically_on_load(1);

		// ancho, alto y numero de canales de la imagen
		int w, h, channels;
		// lee el archivo de imagen
		unsigned char* data = stbi_load(path, &w, &h, &channels, 0);
		// si no se pudo cargar
		if (!data) {
			std::cerr << "Failed to load texture: " << path << std::endl;
			// pixel magenta de respaldo
			unsigned char fallback[4] = { 255, 0, 255, 255 };
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// sale de la funcion
			return;
		}

		// formato por defecto
		GLenum format = GL_RGB;
		if (channels == 1) format = GL_RED;
		else if (channels == 3) format = GL_RGB;
		else if (channels == 4) format = GL_RGBA;

		// selecciona la textura
		glBindTexture(GL_TEXTURE_2D, texture);
		// filas sin relleno de bytes
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		// sube los pixeles a la GPU
		glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
		// genera versiones reducidas (mipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

		// no repite la textura en u
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		// no repite la textura en v
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// filtro al achicar, usando mipmaps
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		// filtro al agrandar
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// libera la imagen de la RAM (ya esta en la GPU)
		stbi_image_free(data);
	}

	// Devuelve el puntero al primer dato de un accessor y su stride en bytes
	static const unsigned char* accessorPtr(const tinygltf::Model& m, int accIdx, size_t& stride)
	{
		// accessor: describe un tipo de dato (posiciones, normales...)
		const auto& acc = m.accessors[accIdx];
		// bufferView: el trozo del buffer donde estan esos datos
		const auto& bv = m.bufferViews[acc.bufferView];
		// bytes que hay entre un elemento y el siguiente
		stride = acc.ByteStride(bv);
		// direccion del primer dato dentro del buffer
		return m.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
	}

	// Carga un .gltf / .glb y crea un VAO por cada primitiva de triangulos
	static bool loadGltf(const std::string& path, std::vector<GltfPrimitive>& out, glm::vec3& bmin, glm::vec3& bmax)
	{
		// esquina minima inicial: un valor muy grande
		bmin = glm::vec3(FLT_MAX);
		// esquina maxima inicial: un valor muy pequeno
		bmax = glm::vec3(-FLT_MAX);

		//guardado el modelo leido
		tinygltf::Model model;
		// LEE EL glTF
		tinygltf::TinyGLTF loader;
		// mensajes de error y de aviso
		std::string err, warn;

		// si termina en .glb lo lee como binario, si no como .gltf de texto
		bool ok = (path.size() >= 4 && path.substr(path.size() - 4) == ".glb")
			? loader.LoadBinaryFromFile(&model, &err, &warn, path)
			: loader.LoadASCIIFromFile(&model, &err, &warn, path);

		// muestra los avisos si hay
		if (!warn.empty()) std::cout << "glTF warn: " << warn << std::endl;
		if (!err.empty())  std::cerr << "glTF error: " << err << std::endl;
		if (!ok) return false;

		// recorre cada malla del modelo
		for (const auto& mesh : model.meshes) {
			// recorre cada primitiva de la malla
			for (const auto& prim : mesh.primitives) {
				// solo se aceptan triangulos
				if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
				// la primitiva debe tener posiciones
				if (!prim.attributes.count("POSITION")) continue;

				// --- vertices ---
				// accessor de las posiciones
				int posAcc = prim.attributes.at("POSITION");
				// stride de las posiciones
				size_t posStride;
				// puntero a los datos de posiciones
				const unsigned char* pos = accessorPtr(model, posAcc, posStride);
				// numero de vertices
				size_t n = model.accessors[posAcc].count;

				// strides de normales, uv y tangentes
				size_t nStride = 0, uvStride = 0, tStride = 0;
				// datos de normales (nullptr si no hay)
				const unsigned char* nor = nullptr;
				// datos de coordenadas de textura (nullptr si no hay)
				const unsigned char* uv = nullptr;
				// datos de tangentes (nullptr si no hay)
				const unsigned char* tan = nullptr;
				// lee las normales si existen
				if (prim.attributes.count("NORMAL"))     nor = accessorPtr(model, prim.attributes.at("NORMAL"), nStride);
				// lee las uv si existen
				if (prim.attributes.count("TEXCOORD_0")) uv = accessorPtr(model, prim.attributes.at("TEXCOORD_0"), uvStride);
				// lee las tangentes si existen
				if (prim.attributes.count("TANGENT"))    tan = accessorPtr(model, prim.attributes.at("TANGENT"), tStride);

				// arreglo de vertices que se va a llenar
				std::vector<Vertex3angle> verts(n);
				// recorre cada vertice
				for (size_t i = 0; i < n; ++i) {
					// referencia al vertice actual
					Vertex3angle& v = verts[i];
					// posicion i en bruto
					const float* p = (const float*)(pos + i * posStride);
					// copia la posicion
					v.pos[0] = p[0]; v.pos[1] = p[1]; v.pos[2] = p[2];
					// actualiza la esquina minima de la caja del modelo
					bmin = glm::min(bmin, glm::vec3(p[0], p[1], p[2]));
					// actualiza la esquina maxima de la caja del modelo
					bmax = glm::max(bmax, glm::vec3(p[0], p[1], p[2]));
					// color blanco opaco
					v.color[0] = v.color[1] = v.color[2] = v.color[3] = 1.0f;

					// si hay uv, las copia
					if (uv) { const float* t = (const float*)(uv + i * uvStride); v.tex[0] = t[0]; v.tex[1] = t[1]; }
					// si no hay uv, usa (0,0)
					else { v.tex[0] = v.tex[1] = 0.0f; }

					// si hay normal, la copia
					if (nor) { const float* q = (const float*)(nor + i * nStride); v.normal[0] = q[0]; v.normal[1] = q[1]; v.normal[2] = q[2]; }
					// si no hay normal, usa una que apunta hacia arriba
					else { v.normal[0] = 0.0f; v.normal[1] = 1.0f; v.normal[2] = 0.0f; }

					// si hay tangente, la copia
					if (tan) { const float* q = (const float*)(tan + i * tStride); v.tangent[0] = q[0]; v.tangent[1] = q[1]; v.tangent[2] = q[2]; }
					// si no hay tangente, usa el eje X
					else { v.tangent[0] = 1.0f; v.tangent[1] = 0.0f; v.tangent[2] = 0.0f; }
				}

				// --- indices (se convierten todos a uint32) ---
				// lista de indices
				std::vector<uint32_t> idx;
				// si el modelo trae indices
				if (prim.indices >= 0) {
					// accessor de los indices
					const auto& acc = model.accessors[prim.indices];
					// stride de los indices
					size_t iStride;
					// puntero a los datos de indices
					const unsigned char* ip = accessorPtr(model, prim.indices, iStride);
					// reserva espacio para todos los indices
					idx.resize(acc.count);
					// recorre cada indice
					for (size_t i = 0; i < acc.count; ++i) {
						// direccion del indice i
						const unsigned char* e = ip + i * iStride;
						// segun el tamano original del indice
						switch (acc.componentType) {
							// indice de 8 bits
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  idx[i] = *(const uint8_t*)e;  break;
							// indice de 16 bits
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: idx[i] = *(const uint16_t*)e; break;
							// indice de 32 bits
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   idx[i] = *(const uint32_t*)e; break;
						}
					}
				}
				// si el modelo no trae indices
				else {
					// uno por cada vertice
					idx.resize(n);
					// llena con 0, 1, 2, 3...
					for (size_t i = 0; i < n; ++i) idx[i] = (uint32_t)i;
				}

				// primitiva que se va a guardar
				GltfPrimitive gp;
				// cuantos indices se dibujaran
				gp.count = (GLsizei)idx.size();

				// CREA LOS OBJETOS EN OPENGL PARA PODER CARGAR EL MODELO
				glGenVertexArrays(1, &gp.vao);
				// crea el buffer de vertices
				glGenBuffers(1, &gp.vbo);
				// crea el buffer de indices
				glGenBuffers(1, &gp.ebo);
				// activa el VAO
				glBindVertexArray(gp.vao);

				// activa el VBO
				glBindBuffer(GL_ARRAY_BUFFER, gp.vbo);
				// sube los vertices a la GPU
				glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex3angle), verts.data(), GL_STATIC_DRAW);
				// activa el EBO (queda guardado dentro del VAO)
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gp.ebo);
				// sube los indices a la GPU
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

				// tamano en bytes de un vertice completo
				GLsizei stride = (GLsizei)sizeof(Vertex3angle);
				// location 0: posicion
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex3angle, pos));     glEnableVertexAttribArray(0);
				// location 1: color
				glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex3angle, color));   glEnableVertexAttribArray(1);
				// location 2: normal
				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex3angle, normal));  glEnableVertexAttribArray(2);
				// location 3: coordenadas de textura
				glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex3angle, tex));     glEnableVertexAttribArray(3);
				// location 4: tangente
				glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex3angle, tangent)); glEnableVertexAttribArray(4);

				// desactiva el VAO
				glBindVertexArray(0);
				// desactiva el VBO
				glBindBuffer(GL_ARRAY_BUFFER, 0);

				// --- textura de color del material (si tiene) ---
				// si la primitiva tiene material
				if (prim.material >= 0) {
					// el material de la primitiva
					const auto& mat = model.materials[prim.material];
					// CARGA LA textura de color
					int ti = mat.pbrMetallicRoughness.baseColorTexture.index;
					// si tiene textura de color
					if (ti >= 0) {
						// la imagen ya decodificada por tinygltf
						const auto& img = model.images[model.textures[ti].source];
						// formato segun el numero de canales
						GLenum fmt = (img.component == 4) ? GL_RGBA : (img.component == 3) ? GL_RGB : GL_RED;
						// crea la textura
						glGenTextures(1, &gp.tex);
						// la selecciona
						glBindTexture(GL_TEXTURE_2D, gp.tex);
						// filas sin relleno de bytes
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						// sube los pixeles a la GPU
						glTexImage2D(GL_TEXTURE_2D, 0, fmt, img.width, img.height, 0, fmt, GL_UNSIGNED_BYTE, img.image.data());
						// genera mipmaps
						glGenerateMipmap(GL_TEXTURE_2D);
						// repite la textura en u
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						// repite la textura en v
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						// filtro al achicar, usando mipmaps
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						// filtro al agrandar
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					}
				}

				// guarda la primitiva ya lista en la lista de salida
				out.push_back(gp);
			}
		}
		// devuelve true si se cargo al menos una primitiva
		return !out.empty();
	}

	int main() {
		// inicia GLFW
		glfwInit();
		// version mayor de OpenGL: 3
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		// version menor de OpenGL: 3 (OpenGL 3.3)
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		// perfil core (sin funciones antiguas)
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		// buffer de profundidad de 24 bits
		glfwWindowHint(GLFW_DEPTH_BITS, 24);

		// crea la ventana de 800x600
		GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Window", nullptr, nullptr);
		// si no se pudo crear la ventana
		if (window == nullptr) {
			// avisa el error
			std::cerr << "Failed to create GLFW window" << std::endl;
			// cierra GLFW
			glfwTerminate();
			// sale con error
			return -1;
		}
		// activa el contexto OpenGL de esta ventana
		glfwMakeContextCurrent(window);
		// vsync: sincroniza con la frecuencia del monitor
		glfwSwapInterval(1);
		// avisa cuando cambia el tamano de la ventana
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
		// carga las funciones de OpenGL
		gladLoadGL();

		// activa la prueba de profundidad
		glEnable(GL_DEPTH_TEST);
		// gana el fragmento mas cercano
		glDepthFunc(GL_LESS);

		// compila el vertex y el fragment shader
		Shaders shaders("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
		// id del programa de shaders
		GLuint shaderProgram = shaders.getProgram();
		// activa el programa
		shaders.use(shaderProgram);

		// crea los ejes del origen
		Origin origin;

		// activa la transparencia
		glEnable(GL_BLEND);
		// formula de mezcla segun el alfa
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// --- Modelo glTF (debe ir despues de gladLoadGL) ---
		// lista de primitivas del modelo
		std::vector<GltfPrimitive> modelo;
		// esquinas de la caja que contiene al modelo
		glm::vec3 bmin, bmax;
		// carga el modelo y avisa si falla
		if (!loadGltf("models/puppet/scene.gltf", modelo, bmin, bmax))
			std::cerr << "No se pudo cargar el modelo" << std::endl;

		// Centro y escala automatica: el lado mas grande del modelo mide "tamanoFinal" unidades
		// tamano final del modelo (cambialo para hacerlo mas grande o pequeno)
		const float tamanoFinal = 1.5f;   // cambia este valor para hacerlo mas grande o pequeno
		// centro de la caja del modelo
		glm::vec3 center = (bmin + bmax) * 0.5f;
		// tamano del modelo en x, y, z
		glm::vec3 ext = bmax - bmin;
		// factor de escala para que el lado mayor mida tamanoFinal
		float modelScale = tamanoFinal / glm::max(ext.x, glm::max(ext.y, ext.z));

		// samplers
		GLint texLoc = glGetUniformLocation(shaderProgram, "uTex");
		GLint normalMapLoc = glGetUniformLocation(shaderProgram, "uNormalMap");
		// activa o desactiva la iluminacion y la normal
		GLint useLightingLoc = glGetUniformLocation(shaderProgram, "uUseLighting");
		GLint useNormalMapLoc = glGetUniformLocation(shaderProgram, "uUseNormalMap");
		// posicion de la luz y camara
		GLint lightPosLoc = glGetUniformLocation(shaderProgram, "uLightPos");
		GLint viewPosLoc = glGetUniformLocation(shaderProgram, "uViewPos");
		// matriz model
		GLint modelLoc = glGetUniformLocation(shaderProgram, "model");

		// asegura que el programa este activo
		shaders.use(shaderProgram);
		// color -> unidad de textura 0
		glUniform1i(texLoc, 0);
		// normales -> unidad de textura 1
		glUniform1i(normalMapLoc, 1);

		// --- Camara ---
		// tamano de la ventana
		int width = 0, height = 0;
		// lee el tamano de la ventana
		glfwGetWindowSize(window, &width, &height);
		// crea la camara en la posicion (0, 0, 2)
		Camara camara(width, height, glm::vec3(0.0f, 0.0f, 2.0f));


		// crea la curva con sus 4 puntos de control
		Bezier curva(
			// P0: punto inicial
			glm::vec3(-2.0f, 0.0f, 0.0f),
			// P1: punto de control
			glm::vec3(-1.0f, 1.0f, -3.0f),
			// P2: punto de control
			glm::vec3(1.0f, -1.0f, -3.0f),
			// P3: punto final
			glm::vec3(2.0f, 0.0f, 0.0f));

		// false = el modelo sigue la curva, true = la camara la sigue
		bool camaraSigueCurva = false;
		// estado anterior de la tecla C
		bool cPrev = false;
		// segundos que tarda en recorrer la curva
		const float duracion = 8.0f;

		// bucle principal: se repite hasta cerrar la ventana
		while (!glfwWindowShouldClose(window)) {

			// color de fondo
			glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
			// borra el color y la profundidad del cuadro anterior
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// usa los shaders
			glUseProgram(shaderProgram);

			// segundos transcurridos desde que inicio el programa
			float t = (float)glfwGetTime();


			// la tecla C esta pulsada ahora?
			bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
			// si se acaba de pulsar, alterna el modo
			if (cNow && !cPrev) camaraSigueCurva = !camaraSigueCurva;
			// recuerda el estado para el siguiente cuadro
			cPrev = cNow;


			// avance sobre la curva: va de 0 a 1 y vuelve a 0
			float s = fmodf(t / duracion, 1.0f);
			// punto de la curva en el parametro s
			glm::vec3 curvePos = curva.punto(s);
			// direccion de avance de la curva en s
			glm::vec3 curveTan = glm::normalize(curva.tangente(s));

			// modo: la camara viaja por la curva
			if (camaraSigueCurva) {
				// punto que la camara debe mirar: el origen
				const glm::vec3 objetivo(0.0f, 0.0f, 0.0f);
				// pone la camara sobre la curva
				camara.Position = curvePos;
				// apunta la camara hacia el origen
				camara.Orientation = glm::normalize(objetivo - curvePos);
			}
			// modo: la camara es libre
			else {
				// lee teclado y mouse para mover la camara
				camara.Inputs(window);
			}
			// envia projection * view al shader como camMatrix
			camara.Matrix(45.0f, 0.1f, 100.0f, shaderProgram, "camMatrix");

			// Luz y posicion de la camara, que el shader necesita
			// luz que orbita en circulo de radio 2
			glUniform3f(lightPosLoc, 2.0f * cos(t), 0.8f, 2.0f * sin(t));
			// envia la posicion de la camara (para el especular)
			glUniform3fv(viewPosLoc, 1, glm::value_ptr(camara.Position));

			// --- Modelo con iluminacion ---
			// activa la iluminacion para el modelo
			glUniform1i(useLightingLoc, 1);
			// el modelo no usa mapa de normales
			glUniform1i(useNormalMapLoc, 0);

			// matriz del modelo, empieza como identidad
			glm::mat4 model(1.0f);
			// si el modelo es quien sigue la curva
			if (!camaraSigueCurva) {
				float yaw = atan2f(curveTan.x, curveTan.z);   // asume que el modelo mira hacia +Z
				// mueve el modelo al punto de la curva
				model = glm::translate(model, curvePos);
				// lo gira alrededor del eje Y hacia donde avanza
				model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			// si la camara es quien sigue la curva
			else {
				model = glm::rotate(model, t * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			}
			// ajusta el modelo al tamano deseado
			model = glm::scale(model, glm::vec3(modelScale));
			// centra el modelo en su origen
			model = glm::translate(model, -center);   // centra el modelo
			// envia la matriz model al shader
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

			// recorre cada primitiva del modelo
			for (const auto& p : modelo) {
				glActiveTexture(GL_TEXTURE0);
				// usa la textura propia de la primitiva (ya no hay textura de respaldo)
				glBindTexture(GL_TEXTURE_2D, p.tex);
				// activa el VAO de la primitiva
				glBindVertexArray(p.vao);
				// dibuja los triangulos con los indices
				glDrawElements(GL_TRIANGLES, p.count, GL_UNSIGNED_INT, 0);
			}
			// desactiva el VAO
			glBindVertexArray(0);

			// --- Origen y curva sin iluminacion, fijos ---
			// desactiva la iluminacion
			glUniform1i(useLightingLoc, 0);
			// model = identidad, para que no se muevan
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
			// dibuja los ejes del origen
			origin.draw();

			// dibuja la curva de Bezier
			curva.draw();

			// muestra el cuadro que se acaba de dibujar
			glfwSwapBuffers(window);
			// procesa los eventos de la ventana (teclado, mouse, cerrar)
			glfwPollEvents();
		}

		// limpieza: recorre cada primitiva Y LAS BORRA
		for (auto& p : modelo) {
			glDeleteVertexArrays(1, &p.vao);
			glDeleteBuffers(1, &p.vbo);
			glDeleteBuffers(1, &p.ebo);
			if (p.tex) glDeleteTextures(1, &p.tex);
		}
		// libera los recursos de la curva
		curva.liberar();
		glDeleteProgram(shaderProgram);


		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;

	}