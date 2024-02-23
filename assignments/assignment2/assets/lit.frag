#version 450

out vec4 FragColor;

in Surface{
	vec3 WorldPos;
	vec3 WorldNormal;
	vec2 TexCoord;
}fs_in;

in vec4 LightSpacePos;
//uniform sampler2D _MainTex; 
uniform vec3 _EyePos;
uniform vec3 _LightDirection = vec3(0.0,-1.0,0.0);
uniform vec3 _LightColor = vec3(1.0);
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);
uniform sampler2D _ShadowMap;

uniform float _MinBias = 0.005;
uniform float _MaxBias = 0.010;

struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias)
{
	//Homogeneous Clip space to NDC [-w,w] to [-1,1]
	vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
	//Convert from [-1,1] to [0,1]
	sampleCoord = sampleCoord * 0.5 + 0.5;

	float myDepth = sampleCoord.z - bias;
	float shadowMapDepth = texture(shadowMap, sampleCoord.xy).r;
	//vec2 texelOffset = 1.0 / textureSize(_ShadowMap,0);
	float shadows = 0;

	int x, y;
	for(y = -1; y <= 1; y++)
	{
		for(x = -1; x <= 1; x++)
		{
			//vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			shadows+=step(shadowMapDepth, myDepth);
		}
	}

	shadows /= 10.0;

	return shadows;
}

void main()
{
	vec3 normal = normalize(fs_in.WorldNormal);

	//Light pointing straight down
	vec3 toLight = -_LightDirection;
	vec3 toEye = normalize(_EyePos - fs_in.WorldPos);

	// Diffuse
	float diffuseFactor = max(dot(normal, toLight), 0.0);
	vec3 diffuseColor = _LightColor * diffuseFactor;

	// Specular
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	// Ambient
	vec3 ambient = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * _LightColor;
	ambient += _AmbientColor * _Material.Ka;


	// Shadows
	float bias = max(_MaxBias * (1.0 - dot(normal, toLight)), _MinBias);
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias);
	vec3 light = ambient * (1.0 - shadow);

	FragColor = vec4(light, 1.0);
}