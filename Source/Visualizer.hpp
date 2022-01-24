#ifndef VISUALIZER_HPP
#define VISUALIZER_HPP

#include <iostream>
#include <fstream>
#include <sstream>

//#define CL_HPP_TARGET_OPENCL_VERSION 210
//#define CL_HPP_MINIMUM_OPENCL_VERSION 200
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include <CL/cl2.hpp>
//#include <CL/cl.hpp>
#include <CL/cl_gl.h>

//OpenGL API Dependencies//
//#define GLFW_EXPOSE_NATIVE_WIN32
//#define GLFW_EXPOSE_NATIVE_WGL
#include <windows.h>
#include <glad\glad.h> 
#include <GLFW\glfw3.h>

class Visualizer
{
	float magnifier = 1.3;

	//OpenGL Inter-operability//
	cl::Context contextOpenGL_;
	uint32_t vertexBufferObject_;
	uint32_t frameBufferObject_;
	uint32_t vertexArrayObject_;

	//OpenGL Texture Variables//
	GLFWwindow* window_;
	uint32_t texture_;
	uint32_t textureBoundary_;
	uint32_t textureWidth_;
	uint32_t textureHeight_;
	unsigned char* texturePixelsBytes_;
	float* texturePixelsFloats_;
	cl::ImageGL textureOpenGL_;
	GLuint shaderProgram_ = 0;
	GLint pos_;
	GLint tex_;
	float vertices_[20] = {
	-1.0f, -1.0f, 0.0f,			1.0f, 1.0f,
	-1.0f, 1.0f, 0.0f,			0.0f, 1.0f,
	 1.0f,  -1.0f, 0.0f,		1.0f, 0.0f,
	 1.0f, 1.0f, 0.0f,			0.0f, 0.0f
	};
	std::vector<cl::Memory> memObjectsGL_;

	bool loadShaderProgram(const char* vertexShaderPath, const char* fragmentShaderPath, GLuint& shaderProgram)
	{
		//Load files source code//
		std::string vertexSource;
		std::string fragmentSource;
		std::ifstream vShaderFile;
		std::ifstream fShaderFile;

		//Open files to ifstream//
		vShaderFile.open(vertexShaderPath);
		fShaderFile.open(fragmentShaderPath);

		//Read file's buffer content into stream//
		std::stringstream vShaderStream, fShaderStream;
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		//Close files//
		vShaderFile.close();
		fShaderFile.close();

		//Convert stream to string//
		vertexSource = vShaderStream.str();
		fragmentSource = fShaderStream.str();

		//Set source code in char* for opengl c use//
		const char* vShaderCode = vertexSource.c_str();
		const char* fShaderCode = fragmentSource.c_str();

		//Compile vertex shader from source//
		GLuint vertexShader;
		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vShaderCode, NULL);
		glCompileShader(vertexShader);

		//Compile fragment shader from source//
		GLuint fragmentShader;
		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
		glCompileShader(fragmentShader);

		//Create and link shaders into shader program//
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);

		//Clean up shaders//
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		//Return status of new shader//
		int status;
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
			return false;
		return true;
	}

public:
	Visualizer(uint32_t aWidth, uint32_t aHeight) : textureWidth_(aWidth), textureHeight_(aHeight)
	{
		init();
	}
	~Visualizer()
	{
		glfwDestroyWindow(window_);
	}

	void init()
	{
		//////////
		//OpenGL//
		//////////////////////////
		//Initialize GLFW window//
		//////////////////////////
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

		//Create GLFW window//
		window_ = glfwCreateWindow(textureWidth_ * magnifier, textureHeight_ * magnifier, "OpenGL Interop", NULL, NULL);
		if (window_ == NULL)
		{
			std::cout << "Failed to create GLFW window" << std::endl;
			glfwTerminate();
		}
		glfwMakeContextCurrent(window_);

		//Initialize GLAD for loading OpenGL function pointers etc - Alternative to GLEW//
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			std::cout << "Failed to initialize GLAD" << std::endl;
		}
		std::cout << "OpenGL " << glGetString(GL_VERSION) << " Supported" << std::endl;

		glViewport(0, 0, textureWidth_ * magnifier, textureHeight_ * magnifier);

		//Creating OpenCL/GL context//
		//cl_context_properties propertiesOpenGL[5] =
		//{
		//	CL_GL_CONTEXT_KHR,
		//	(intptr_t)wglGetCurrentContext(),
		//	CL_WGL_HDC_KHR,
		//	(intptr_t)wglGetCurrentDC(),
		//	0
		//};
		//contextOpenGL_ = cl::Context(devices, propertiesOpenGL, NULL, NULL);

		//OpenGL Buffers//
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);

		//Load Shader Program//
		const char* vertexShaderPath = { "shaders/opengl/vs.glsl" };	//Vertex shader of render program
		const char* fragmentShaderPath = { "shaders/opengl/fs.glsl" }; //Fragment shader of render program

		if (!loadShaderProgram(vertexShaderPath, fragmentShaderPath, shaderProgram_))
			std::cout << "Failed to create shader." << std::endl;

		////////////////////////////
		//Create VBO + VAO objects//
		////////////////////////////

		//VBO is GPU memory containing vertices data//
		glGenBuffers(1, &vertexBufferObject_);

		//VAO interprets how VBO content is read//
		glGenVertexArrays(1, &vertexArrayObject_);
		glBindVertexArray(vertexArrayObject_);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

		//@ToDo - Need to work ou these values...//
		uint32_t numAttributesPerVertex = 5;
		uint32_t numPerVertex = 3;
		pos_ = glGetAttribLocation(shaderProgram_, "aPos");
		glVertexAttribPointer(pos_, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(0 * sizeof(GLfloat)));
		glEnableVertexAttribArray(pos_);
		tex_ = glGetAttribLocation(shaderProgram_, "aTex");
		glVertexAttribPointer(tex_, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); //@ToDo - Why is  (void*)(1 * sizeof(GLfloat)) ? Should be  (void*)(3 * sizeof(GLfloat))
		glEnableVertexAttribArray(tex_);

		//Init texture pixels//
		uint8_t numChannels = 1;																//4 channels per pixel - RGBA.
		uint32_t numPixelsWidth = textureWidth_ * numChannels;

		//Populate unsigned byte texture//
		texturePixelsBytes_ = new unsigned char[textureWidth_*textureHeight_*numChannels];				//Allocate enough memory to represent texture.
		memset(texturePixelsBytes_, 0, sizeof(unsigned char) * textureWidth_ * textureHeight_ * numChannels);	//Clear memory.

		for (uint32_t i = 0; i < textureHeight_; i++) {
			for (uint32_t j = 0; j < numPixelsWidth; ++j) {
				texturePixelsBytes_[i * numPixelsWidth + j] = 0;
				texturePixelsBytes_[i * numPixelsWidth + ++j] = 0;
				texturePixelsBytes_[i * numPixelsWidth + ++j] = 0;
				texturePixelsBytes_[i * numPixelsWidth + ++j] = 255;
			}
		}

		//Populate floating point texture//
		texturePixelsFloats_ = new float[textureWidth_*textureHeight_*numChannels];				//Allocate enough memory to represent texture.
		memset(texturePixelsFloats_, 0, sizeof(float) * textureWidth_*textureHeight_ * numChannels);	//Clear memory.

		for (uint32_t i = 0; i < textureHeight_; i++) {
			for (uint32_t j = 0; j < numPixelsWidth; ++j) {
				texturePixelsFloats_[i * numPixelsWidth + j] = 0.0;
				texturePixelsFloats_[i * numPixelsWidth + ++j] = 0.0;
				texturePixelsFloats_[i * numPixelsWidth + ++j] = 0.0;
				texturePixelsFloats_[i * numPixelsWidth + ++j] = 1.0;
			}
		}

		//OpenGL Textures//
		glGenTextures(1, &texture_);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		//cl::BufferGL();	//Create a OpenCL buffer from texture.
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, textureWidth_, textureHeight_, 0, GL_RED, GL_FLOAT, texturePixelsFloats_);	//@ToDo - Change for different image formats. GL_RGBA
		glEnable(GL_TEXTURE_2D);

		glGenTextures(1, &textureBoundary_);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, textureBoundary_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		//cl::BufferGL();	//Create a OpenCL buffer from texture.
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, textureWidth_, textureHeight_, 0, GL_RED, GL_FLOAT, texturePixelsFloats_);	//@ToDo - Change for different image formats. GL_RGBA
		glEnable(GL_TEXTURE_2D);

		//Framebuffer//
		//glGenFramebuffers(1, &frameBufferObject_);
		//glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBufferObject_);
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
		//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		//	std::cout << "Framebuffer object successfully created!" << std::endl;
		//else
		//	std::cout << "Error creating framebuffer." << std::endl;

		glUseProgram(shaderProgram_);

		//textureOpenGL_ = clCreateFromGLTexture(context_, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture_, NULL);
		//textureOpenGL_ = cl::ImageGL(context_, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture_, &errorStatus_);
		//glViewport(0, 0, aWidth, aHeight);
		glfwMakeContextCurrent(NULL);
	}

	bool render(float* aData, float* aBoundary)
	{	
		glfwMakeContextCurrent(window_);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(shaderProgram_);
		glBindVertexArray(vertexArrayObject_);
		
		//GL_UNSIGNED_BYTE
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D, texture_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, textureWidth_, textureHeight_, 0, GL_RED, GL_FLOAT, aData);
		glUniform1i(glGetUniformLocation(shaderProgram_, "aTexture"), 0);
		//glViewport(0, 0, 128 * 1, 128 * 1);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, textureBoundary_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, textureWidth_, textureHeight_, 0, GL_RED, GL_FLOAT, aBoundary);
		glUniform1i(glGetUniformLocation(shaderProgram_, "aTextureBoundary"), 1);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glfwSwapBuffers(window_);
		glfwPollEvents();
		//Poll escape key state - If presses, exit program//
		if (GLFW_PRESS == glfwGetKey(window_, GLFW_KEY_ESCAPE))
			return false;

		glfwMakeContextCurrent(NULL);

		return true;
	}

	GLFWwindow* getWindow()
	{
		return window_;
	}

	void destroyWindow()
	{
		glfwDestroyWindow(window_);
		glfwTerminate();
	}
};

#endif