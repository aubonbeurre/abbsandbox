#include "CommonHeader.h"

#include "ImagingHandler.h"

#include "ImagingThriftUtils.h"

using namespace boost;
using namespace boost::gil;
using namespace imaging;

namespace imaging {

void ImagingHandler::mandelbrot(std::string& _return, const int32_t w, const int32_t h) {
	try {
		// Construct a Mandelbrot view with a locator, taking top-left corner (0,0) and step (1,1)
		point_t dims(w, h);
		my_virt_view_t mandel(dims, locator_t(point_t(0,0), point_t(1,1), mandelbrot_fn(dims)));
	
		view_to_string(mandel, _return);
	} catch(std::exception& e) {
		InvalidOperation io;
		io.what = 0;
		io.why = e.what();
		throw io;
	} catch(...) {
		InvalidOperation io;
		io.what = 0;
		io.why = "Unknown exception";
		throw io;
	}
}

void ImagingHandler::transform(std::string& _return, const Transform::type t, const std::string& img) {
	try {
		rgb8_image_t src_img = image_from_string(img) ;
		rgb8_image_t dst_img;

		copy_transform(src_img, t, dst_img);

		view_to_string(view(dst_img), _return);
	} catch(std::exception& e) {
		InvalidOperation io;
		io.what = 0;
		io.why = e.what();
		throw io;
	} catch(...) {
		InvalidOperation io;
		io.what = 0;
		io.why = "Unknown exception";
		throw io;
	}
}

} // namespace imaging
