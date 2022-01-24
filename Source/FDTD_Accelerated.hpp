#ifndef FDTD_ACCELERATED_HPP
#define FDTD_ACCELERATED_HPP

#include <utility>
#include <stdint.h>
#include <iostream>
#include <fstream>

//#define CL_HPP_TARGET_OPENCL_VERSION 210
//#define CL_HPP_MINIMUM_OPENCL_VERSION 200
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include <CL/cl2.hpp>
//#include <CL/cl.hpp>
#include <CL/cl_gl.h>

//Parsing parameters as json file//
#include "json.hpp"
using nlohmann::json;

#include "FDTD_Grid.hpp"
#include "Buffer.hpp"

#include "Visualizer.hpp"

enum DeviceType { INTEGRATED = 32902, DISCRETE = 4098, NVIDIA = 4318 };
enum Implementation { OPENCL, CUDA, VULKAN, DIRECT3D };

#include <string>

class FDTD_Accelerated
{
private:
	Implementation implementation_;
	uint32_t sampleRate_;
	DeviceType deviceType_;

	//CL//
	cl_int errorStatus_ = 0;
	cl_uint num_platforms, num_devices;
	cl::Platform platform_;
	cl::Context context_;
	cl::Device device_;
	cl::CommandQueue commandQueue_;
	cl::Program kernelProgram_;
	std::string kernelSourcePath_;
	cl::Kernel kernel_;
	cl::NDRange globalws_;
	cl::NDRange localws_;

	//CL Buffers//
	cl::Buffer idGrid_;
	cl::Buffer modelGrid_;
	cl::Buffer boundaryGridBuffer_;
	cl::Buffer outputBuffer_;
	cl::Buffer excitationBuffer_;
	cl::Buffer localBuffer_;
	cl::Buffer outputPositionBuffer_;

	//Model//
	int listenerPosition_[2];
	int excitationPosition_[2];
	Model* model_;
	int modelWidth_;
	int modelHeight_;
	int gridElements_;
	int gridByteSize_;

	//Output and excitations//
	typedef float base_type_;
	unsigned int bufferSize_;
	Buffer<base_type_> output_;
	Buffer<base_type_> excitation_;

	int bufferRotationIndex_ = 1;

	Visualizer* vis;

	float* renderGrid;
	int* idGridInput_;
	int* outputGridInput_;
	float* boundaryGridInput_;
	float emptyBuffer_[44100];

	void initOpenCL()
	{
		std::vector <cl::Platform> platforms;
		cl::Platform::get(&platforms);
		for (cl::vector<cl::Platform>::iterator it = platforms.begin(); it != platforms.end(); ++it)
		{
			cl::Platform platform(*it);

			cl_context_properties contextProperties[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(platform)(), 0 };
			context_ = cl::Context(CL_DEVICE_TYPE_GPU, contextProperties);

			cl::vector<cl::Device> devices = context_.getInfo<CL_CONTEXT_DEVICES>();

			int device_id = 0;
			for (cl::vector<cl::Device>::iterator it2 = devices.begin(); it2 != devices.end(); ++it2)
			{
				cl::Device device(*it2);
				auto d = device.getInfo<CL_DEVICE_VENDOR_ID>();
				if (d == DeviceType::NVIDIA || d == DeviceType::DISCRETE)	//Hard coded to pick AMD or NVIDIA.
				{
					//Create command queue for first device - Profiling enabled//
					commandQueue_ = cl::CommandQueue(context_, device, CL_QUEUE_PROFILING_ENABLE, &errorStatus_);	//Need to specify device 1[0] of platform 3[2] for dedicated graphics - Harri Laptop.
					if (errorStatus_)
						std::cout << "ERROR creating command queue for device. Status code: " << errorStatus_ << std::endl;

					std::cout << "\t\tDevice Name Chosen: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

					return;
				}
			}
			std::cout << std::endl;
		}
	}
	void initBuffersCL()
	{
		//Create input and output buffer for grid points//
		idGrid_ = cl::Buffer(context_, CL_MEM_READ_WRITE, gridByteSize_);
		modelGrid_ = cl::Buffer(context_, CL_MEM_READ_WRITE, gridByteSize_ * 3);
		boundaryGridBuffer_ = cl::Buffer(context_, CL_MEM_READ_WRITE, gridByteSize_);
		outputBuffer_ = cl::Buffer(context_, CL_MEM_READ_WRITE, output_.bufferSize_ * sizeof(float));
		excitationBuffer_ = cl::Buffer(context_, CL_MEM_READ_WRITE, excitation_.bufferSize_ * sizeof(float));
		outputPositionBuffer_ = cl::Buffer(context_, CL_MEM_READ_WRITE, gridByteSize_);

		//Copy data to newly created device's memory//
		float* temporaryGrid =  new float[gridElements_ * 3];
		memset(temporaryGrid, 0, gridByteSize_ * 3);

		commandQueue_.enqueueWriteBuffer(idGrid_, CL_TRUE, 0, gridByteSize_ , idGridInput_);
		commandQueue_.enqueueWriteBuffer(modelGrid_, CL_TRUE, 0, gridByteSize_*3, temporaryGrid);
		commandQueue_.enqueueWriteBuffer(boundaryGridBuffer_, CL_TRUE, 0, gridByteSize_, boundaryGridInput_);
		commandQueue_.enqueueWriteBuffer(outputPositionBuffer_, CL_TRUE, 0, gridByteSize_, outputGridInput_);
	}
	void step()
	{
		commandQueue_.enqueueNDRangeKernel(kernel_, cl::NullRange/*globaloffset*/, globalws_, localws_, NULL);
		commandQueue_.enqueueNDRangeKernel(kernel_, cl::NullRange/*globaloffset*/, 2, localws_, NULL);
		//commandQueue_.finish();

		output_.bufferIndex_++;
		excitation_.bufferIndex_++;
		bufferRotationIndex_ = (bufferRotationIndex_ + 1) % 3;
	}
protected:
public:
	FDTD_Accelerated(Implementation aImplementation, uint32_t aSampleRate, float aGridSpacing) : 
		implementation_(aImplementation),
		sampleRate_(aSampleRate),
		modelWidth_(128),
		modelHeight_(128),
		//model_(64, 64, 0.5),
		bufferSize_(aSampleRate),	//@ToDo - Make these controllable.
		output_(bufferSize_),
		excitation_(bufferSize_)
	{
		listenerPosition_[0] = 16;
		listenerPosition_[1] = 16;
		excitationPosition_[0] = 32;
		excitationPosition_[1] = 32;

		memset(emptyBuffer_, 0, 44100 * sizeof(float));
	}

	~FDTD_Accelerated()
	{
		delete vis;
	}

	void buildProgram()
	{
		//Build program//
		kernelProgram_.build();

		kernel_ = cl::Kernel(kernelProgram_, "compute", &errorStatus_);	//@ToDo - Hard coded the kernel name. Find way to generate this?
	}

	void fillBuffer(float* input, float* output, uint32_t numSteps)
	{
		//Load excitation samples into GPU//
		commandQueue_.enqueueWriteBuffer(excitationBuffer_, CL_TRUE, 0, numSteps * sizeof(float), input);
		kernel_.setArg(5, sizeof(cl_mem), &excitationBuffer_);

		//Calculate buffer size of synthesizer output samples//
		for (unsigned int i = 0; i != numSteps; ++i)
		{
			input[i] = 0.0;
			//Increments kernel indices//
			kernel_.setArg(4, sizeof(int), &output_.bufferIndex_);
			kernel_.setArg(3, sizeof(int), &bufferRotationIndex_);

			step();
		}

		output_.resetIndex();
		excitation_.resetIndex();

		commandQueue_.enqueueReadBuffer(outputBuffer_, CL_TRUE, 0, numSteps * sizeof(float), output);
		commandQueue_.enqueueWriteBuffer(outputBuffer_, CL_TRUE, 0, numSteps * sizeof(float), emptyBuffer_);
	}
	void renderSimulation()
	{
		commandQueue_.enqueueReadBuffer(modelGrid_, CL_TRUE, 0, gridByteSize_, renderGrid);
		render(renderGrid, boundaryGridInput_);
	}


	void createModel(const std::string aPath, float aBoundaryValue, uint32_t aInputPosition[2], uint32_t aOutputPosition[2])
	{
		// JOSN parsing.
		//Read json file into program object//
		std::ifstream ifs(aPath);
		json jsonFile = json::parse(ifs);
		//std::cout << j << std::endl;

		 modelWidth_ = jsonFile["buffer"].size();
		 modelHeight_= jsonFile["buffer"][0].size();

		 outputGridInput_ = new int[modelWidth_*modelHeight_];
		 boundaryGridInput_ = new float[modelWidth_*modelHeight_];
		 idGridInput_ = new int[modelWidth_*modelHeight_];
		for (int i = 0; i != modelWidth_; ++i)
		{
			for (int j = 0; j != modelHeight_; ++j)
			{
				idGridInput_[i*modelWidth_ + j] = jsonFile["buffer"][i][j];
				outputGridInput_[i*modelWidth_ + j] = 0;
			}
		}

		globalws_ = cl::NDRange(modelWidth_, modelHeight_);
		localws_ = cl::NDRange(32, 32);						//@ToDo - CHANGE TO OPTIMIZED GROUP SIZE.

		//deviceType_ = INTEGRATED;
		deviceType_ = NVIDIA;

		initOpenCL();
		initRender();

		model_ = new Model(modelWidth_, modelHeight_, aBoundaryValue);
		model_->setInputPosition(aInputPosition[0], aInputPosition[1]);

		//@TODO - Temporary post-processing boundary calculation. Remove when added in SVG parser.
		int boundaryCount = 0;
		for (int i = 1; i != (modelWidth_- 1); ++i)
		{
			for (int j = 1; j != (modelHeight_ - 1); ++j)
			{
				if (idGridInput_[i*modelWidth_ + j] == 2)
				{
					//std::cout << j << ", " << i << "\n";
				}
				//@ToDo - Work out calcualting boundary grid correctly.
				//This way trying to manually treat string differently from others.
				//if (idGridInput_[i*modelWidth_ + j] == 3)
				//{
				//	int gridID = idGridInput_[i*modelWidth_ + j];

				//	boundaryCount = 0;
				//	if (idGridInput_[(i - 1)*modelWidth_ + j] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i + 1)*modelWidth_ + j] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i - 1)*modelWidth_ + j + 1] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i - 1)*modelWidth_ + j - 1] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i + 1)*modelWidth_ + j + 1] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i + 1)*modelWidth_ + j - 1] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i)*modelWidth_ + j - 1] == gridID)
				//		++boundaryCount;
				//	if (idGridInput_[(i)*modelWidth_ + j + 1] == gridID)
				//		++boundaryCount;

				//	if (boundaryCount == 1)
				//		boundaryGridInput_[i*modelWidth_ + j] = 1.0;
				//}
				//else
				//{
				//	if (idGridInput_[i*modelWidth_ + j] > 0 && (idGridInput_[(i-1)*modelWidth_ + j] == 0 || idGridInput_[(i+1)*modelWidth_ + j] == 0 || idGridInput_[i*modelWidth_ + j-1] == 0 || idGridInput_[i*modelWidth_ + j+1] == 0))
				//	//if (idGridInput_[i][j] > 0 && (idGridInput_[i-1][j] == 0 || idGridInput_[i+1][j] == 0 || idGridInput_[i][j-1] == 0 || idGridInput_[i][j+1] == 0))
				//	{
				//		boundaryGridInput_[i*modelWidth_ + j] = 1.0;
				//	}
				//}
				/*if (idGridInput_[i*modelWidth_ + j] > 0)
				{
					boundaryCount = 0;
					if (idGridInput_[(i - 1)*modelWidth_ + j] == 0)
						++boundaryCount;
					if (idGridInput_[(i + 1)*modelWidth_ + j] == 0)
						++boundaryCount;
					if (idGridInput_[(i - 1)*modelWidth_ + j+1] == 0)
						++boundaryCount;
					if (idGridInput_[(i - 1)*modelWidth_ + j-1] == 0)
						++boundaryCount;
					if (idGridInput_[(i + 1)*modelWidth_ + j+1] == 0)
						++boundaryCount;
					if (idGridInput_[(i + 1)*modelWidth_ + j-1] == 0)
						++boundaryCount;
					if (idGridInput_[(i)*modelWidth_ + j-1] == 0)
						++boundaryCount;
					if (idGridInput_[(i)*modelWidth_ + j + 1] == 0)
						++boundaryCount;
					if(boundaryCount > 1 && boundaryCount < 5)
						boundaryGridInput_[i*modelWidth_ + j] = 1.0;
				}*/

				if (idGridInput_[i*modelWidth_ + j] > 0 && (idGridInput_[(i-1)*modelWidth_ + j] == 0 || idGridInput_[(i+1)*modelWidth_ + j] == 0 || idGridInput_[i*modelWidth_ + j-1] == 0 || idGridInput_[i*modelWidth_ + j+1] == 0))
				//if (idGridInput_[i][j] > 0 && (idGridInput_[i-1][j] == 0 || idGridInput_[i+1][j] == 0 || idGridInput_[i][j-1] == 0 || idGridInput_[i][j+1] == 0))
				{
					boundaryGridInput_[i*modelWidth_ + j] = aBoundaryValue;
				}
				//std::cout << model_->boundaryGrid_.valueAt(i, j) << " | ";
			}
			//std::cout << std::endl;
		}

		gridElements_ = (modelWidth_ * modelHeight_);
		gridByteSize_ = (gridElements_ * sizeof(float));
		renderGrid = new float[gridElements_ * 3];

		if (implementation_ == Implementation::OPENCL)
		{
			initBuffersCL();
		}

		createExplicitEquation(aPath);
	}

	void createExplicitEquation(const std::string aPath)
	{
		//Read json file into program object//
		std::ifstream ifs(aPath);
		json jsonFile = json::parse(ifs);
		std::string sourceFile = jsonFile["controllers"][0]["physics_kernel"];

		//Create program source object from std::string source code//
		std::vector<std::string> programSources;
		programSources.push_back(sourceFile);
		cl::Program::Sources source(programSources);	//Apparently this takes a vector of strings as the program source.

		//Create program from source code//
		kernelProgram_ = cl::Program(context_, source, &errorStatus_);
		if (errorStatus_)
			std::cout << "ERROR creating OpenCL program from source. Status code: " << errorStatus_ << std::endl;
		
		//Build program//
		char options[1024];
		snprintf(options, sizeof(options),
			" -cl-fast-relaxed-math"
			" -cl-single-precision-constant"
			//""
		);
		kernelProgram_.build(options);	//@Highlight - Keep this in?

		kernel_ = cl::Kernel(kernelProgram_, "fdtdKernel", &errorStatus_);	//@ToDo - Hard coded the kernel name. Find way to generate this?
		if (errorStatus_)
			std::cout << "ERROR building OpenCL kernel from source. Status code: " << errorStatus_ << std::endl;

		kernel_.setArg(0, sizeof(cl_mem), &idGrid_);
		kernel_.setArg(1, sizeof(cl_mem), &modelGrid_);
		kernel_.setArg(2, sizeof(cl_mem), &boundaryGridBuffer_);
		kernel_.setArg(6, sizeof(cl_mem), &outputBuffer_);

		int inPos = model_->getInputPosition();
		int outPos = model_->getOutputPosition();
		kernel_.setArg(7, sizeof(int), &inPos);
		kernel_.setArg(8, sizeof(cl_mem), &outputPositionBuffer_);
	}
	void createMatrixEquation(const std::string aPath);	//How is the matrix equations defined? Is there just a default matrix equation that can be formed for many equations or need be defined?

	//@ToDo - Do we need this? Coefficients just need to use .setArg(), don't need to create buffer for them...
	void createCoefficient(std::string aCoeff)
	{

	}
	void updateCoefficient(std::string aCoeff, uint32_t aIndex, float aValue)
	{
		kernel_.setArg(aIndex, sizeof(float), &aValue);	//@ToDo - Need dynamicaly find index for setArg (The first param)
	}

	void setInputPosition(int aInputs[])
	{
		model_->setInputPosition(aInputs[0], aInputs[1]);
		int inPos = model_->getInputPosition();
		kernel_.setArg(7, sizeof(int), &inPos);
	}
	void setOutputPosition(int aOutputs[])
	{
		model_->setOutputPosition(aOutputs[0], aOutputs[1]);
		int flatPosition = model_->getOutputPosition();
		outputGridInput_[flatPosition] = 1;
		commandQueue_.enqueueWriteBuffer(outputPositionBuffer_, CL_TRUE, 0, gridByteSize_, outputGridInput_);
		kernel_.setArg(8, sizeof(cl_mem), &outputPositionBuffer_);
	}
	void setInputPositions(std::vector<uint32_t> aInputs);
	void setOutputPositions(std::vector<uint32_t> aOutputs);

	void setInputs(std::vector<std::vector<float>> aInputs);

	std::vector<std::vector<float>> getInputs();
	std::vector<std::vector<float>> getOutputs();

	void initRender()
	{
		vis = new Visualizer(modelWidth_, modelHeight_);
	}
	void render(float* aData, float* aBoundary)
	{
		vis->render(aData, aBoundary);
	}
	GLFWwindow* getWindow()
	{
		return vis->getWindow();
	}

	int getModelWidth()
	{
		return modelWidth_;
	}
	int getModelHeight()
	{
		return modelHeight_;
	}
};

#endif