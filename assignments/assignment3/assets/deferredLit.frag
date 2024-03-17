#version 450
layout (location = 0) out vec4 FragColor;

in vec2 UV;

vec4 LightSpacePos;
uniform mat4 _LightViewProjection;

uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor;
uniform vec3 _AmbientColor = vec3(0.3, 0.4, 0.46);
uniform sampler2D _ShadowMap;
uniform sampler2D _MainTex;

uniform float _MinBias = 0.007;
uniform float _MaxBias = 0.2;

struct Material
{
	float AmbientCo;
	float DiffuseCo;
	float SpecualarCo;
	float Shininess;
};

struct PointLight{
	vec3 position;
	float radius;
	vec4 color;
};

#define MAX_POINT_LIGHTS 64
uniform PointLight _PointLights[MAX_POINT_LIGHTS];

uniform Material _Material;

uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;

vec3 toLight;
vec3 toEye;
vec3 h;

float diffuseFactor;
float specularFactor;


float attenuateExponential(float distance, float radius)
{
	float i = clamp(1.0 - pow(distance/radius,4.0),0.0,1.0);

	return i * i;
}

vec3 calcPointLight(PointLight light,vec3 normal,vec3 pos)
{
	vec3 diff = light.position - pos;
	//Direction toward light position
	vec3 toLight = normalize(diff);
	toEye = normalize(_EyePos - pos);
	h = normalize(toLight + toEye);
	diffuseFactor = max(dot(normal, toLight), 0.0);
	specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	//TODO: Usual blinn-phong calculations for diffuse + specular
	vec3 lightColor = (diffuseFactor + specularFactor) * light.color.rgb;
	//Attenuation
	float d = length(diff); //Distance to light
	lightColor *= attenuateExponential(d,light.radius); //See below for attenuation options
	return lightColor;
}



float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias)
{
	vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
	sampleCoord = sampleCoord * 0.5 + 0.5;

	float myDepth = sampleCoord.z - bias;

	float totalShadow = 0;
	vec2 texelOffset = 1.0 / textureSize(_ShadowMap,0);

	for(int y = -1; y <=1; y++)
	{
		for(int x = -1; x <=1; x++)
		{
			vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow+=step(texture(_ShadowMap,uv).r,myDepth);
		}
	}

	totalShadow /= 9.0;

	return totalShadow;
}


vec3 calculateLighting(vec3 normal, vec3 worldPos, vec3 albedo, vec4 LightSpacePos)
{
	toEye = normalize(_EyePos - worldPos);
	toLight = -_LightDirection;
	h = normalize(toLight + toEye);
	diffuseFactor = max(dot(normal, toLight), 0.0);
	specularFactor = pow(max(dot(normal, h), 0.0), _Material.Shininess);

	// Diffuse
	vec3 diffuseColor = _LightColor * diffuseFactor;
	vec3 lightColor = (_Material.DiffuseCo * diffuseFactor + _Material.SpecualarCo * specularFactor) * _LightColor;

	// Ambient
	lightColor += _AmbientColor * _Material.AmbientCo;

	float bias = max(_MaxBias * (1.0 - dot(normal, toLight)), _MinBias);
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias);
	vec3 light = lightColor * (1.0 - shadow);

	return light;
}

void main()
{
	//Sample surface properties for this screen pixel
	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 albedo = texture(_gAlbedo,UV).xyz;
	vec3 ligthColor = vec3(0);


	// Light Space
	LightSpacePos = _LightViewProjection * vec4(worldPos, 1);
	ligthColor += calculateLighting(normal,worldPos,albedo,LightSpacePos);

	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		ligthColor += calcPointLight(_PointLights[i], normal, worldPos);
	}

	//Worldspace lighting calculations, same as in forward shading
	FragColor = vec4(albedo * ligthColor,0.0);
}




