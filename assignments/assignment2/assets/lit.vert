#version 450

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

uniform mat4 _Model;
uniform mat4 _ViewProjection;
uniform mat4 _LightViewProjection;

out Surface{
	vec3 WorldPos;
	vec3 WorldNormal;
	vec2 TexCoord;
}vs_out;

out vec4 LightSpacePos;

void main()
{
	vs_out.WorldPos = vec3(_Model * vec4(vPos, 1.0));
	vs_out.WorldNormal = transpose(inverse(mat3(_Model))) * vNormal;
	vs_out.TexCoord = vTexCoord;
	LightSpacePos = _LightViewProjection * _Model * vec4(vPos, 1);
	gl_Position = _ViewProjection * _Model * vec4(vPos, 1.0);
}