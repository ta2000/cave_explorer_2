#version 450
#extension GL_ARB_seperate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    mat4 model;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    mat4 modelview = ubo.view * ubo.model;
    gl_Position = ubo.projection * modelview * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}
