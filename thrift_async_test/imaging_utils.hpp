#pragma once

#include <boost/gil/gil_all.hpp>

using namespace boost::gil;

namespace imaging {

// models PixelDereferenceAdaptorConcept
struct mandelbrot_fn {
	typedef point2<ptrdiff_t>   point_t;

	typedef mandelbrot_fn	   const_t;
	typedef gray8_pixel_t	   value_type;
	typedef value_type         reference;
	typedef value_type         const_reference;
	typedef point_t            argument_type;
	typedef reference          result_type;
	BOOST_STATIC_CONSTANT(bool, is_mutable=false);

	mandelbrot_fn() {}
	mandelbrot_fn(const point_t& sz) : _img_size(sz) {}

	result_type operator()(const point_t& p) const {
		// normalize the coords to (-2..1, -1.5..1.5)
		double t=get_num_iter(point2<double>(p.x/(double)_img_size.x*3-2, p.y/(double)_img_size.y*3-1.5f));
		return value_type((bits8)(pow(t,0.2)*255));   // raise to power suitable for viewing
	}
private:
	point_t _img_size;

	double get_num_iter(const point2<double>& p) const {
		point2<double> Z(0,0);
		for (int i=0; i<100; ++i) {	 // 100 iterations
			Z = point2<double>(Z.x*Z.x - Z.y*Z.y + p.x, 2*Z.x*Z.y + p.y);
			if (Z.x*Z.x + Z.y*Z.y > 4)
				return i/(double)100;
		}
		return 0;
	}
};

typedef mandelbrot_fn::point_t point_t;
typedef virtual_2d_locator<mandelbrot_fn,false> locator_t;
typedef image_view<locator_t> my_virt_view_t;

template <typename Out>
struct halfdiff_cast_channels {
    template <typename T> Out operator()(const T& in1, const T& in2) const {
        return Out((in1-in2)/2);
    }
};

template <typename SrcView, typename DstView>
void x_gradient(const SrcView& src, const DstView& dst) {
    typedef typename channel_type<DstView>::type dst_channel_t;

    for (int y=0; y<src.height(); ++y) {
        typename SrcView::x_iterator src_it = src.row_begin(y);
        typename DstView::x_iterator dst_it = dst.row_begin(y);

        for (int x=1; x<src.width()-1; ++x)
            static_transform(src_it[x-1], src_it[x+1], dst_it[x], 
                               halfdiff_cast_channels<dst_channel_t>());
    }
}

} // namespace imaging
