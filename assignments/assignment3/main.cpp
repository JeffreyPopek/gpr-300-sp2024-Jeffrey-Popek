#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/procGen.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);


// Global
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;

ew::Camera mainCamera;
ew::Camera lightCamera;
ew::CameraController cameraController;
ew::Transform monkeyTransform;
ew::Transform planeTransform;

struct Material {
	float Ambient = 1.0;
	float Diffuse = 0.5;
	float Specualar = 0.5;
	float Shininess = 128;
}material;

struct ChromaticAberration
{
	float r = 0.5;
	float g = 0.5;
	float b = 0.5;
	int effectOn = 0;
}chromaticAberration;

struct Light {
	glm::vec3 lightDirection = glm::vec3(0.0, -1.0, 0.0);
	glm::vec3 lightColor = glm::vec3(1);
}light;

struct PointLight {
	glm::vec3 position;
	float radius;
	glm::vec4 color;
};
const int MAX_POINT_LIGHTS = 64;
PointLight pointLights[MAX_POINT_LIGHTS];

struct Shadow {
	float minBias = 0.007;
	float maxBias = 0.2;
}shadow;

#pragma region Custom frame buffer

struct Framebuffer {
	unsigned int fbo;
	unsigned int colorTexture[8];
	unsigned int depthTexture;
	unsigned int width;
	unsigned int height;
}framebuffer;


Framebuffer createFrameBuffer(unsigned int width, unsigned int height, int color)
{
	Framebuffer frameBuffer;

	frameBuffer.width = width;
	frameBuffer.height = height;

	glCreateFramebuffers(1, &frameBuffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.fbo);

	// Color buffer
	glGenTextures(1, &frameBuffer.colorTexture[0]);
	glBindTexture(GL_TEXTURE_2D, frameBuffer.colorTexture[0]);
	glTexStorage2D(GL_TEXTURE_2D, 1, color, frameBuffer.width, frameBuffer.height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Depth buffer
	glGenTextures(1, &frameBuffer.depthTexture);
	glBindTexture(GL_TEXTURE_2D, frameBuffer.depthTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frameBuffer.colorTexture[0], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, frameBuffer.depthTexture, 0);

	return frameBuffer;
}

Framebuffer createGBuffer(unsigned int width, unsigned int height)
{
	Framebuffer buffer;
	buffer.width = width;
	buffer.height = height;

	glCreateFramebuffers(1, &buffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);

	glEnable(GL_DEPTH_TEST);

	int inputs[3]
	{
		GL_RGB32F, // World Pos
		GL_RGB16F, // World Normal
		GL_RGB16F // Albedo Color
	};

	for (size_t i = 0; i < 3; i++)
	{
		glGenTextures(1, &buffer.colorTexture[i]);
		glBindTexture(GL_TEXTURE_2D, buffer.colorTexture[i]);
		glTexStorage2D(GL_TEXTURE_2D, 1, inputs[i], width, height);

		//Clamp to border so we don't wrap when sampling for post processing
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		//Attach each texture to a different slot.
		//GL_COLOR_ATTACHMENT0 + 1 = GL_COLOR_ATTACHMENT1, etc
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, buffer.colorTexture[i], 0);
	}

	//Explicitly tell OpenGL which color attachments we will draw to
	const GLenum drawBuffers[3] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};

	glDrawBuffers(3, drawBuffers);

	//TODO: Add texture2D depth buffer
	glGenTextures(1, &buffer.depthTexture);
	glBindTexture(GL_TEXTURE_2D, buffer.depthTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, buffer.depthTexture, 0);

	//Clean up global state
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return buffer;
}

#pragma endregion


void drawUI(Framebuffer& gBuffer, unsigned int shadowMap);

int main() {
	GLFWwindow* window = initWindow("Assignment 3", screenWidth, screenHeight);

	// Shader setup
	ew::Shader shader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader postProcessShader = ew::Shader("assets/postprocess.vert", "assets/postprocess.frag");
	ew::Shader shadowShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader deferredShader = ew::Shader("assets/postprocess.vert", "assets/deferredLit.frag");
	ew::Shader geometryShader = ew::Shader("assets/lit.vert", "assets/geometryPass.frag");
	ew::Shader lightOrbShader = ew::Shader("assets/lightOrb.vert", "assets/lightOrb.frag");

	// Model setup
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");

	// Mesh setup
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));
	ew::Mesh sphereMesh = ew::Mesh(ew::createSphere(1.0f, 8));
	planeTransform.position = glm::vec3(0.0f, -1.0f, 0.0f);

	// Texture setup
	GLuint floorTexture = ew::loadTexture("assets/floor_texture.jpg");
	GLuint monkeyTexture = ew::loadTexture("assets/brick_texture.jpg");

	// Main camera setup
	mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	mainCamera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	mainCamera.aspectRatio = (float)screenWidth / screenHeight;
	mainCamera.fov = 60.0f;

	// Light camera setup
	lightCamera.target = glm::vec3(18.0f, 0.0f, 18.0f);
	lightCamera.orthoHeight = 45.0f;
	lightCamera.position = lightCamera.target - light.lightDirection * 10.0f;
	lightCamera.orthographic = true;
	lightCamera.nearPlane = 0.001f;
	lightCamera.farPlane = 45.0f;
	lightCamera.aspectRatio = 1;

	// FBO
	Framebuffer FBO = createFrameBuffer(screenWidth, screenHeight, GL_RGBA16);
	Framebuffer lightOrbs = createFrameBuffer(screenWidth, screenHeight, GL_RGBA16);
	Framebuffer GBuffer = createGBuffer(screenWidth, screenHeight);

	// Shadow map
	unsigned int shadowFBO, shadowMap;
	glCreateFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

	glGenTextures(1, &shadowMap);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 2048, 2048);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//Pixels outside of frustum should have max distance (white)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMap, 0);

	// Dummy VAO
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glEnable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	deferredShader.use();

	// Setup for each pointlight
	int index = 0;
	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			pointLights[index].position = glm::vec3((x * 5) + 1, -0.5, (y * 5) + 1);
			pointLights[index].radius = 5.0;
			pointLights[index].color = glm::vec4(rand() % 4, rand() % 4, rand() % 4, 1);
			index++;
		}
	}

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		//monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		
		cameraController.move(window, &mainCamera, deltaTime);
		lightCamera.position = lightCamera.target - light.lightDirection * 10.0f;

		glm::mat4 lightView = lightCamera.viewMatrix();
		glm::mat4 lightProj = lightCamera.projectionMatrix();
		glm::mat4 lightMatrix = lightProj * lightView;

		// First shadow
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glViewport(0, 0, 2048, 2048);
		glClear(GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_FRONT);

		shadowShader.use();
		// Put shadows on monkeys
		for (int x = 0; x < 8; x++)
		{
			for (int y = 0; y < 8; y++)
			{
				shadowShader.setMat4("_ViewProjection", lightMatrix);

				planeTransform.position = glm::vec3(x * 5, -1, y * 5);
				monkeyTransform.position = glm::vec3(x * 5, 0, y * 5);

				shadowShader.setMat4("_Model", monkeyTransform.modelMatrix());
				monkeyModel.draw();

				shadowShader.setMat4("_Model", planeTransform.modelMatrix());
				planeMesh.draw();
			}
		}

		shadowShader.setMat4("_Model", monkeyTransform.modelMatrix());
		monkeyModel.draw();
		shadowShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();

		glBindFramebuffer(GL_FRAMEBUFFER, GBuffer.fbo);
		glViewport(0, 0, GBuffer.width, GBuffer.height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_BACK);

		glBindTextureUnit(0, shadowMap);
		glBindTextureUnit(1, monkeyTexture);
		glBindTextureUnit(2, floorTexture);

		geometryShader.use();
		geometryShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());

		for (int x = 0; x < 8; x++)
		{
			for (int y = 0; y < 8; y++)
			{
				planeTransform.position = glm::vec3(x * 5, -1, y * 5);
				monkeyTransform.position = glm::vec3(x * 5, 0, y * 5);
				geometryShader.setInt("_MainTex", 1);
				geometryShader.setMat4("_Model", monkeyTransform.modelMatrix());
				monkeyModel.draw();
				geometryShader.setMat4("_Model", planeTransform.modelMatrix());
				geometryShader.setInt("_MainTex", 2);
				planeMesh.draw();
			}
		}

		// Second Framebuffer pass
		glBindFramebuffer(GL_FRAMEBUFFER, FBO.fbo);
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw scene 
		deferredShader.use();

		glBindTextureUnit(0, GBuffer.colorTexture[0]);
		glBindTextureUnit(1, GBuffer.colorTexture[1]);
		glBindTextureUnit(2, GBuffer.colorTexture[2]);
		glBindTextureUnit(3, shadowMap);


		deferredShader.setInt("_ShadowMap", 3);
		deferredShader.setMat4("_LightViewProjection", lightMatrix);
		deferredShader.setVec3("_LightDirection", light.lightDirection);
		deferredShader.setVec3("_LightColor", light.lightColor);
		deferredShader.setFloat("_MinBias", shadow.minBias);
		deferredShader.setFloat("_MaxBias", shadow.maxBias);

		deferredShader.setFloat("_Material.AmbientCo", material.Ambient);
		deferredShader.setFloat("_Material.DiffuseCo", material.Diffuse);
		deferredShader.setFloat("_Material.SpecualarCo", material.Specualar);
		deferredShader.setFloat("_Material.Shininess", material.Shininess);

		deferredShader.setVec3("_EyePos", mainCamera.position);

		for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
			//Creates prefix "_PointLights[0]." etc
			std::string prefix = "_PointLights[" + std::to_string(i) + "].";
			deferredShader.setVec3(prefix + "position", pointLights[i].position);
			deferredShader.setFloat(prefix + "radius", pointLights[i].radius);
			deferredShader.setVec4(prefix + "color", pointLights[i].color);
		}



		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, GBuffer.fbo); //Read from gBuffer 
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO.fbo); //Write to current fbo
		glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		//Draw all light orbs
		lightOrbShader.use();
		lightOrbShader.setMat4("_ViewProjection", mainCamera.projectionMatrix() * mainCamera.viewMatrix());
		for (int i = 0; i < MAX_POINT_LIGHTS; i++)
		{
			glm::mat4 m = glm::mat4(1.0f);
			m = glm::translate(m, pointLights[i].position);
			m = glm::scale(m, glm::vec3(0.2f)); 

			lightOrbShader.setMat4("_Model", m);
			lightOrbShader.setVec3("_Color", pointLights[i].color);
			sphereMesh.draw();
		}

		// Second pass
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Post processing effect
		//DON'T TURN THIS ON IT LOOKS SO DUMB
		postProcessShader.use();
		postProcessShader.setFloat("r", chromaticAberration.r);
		postProcessShader.setFloat("g", chromaticAberration.g);
		postProcessShader.setFloat("b", chromaticAberration.b);
		postProcessShader.setInt("effectOn", chromaticAberration.effectOn);

		// Fullscreen quad
		glBindTextureUnit(0, FBO.colorTexture[0]);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		drawUI(GBuffer, shadowMap);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteFramebuffers(1, &FBO.fbo);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* mainCamera, ew::CameraController* controller) {
	mainCamera->position = glm::vec3(0, 0, 5.0f);
	mainCamera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI(Framebuffer& gBuffer, unsigned int shadowMap) {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");


	// Material ImGUI (Lighting)
	if (ImGui::CollapsingHeader("Material")) 
	{
		ImGui::SliderFloat("Ambient", &material.Ambient, 0.0f, 1.0f);
		ImGui::SliderFloat("Diffuse", &material.Diffuse, 0.0f, 1.0f);
		ImGui::SliderFloat("Specular", &material.Specualar, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SliderFloat("Light Direction X", &light.lightDirection.x, -1.0f, 1.0f);
		ImGui::SliderFloat("Light Direction Y", &light.lightDirection.y, -1.0f, 1.0f);
		ImGui::SliderFloat("Light Direction Z", &light.lightDirection.z, -1.0f, 1.0f);
		ImGui::ColorEdit3("Light Color", &light.lightColor.r);

		if (ImGui::CollapsingHeader("Shadow"))
		{
			ImGui::SliderFloat("Min Bias", &shadow.minBias, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Bias", &shadow.maxBias, 0.0f, 1.0f);
		}
	}

	// Post processing
	if (ImGui::CollapsingHeader("Chromatic Aberration")) {
		ImGui::SliderFloat("R", &chromaticAberration.r, 0.0f, 1.0f);
		ImGui::SliderFloat("G", &chromaticAberration.g, 0.0f, 1.0f);
		ImGui::SliderFloat("B", &chromaticAberration.b, 0.0f, 1.0f);
		ImGui::SliderInt("Toggle Effect", &chromaticAberration.effectOn, 0, 1);
	}

	// Camera Control ImGUI
	if (ImGui::Button("Reset Camera")) 
	{
		resetCamera(&mainCamera, &cameraController);
	}
	ImGui::End();

	ImGui::Begin("Shadow Map");
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
	ImGui::Image((ImTextureID)shadowMap, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();

	ImGui::End();

	ImGui::Begin("GBuffers");
	ImVec2 texSize = ImVec2(gBuffer.width / 4, gBuffer.height / 4);
	for (size_t i = 0; i < 3; i++)
	{
		ImGui::Image((ImTextureID)gBuffer.colorTexture[i], texSize, ImVec2(0, 1), ImVec2(1, 0));
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}

