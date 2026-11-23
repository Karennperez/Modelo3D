#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in vec3 aTangent;

out vec4 vColor;
out vec2 vTexCoord;
out vec3 vFragPos;
out mat3 vTBN;

uniform mat4 camMatrix;
uniform mat4 model;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = camMatrix * worldPos;

    vFragPos  = vec3(worldPos);
    vColor    = aColor;
    vTexCoord = aTexCoord;

    vec3 N = normalize(mat3(model) * aNormal);
    //Gira la tanjente junto con la piramide, para que siga apuntando hacia donde crece u
    vec3 T = normalize(mat3(model) * aTangent);
    T = normalize(T - dot(T, N) * N);//Los ejes quedan perpendiculares para funcionar 
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
}