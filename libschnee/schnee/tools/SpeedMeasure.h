#pragma once

#include <schnee/sftypes.h>
#include <schnee/tools/MicroTime.h>

namespace sf {

/// Measures the speed of a download
class SpeedMeasure {
public:
	/// Initializes speed measure
	/// A delay will measure speed each second
	SpeedMeasure(double delay = 1.0f) : mDelay (delay) { mLastCrossPoint = 0.0; mSum = 0.0; mLastValue = 0.0;}
	~SpeedMeasure () {}
	void add (float f){
		double t = sf::microtime ();
		if (mLastCrossPoint == 0.0){
			// First element
			mLastCrossPoint = t;
			mLastValue = 0;
		}
		if (t - mLastCrossPoint > mDelay){
			double diff = t - mLastCrossPoint;
			mLastValue = mSum / diff;
			mLastCrossPoint = t;
			mSum = 0;
		}
		mSum += f;
	}

	float avg () {
		return mLastValue;
	}
private:
	double mLastCrossPoint;
	double mSum;
	double mLastValue;
	double mDelay;
};

}
