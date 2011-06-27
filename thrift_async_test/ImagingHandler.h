#pragma once

#include "gen-cpp/Imaging.h"
#include <boost/gil/typedefs.hpp>

namespace imaging {

class ImagingHandler: public ImagingIf {
public:
	ImagingHandler() {
	}

	virtual void mandelbrot(std::string& _return, const int32_t w, const int32_t h);

	virtual void transform(std::string& _return, const Transform::type t, const std::string& img);

protected:
	template <typename Img>
	void copy_transform(const Img& srcimg, const Transform::type t, Img& dstimg);

	template <typename SrcView>
	static void view_to_string(const SrcView& v, std::string& _return);

	static boost::gil::rgb8_image_t image_from_string(const std::string& img);
};

} // namespace imaging
