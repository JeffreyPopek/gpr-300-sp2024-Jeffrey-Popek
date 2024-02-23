#version 450

out vec2 UV;

void main(){
	float u = (((uint(gl_VertexID)+2u) / 3u) % 2u);
	float v = (((uint(gl_VertexID)+1u) / 3u) % 2u);
	UV = vec2(u,v);
	gl_Position = vec4(-1.0+u*2.0,-1.0+v*2.0,0.0,1.0);

}
