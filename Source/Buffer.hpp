#ifndef BUFFER_H
#define BUFFER_H

#include <iostream>

//Very simple template buffer class - Does not provide c++ abstraction but remains low level for programmer to worm with//
template<typename T>
struct Buffer
{
	const unsigned long numberSamples_;
	const unsigned long bufferSize_;
	int bufferIndex_;
	T* buffer_;

	Buffer(const int aNumberSamples) :
		numberSamples_(aNumberSamples),
		bufferSize_(numberSamples_ * sizeof(T)),
		bufferIndex_(0)
	{
		buffer_ = new float[numberSamples_];
		for (unsigned long i = 0; i != numberSamples_; ++i)
			buffer_[i] = 0.0;
	}

	T& operator[] (const int index)
	{
		return buffer_[index];
	}

	T& next()
	{
		return buffer_[bufferIndex_++];
	}

	void resetIndex()
	{
		bufferIndex_ = 0;
	}

	//Possibly helpful for debugging - Uses std::cout to handle values generically//
	void print()
	{
		for (int i = 0; i != numberSamples_; ++i)
			std::cout << buffer_[i] << ", ";
		std::cout << std::endl;
	}
};

#endif