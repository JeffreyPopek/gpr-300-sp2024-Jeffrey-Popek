#version 450

out vec4 FragColor;

in Surface{
	vec3 WorldPos;
	vec3 WorldNormal;
	vec2 TexCoord;
}fs_in;

in vec4 LightSpacePos;

uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);
uniform sampler2D _ShadowMap;
uniform sampler2D _MainTex;

uniform float _MinBias = 0.02;
uniform float _MaxBias = 0.2;

// Coefficients for editor
struct Material
{
	float AmbientCo;
	float DiffuseCo;
	float SpecualarCo;
	float Shininess;
};

uniform Material _Material;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias)
{
	vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
	sampleCoord = sampleCoord * 0.5 + 0.5;

	float myDepth = sampleCoord.z - bias;

	float shadows = 0;
	vec2 texelOffset = 1.0 / textureSize(_ShadowMap,0);

	for(int y = -1; y <=1; y++)
	{
		for(int x = -1; x <=1; x++)
		{
			vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			shadows+=step(texture(_ShadowMap,uv).r,myDepth);
		}
	}

	shadows /= 9.0;

	return shadows;
}

void main()
{
	vec3 normal = normalize(fs_in.WorldNormal);
	vec3 toLight = -_LightDirection;
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);

	// Diffuse
	float diffuseFactor = max(dot(normal, toLight), 0.0);
	vec3 diffuseColor = _LightColor * diffuseFactor;

	// Specular
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	vec3 lightColor = (_Material.DiffuseCo * diffuseFactor + _Material.SpecualarCo * specularFactor) * _LightColor;

	// Ambient
	lightColor += _AmbientColor * _Material.AmbientCo;

	float bias = max(_MaxBias * (1.0 - dot(normal, toLight)), _MinBias);
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias);
	vec3 objectColor = texture(_MainTex, fs_in.TexCoord).rgb;
	vec3 light = lightColor * (1.0 - shadow);

	FragColor = vec4(objectColor * light, 1.0);
}