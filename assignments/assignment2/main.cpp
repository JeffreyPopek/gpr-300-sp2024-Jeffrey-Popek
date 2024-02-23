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
void drawUI(unsigned int shadowMap);

ew::Camera camera;
ew::CameraController cameraController;
ew::Camera lightCamera;
ew::Transform monkeyTransform;
ew::Transform planeTransform;

//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;

struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
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

struct Shadow {
	float minBias = 0.01;
	float maxBias = 0.04;
}shadow;

int main() {
	GLFWwindow* window = initWindow("Assignment 2", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Shader and Model Setup
	ew::Shader shader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader postProcessShader = ew::Shader("assets/postprocess.vert", "assets/postprocess.frag");
	ew::Shader shadowShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	//GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");
	
	// Plane setup
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));
	planeTransform.position = glm::vec3(0.0f, -1.0f, 0.0f);


	// Normal camera Setup
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;

	// Light camera setup
	lightCamera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	lightCamera.orthoHeight = 5.0f;
	lightCamera.position = lightCamera.target - light.lightDirection * lightCamera.orthoHeight;
	lightCamera.orthographic = true;
	lightCamera.nearPlane = 0.01f;
	lightCamera.farPlane = 20.0f;
	lightCamera.aspectRatio = 1;

	// FBO
	unsigned int fbo;
	glCreateFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Color Buffer
	unsigned int frameBufferTexture;
	glGenTextures(1, &frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16, screenWidth, screenHeight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Attach color buffer to framebuffer
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frameBufferTexture, 0);

	// Depth Buffer
	unsigned int depthBuffer;
	glGenTextures(1, &depthBuffer);
	glBindTexture(GL_TEXTURE_2D, depthBuffer);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);

	// Assign depth buffer
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBuffer, 0);

	// Shadow map stuff
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

	// Draw Buffer Setup in case of multiple render textures
	static const GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	/*
	1. Shadow buffer pass
	2. First framebuffer pass
	3. Draw scene (with shadow)
	4. Second framebuffer pass
	5. Apply post processing effect
	6. Draw scene onto fullscreen quad
	7. Draw UI
	*/

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		//monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		cameraController.move(window, &camera, deltaTime);

		lightCamera.position = lightCamera.target - light.lightDirection * 5.0f;

		glm::mat4 lightView = lightCamera.viewMatrix();
		glm::mat4 lightProj = lightCamera.projectionMatrix();
		glm::mat4 lightMatrix = lightProj * lightView;

		// First shadow buffer pass
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glViewport(0, 0, 2048, 2048);
		glClear(GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_FRONT);

		// Shadow shader
		shadowShader.use();
		shadowShader.setMat4("_ViewProjection", lightMatrix);
		shadowShader.setMat4("_Model", monkeyTransform.modelMatrix());

		// Draw model and plane in scene
		monkeyModel.draw();
		shadowShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();

		// First framebuffer pass
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);

		// Draw Scene
		glBindTextureUnit(0, shadowMap);
		shader.use();

		shader.setVec3("_EyePos", camera.position);
		shader.setMat4("_Model", monkeyTransform.modelMatrix());
		shader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

		shader.setFloat("_Material.Ka", material.Ka);
		shader.setFloat("_Material.kD", material.Kd);
		shader.setFloat("_Material.Ks", material.Ks);
		shader.setFloat("_Material.Shininess", material.Shininess);

		// Shadows
		shader.setInt("_ShadowMap", 0);
		shader.setMat4("_LightViewProjection", lightMatrix);
		shader.setVec3("_LightDirection", light.lightDirection);
		shader.setVec3("_LightColor", light.lightColor);
		shader.setFloat("_MinBias", shadow.minBias);
		shader.setFloat("_MaxBias", shadow.maxBias);

		monkeyModel.draw();

		shader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();

		// Second frambuffer pass to bind
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Post processing effect
		//DON'T TURN THIS ON IT LOOKS SO DUMB
		postProcessShader.use();
		postProcessShader.setFloat("r", chromaticAberration.r);
		postProcessShader.setFloat("g", chromaticAberration.g);
		postProcessShader.setFloat("b", chromaticAberration.b);
		postProcessShader.setInt("effectOn", chromaticAberration.effectOn);

		// Fullscreen Quad
		glBindTextureUnit(0, frameBufferTexture);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		drawUI(shadowMap);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteFramebuffers(1, &fbo);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI(unsigned int shadowMap) {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");


	// Material ImGUI (Lighting)
	if (ImGui::CollapsingHeader("Material")) 
	{
ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SliderFloat("Light Direction X", &light.lightDirection.x, -5.0f, 5.0f);
		ImGui::SliderFloat("Light Direction Y", &light.lightDirection.y, -5.0f, 5.0f);
		ImGui::SliderFloat("Light Direction Z", &light.lightDirection.z, -5.0f, 5.0f);
		// Wasn't working idk why, fix later
		//ImGui::ColorEdit3("Light Color", &light.lightColor.r);

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
		resetCamera(&camera, &cameraController);
	}
	ImGui::End();

	ImGui::Begin("Shadow Map");
	//Using a Child allow to fill all the space of the window.
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
	ImGui::Image((ImTextureID)shadowMap, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();
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

