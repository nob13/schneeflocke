#pragma once

namespace sf {

/// A easy filter where you can put in multiple elements (or more)
/// And it returns the average of the last size measurements.
/// Used for delay measurement
struct SmoothingFilter {
	SmoothingFilter (int size) {
		mSize = size;
		mPos  = 0;
		mFull = 0;
		mMeasure = new float[size];
	}
	~SmoothingFilter () {
		delete [] mMeasure;
	}
	void add (float f) {
		mMeasure [mPos] = f;
		mPos  = (mPos + 1) % mSize;
		if (mFull < mSize) mFull++;
	}
	bool hasAvg () const {
		return (mFull > 0);
	}
	float avg () const {
		if (mFull == 0) {
			float a (1.0f);
			float b (0.0f);
			return ( a / b ); // pos inf (doesn't compile in MSVC when inlined)
		}
		double sum = 0.0;
		for (int i = 0; i < mFull; i++) {
			sum+= mMeasure[i];
		}
		return (float) (sum / (double)(mFull));
	}
private:
	SmoothingFilter (const SmoothingFilter&) {}
	SmoothingFilter & operator= (const SmoothingFilter & ) { return *this; }
	int mPos;
	int mFull;
	int mSize;
	float * mMeasure;
};

}
