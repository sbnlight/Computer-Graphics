#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoords = aTexCoords;
    // 将法线从物体空间转换到世界空间（考虑非均匀缩放）
    // Convert normal from object space to world space (considering non-uniform scaling)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    // 计算片段在世界空间中的位置
    // Compute fragment position in world space
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // TODO 1: Calculate gl_Position using the model, view, and projection matrices
    // 将顶点位置从物体空间转换到裁剪空间
    // Transform vertex position from object space to clip space
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}