#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in uvec4 aJoints;
layout(location=3) in vec4 aWeights;
layout(location=4) in vec3 aNormal;

uniform mat4 uModel, uView, uProjection;
uniform mat4 uBones[128];

out vec2 vUV;
out vec3 vNormalView;

void main() {
    mat4 skinMat =
        aWeights.x * uBones[aJoints.x] +
        aWeights.y * uBones[aJoints.y] +
        aWeights.z * uBones[aJoints.z] +
        aWeights.w * uBones[aJoints.w];

    vec4 skinnedPos = skinMat * vec4(aPos, 1.0);
    gl_Position = uProjection * uView * uModel * skinnedPos;

    vUV = aUV;

    // Transform normal to view space
    mat3 normalMat = mat3(uView * uModel * skinMat);
    vNormalView = normalize(normalMat * aNormal);
}
