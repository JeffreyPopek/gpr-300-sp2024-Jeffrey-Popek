#version 450
//Vertex attributes
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

uniform mat4 _Model; 
uniform mat4 _ViewProjection;

out Surface{
	vec3 WorldPos; //Vertex position in world space
	vec3 WorldNormal; //Vertex normal in world space
	vec2 TexCoord;
}vs_out;

out vec2 UV;

void main(){
	//Transform vertex position to World Space.
vs_out.WorldPos = vec3(_Model * vec4(vPos,1.0));
	//Transform vertex normal to world space using Normal Matrix
	vs_out.WorldNormal = transpose(inverse(mat3(_Model))) * vNormal;
vs_out.TexCoord = vTexCoord;
gl_Position = _ViewProjection * _Model * vec4(vPos,1.0);

float u = (((uint(gl_VertexID)+2u) / 3u) % 2u);
	float v = (((uint(gl_VertexID)+1u) / 3u) % 2u);
	UV = vec2(u,v);
	gl_Position = vec4(-1.0+u*2.0,-1.0+v*2.0,0.0,1.0);

}
