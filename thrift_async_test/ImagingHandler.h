#pragma once

#include "gen-cpp/Imaging.h"

namespace imaging {

class ImagingHandler: public ImagingIf {
public:
	ImagingHandler() {
	}

	virtual void mandelbrot(std::string& _return, const int32_t w, const int32_t h);

	virtual void transform(std::string& _return, const Transform::type t, const std::string& img);
};

} // namespace imaging
