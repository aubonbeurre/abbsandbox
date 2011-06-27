#pragma once

#include "gen-cpp/Imaging.h"

#include "imaging_utils.hpp"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <fstream>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>

namespace imaging {

template <typename Img>
void copy_transform(const Img& srcimg, const Transform::type t, Img& dstimg) {
	typedef typename Img::const_view_t          src_cview_t;
			
	src_cview_t src_view = const_view(srcimg);
	switch(t) {
		case Transform::UPDOWN:
			dstimg = Img(src_view.dimensions());
			copy_pixels(flipped_up_down_view(src_view), view(dstimg));
			break;
		case Transform::LEFTRIGHT:
			dstimg = Img(src_view.dimensions());
			copy_pixels(flipped_left_right_view(src_view), view(dstimg));
			break;
		case Transform::TRANSPOSE:
			dstimg = Img(src_view.dimensions().y, src_view.dimensions().x);
			copy_pixels(transposed_view(src_view), view(dstimg));
			break;
		case Transform::ROTATE90CW:
			dstimg = Img(src_view.dimensions().y, src_view.dimensions().x);
			copy_pixels(rotated90cw_view(src_view), view(dstimg));
			break;
		case Transform::ROTATE90CCW:
			dstimg = Img(src_view.dimensions().y, src_view.dimensions().x);
			copy_pixels(rotated90ccw_view(src_view), view(dstimg));
			break;
		case Transform::ROTATE180:
			dstimg = Img(src_view.dimensions());
			copy_pixels(rotated180_view(src_view), view(dstimg));
			break;
		case Transform::XGRADIENT:
			dstimg = Img(src_view.dimensions());
			x_gradient(src_view, view(dstimg));
			break;
				
	}
}

template <typename SrcView>
void view_to_string(const SrcView& v, std::string& _return) {
	boost::filesystem::path jpegpath = get_tmp_filename();

	try {
		jpeg_write_view(jpegpath.file_string().c_str(), v);
	} catch(std::exception& e) {
		fprintf(stderr, "jpeg_write_view error %d\n", errno);
		throw;
	}

	std::ifstream jpegIn( jpegpath.file_string().c_str(), std::ios::binary|std::ios::in);
	jpegIn.seekg (0, std::ios::end);
	size_t blob_size = jpegIn.tellg();
	jpegIn.seekg (0, std::ios::beg);

	if(blob_size) {
		_return = std::string(blob_size, 0);
		jpegIn.read(&*_return.begin(), blob_size);
	}
	jpegIn.close();

	boost::filesystem::remove(jpegpath);
}

rgb8_image_t image_from_string(const std::string& img);

boost::filesystem::path get_tmp_filename();

} // namespace imaging
