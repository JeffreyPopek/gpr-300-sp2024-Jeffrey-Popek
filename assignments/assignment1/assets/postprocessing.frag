#version 450
out vec4 FragColor;

in vec2 UV;

out vec4 fColor;

uniform sampler2D colorTexture;


void main(){
	vec3 color;
	float redOffset   =  0.009;
	float greenOffset =  0.006;
	float blueOffset  = -0.006;

	vec2 texSize  = textureSize(colorTexture, 0).xy;
	vec2 texCoord = gl_FragCoord.xy / texSize;


	fColor.r  = texture(colorTexture, texCoord + vec2(redOffset)).r;
	fColor.g  = texture(colorTexture, texCoord + vec2(greenOffset)).g;
	fColor.ba = texture(colorTexture, texCoord + vec2(blueOffset)).ba;

	FragColor = fColor;
}
