//Invert effect fragment shader
#version 450
out vec4 FragColor;
in vec2 UV;

uniform float r;
uniform float g;
uniform float b;
uniform int effectOn;

uniform sampler2D _ColorBuffer;

void main(){
	vec3 color = texture(_ColorBuffer, UV).rgb;

	float r = texture(_ColorBuffer, UV - vec2(r, 0)).x;
	float g = texture(_ColorBuffer, UV - vec2(g, 0)).y;
	float b = texture(_ColorBuffer, UV - vec2(b, 0)).z;

	if(effectOn == 1)
	{
		FragColor = vec4(r, g, b, 1.0);
	}
	else
	{
	FragColor = vec4(color, 1.0);
	}
}
