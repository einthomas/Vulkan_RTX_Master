#version 460

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) flat in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 color;

layout(push_constant) uniform PushConstants {
	bool shadedRendering;
} pushConstants;

void main() {
    if (pushConstants.shadedRendering) {
        color = vec4(fragColor * max(0.0f, dot(fragNormal, vec3(-1.0f, 0.0f, 0.0f))), 1.0f);
    } else {
        color = vec4(fragColor, 1.0f);
    }
}
