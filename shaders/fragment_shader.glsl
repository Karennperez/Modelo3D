#version 330 core

in vec4 vColor;
in vec2 vTexCoord;
in vec3 vFragPos;
in mat3 vTBN;//Matriz tangente,bitangente y normal


out vec4 FragColor;

uniform sampler2D uTex; // textura de color para la unidad cero
uniform sampler2D uNormalMap;   // mapa de normales (unidad 1)

uniform bool uUseLighting;      // falso para el origen (ejes)
uniform bool uUseNormalMap;     // tecla N para comparar
uniform vec3 uLightPos;  //posicion de la luz
uniform vec3 uViewPos; //Posicion de la camara 


void main()
{
	// El origen se dibuja sin iluminacion y conserva el color de sus vertices
    if (!uUseLighting) {
        FragColor = texture(uTex, vTexCoord) * vColor;
        return;
    }

    // Solo la textura: sin tinte de color por cara
    vec4 base = texture(uTex, vTexCoord);

    // Normal del mapa: [0,1] -> [-1,1] y de espacio tangente a espacio mundo
    vec3 normal = vTBN[2];
    if (uUseNormalMap) {
        normal = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        normal.xy *= 2.0;   // exagera el relieve (prueba 1.0, 2.0, 3.0)
        normal = vTBN * normalize(normal);
    }
    normal = normalize(normal);

    vec3 lightDir = normalize(uLightPos - vFragPos);
    vec3 viewDir  = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);

    float ambient  = 0.20;
    float diffuse  = max(dot(normal, lightDir), 0.0);
    float specular = 0.50 * pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 result = base.rgb * (diffuse + ambient) + vec3(specular) * base.r;
    FragColor = vec4(result, base.a);
}

