#ifndef CARTISIAN_GRID_HPP
#define CARTISIAN_GRID_HPP

template<typename T>
class Cartisian_Grid {
private:
	const unsigned int width_;
	const unsigned int height_;
	const unsigned int size_;

	T* values_;				//Think of a better name for this?
public:
	Cartisian_Grid(unsigned int width, unsigned int height) :
		width_(width),
		height_(height),
		size_(width_*height_),
		values_{ new T[size_] } {
	}

	~Cartisian_Grid() {
		values_ = nullptr;	//Why this needed to stop break?
		delete[] values_;
	}

	int indexAt(unsigned int x, unsigned int y) const {
		return (y*width_ + x);
	}

	T* pointerAt(unsigned int x, unsigned int y) const {
		return (values_ + indexAt(x, y));
	}

	T& valueAt(unsigned int x, unsigned int y) const {
		return *(values_ + indexAt(x, y));
	}

	T* getGrid()
	{
		return values_;
	}
};

#endif