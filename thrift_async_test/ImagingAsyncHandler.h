#pragma once

#include "gen-cpp/Imaging.h"

namespace imaging {

class ImagingAsyncHandler: public ImagingCobSvIf {
public:
	ImagingAsyncHandler() {
	}

	virtual void mandelbrot(std::tr1::function<void(std::string const& _return)> cob, std::tr1::function<void(::apache::thrift::TDelayedException* _throw)> exn_cob, const int32_t w, const int32_t h);

	virtual void transform(std::tr1::function<void(std::string const& _return)> cob, std::tr1::function<void(::apache::thrift::TDelayedException* _throw)> exn_cob, const Transform::type t, const std::string& img);
};

} // namespace imaging
