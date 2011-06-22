/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <concurrency/ThreadManager.h>
#include <concurrency/PosixThreadFactory.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <server/TThreadPoolServer.h>
#include <server/TThreadedServer.h>
#include <server/TNonblockingServer.h>
#include <transport/TServerSocket.h>
#include <transport/TTransportUtils.h>

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <fstream>

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

//#include "gen-cpp/Calculator.h"
#include "gen-cpp/Imaging.h"

#include <event2/event.h>
#include <event2/thread.h>

#ifdef WIN32
#define snprintf _snprintf_c
#define PATH_MAX MAX_PATH
#else
#include <unistd.h>
#endif

static event_base *base;

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace apache::thrift::concurrency;

//using namespace tutorial;
//using namespace shared;
using namespace imaging;

using namespace boost;
using namespace boost::gil;

#ifdef _WIN32
static void get_tmp_filename(char *filename, int size)
{
    char temp_dir[PATH_MAX];

    GetTempPathA(PATH_MAX, temp_dir);
    GetTempFileNameA(temp_dir, "qem", 0, filename);
}
#else
static void get_tmp_filename(char *filename, int size)
{
    int fd;
    const char *tmpdir;
    /* XXX: race condition possible */
    tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    snprintf(filename, size, "%s/vl.XXXXXX", tmpdir);
    fd = mkstemp(filename);
    close(fd);
}
#endif

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

class ImagingHandler: public ImagingIf {
public:
	ImagingHandler() {
	}


	virtual void mandelbrot(std::string& _return, const int32_t w, const int32_t h) {
		try {
			// Construct a Mandelbrot view with a locator, taking top-left corner (0,0) and step (1,1)
			point_t dims(w, h);
			my_virt_view_t mandel(dims, locator_t(point_t(0,0), point_t(1,1), mandelbrot_fn(dims)));
	
			returnDeleteView(mandel, _return);
		} catch(std::exception& e) {
			InvalidOperation io;
			io.what = 0;
			io.why = e.what();
			throw io;
		} catch(...) {
			InvalidOperation io;
			io.what = 0;
			io.why = "Unknown excpetion";
			throw io;
		}
	}

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

	virtual void transform(std::string& _return, const Transform::type t, const std::string& img) {
		try {
			rgb8_image_t src_img = image_from_string(img) ;
			rgb8_image_t dst_img;

			copy_transform(src_img, t, dst_img);

			returnDeleteView(view(dst_img), _return);
		} catch(std::exception& e) {
			InvalidOperation io;
			io.what = 0;
			io.why = e.what();
			throw io;
		} catch(...) {
			InvalidOperation io;
			io.what = 0;
			io.why = "Unknown excpetion";
			throw io;
		}
	}

protected:
	template <typename SrcView>
	void returnDeleteView(const SrcView& v, std::string& _return) {
		char temp_path[PATH_MAX];
		get_tmp_filename(temp_path, PATH_MAX);
		boost::filesystem::path jpegpath(temp_path);

		jpeg_write_view(temp_path, v);

		std::ifstream jpegIn( jpegpath.file_string().c_str(), std::ios::binary|std::ios::in);
		jpegIn.seekg (0, std::ios::end);
		size_t blob_size = jpegIn.tellg();
		jpegIn.seekg (0, std::ios::beg);

		if(blob_size) {
			_return = std::string(blob_size, 0);
			jpegIn.read(&*_return.begin(), blob_size);
		}

		boost::filesystem::remove(jpegpath);
	}

	rgb8_image_t image_from_string(const std::string& img) {
		char temp_path[PATH_MAX];
		get_tmp_filename(temp_path, PATH_MAX);
		boost::filesystem::path jpegpath(temp_path);

		std::ofstream jpegOut(temp_path, std::ios::out|std::ios::trunc|std::ios::binary);
		jpegOut << img;
		jpegOut.close();

		rgb8_image_t image;
		jpeg_read_and_convert_image(temp_path, image);

		boost::filesystem::remove(jpegpath);
		return image;
	}

};


/*
class CalculatorHandler: public CalculatorIf {
public:
	CalculatorHandler() {
	}

	void ping() {
		printf("ping()\n");
	}

	int32_t add(const int32_t n1, const int32_t n2) {
		printf("add(%d,%d)\n", n1, n2);
		return n1 + n2;
	}

	int32_t calculate(const int32_t logid, const Work &work) {
		printf("calculate(%d,{%d,%d,%d})\n", logid, work.op, work.num1,
				work.num2);
		int32_t val;

		switch (work.op) {
		case Operation::ADD:
			val = work.num1 + work.num2;
			break;
		case Operation::SUBTRACT:
			val = work.num1 - work.num2;
			break;
		case Operation::MULTIPLY:
			val = work.num1 * work.num2;
			break;
		case Operation::DIVIDE:
			if (work.num2 == 0) {
				InvalidOperation io;
				io.what = work.op;
				io.why = "Cannot divide by 0";
				throw io;
			}
			val = work.num1 / work.num2;
			break;
		default:
			InvalidOperation io;
			io.what = work.op;
			io.why = "Invalid Operation";
			throw io;
		}

		SharedStruct ss;
		ss.key = logid;
		char buffer[12];
		snprintf(buffer, sizeof(buffer), "%d", val);
		ss.value = buffer;

		log[logid] = ss;

		return val;
	}

	void getStruct(SharedStruct &ret, const int32_t logid) {
		printf("getStruct(%d)\n", logid);
		ret = log[logid];
	}

	void zip() {
		printf("zip()\n");
	}

protected:
	map<int32_t, SharedStruct> log;

};
*/

static void _log_cb(int severity, const char *msg) {
	static const char* sev[4] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
	};
#ifdef _WIN32
	int socket_errno = WSAGetLastError();
#endif
	std::cout << sev[severity] << ": " << msg << std::endl;
}

int main(int argc, char **argv) {
	event_config *conf = event_config_new();

#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.								  */
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}

	evthread_use_windows_threads();

	event_config_set_flag(conf, EVENT_BASE_FLAG_STARTUP_IOCP);
#endif

	base = event_base_new_with_config(conf);
	const char ** methods = event_get_supported_methods();

	std::cout << "Version: " << event_get_version() << std::endl;
	std::cout << "Method: " << event_base_get_method(base) << std::endl;
	std::cout << "Features: 0x" << std::hex << event_base_get_features(base) << std::endl;
	std::cout << "Base: " << base << std::endl;
	while(*methods) {
		std::cout << "Method: " << *methods++ << std::endl;
	}

	event_set_log_callback(_log_cb);

	boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
//	boost::shared_ptr<CalculatorHandler> handler(new CalculatorHandler());
//	boost::shared_ptr<TProcessor> processor(new CalculatorProcessor(handler));
	boost::shared_ptr<ImagingHandler> handler(new ImagingHandler());
	boost::shared_ptr<TProcessor> processor(new ImagingProcessor(handler));

	// using thread pool with maximum 15 threads to handle incoming requests
	boost::shared_ptr<ThreadManager> threadManager =
			ThreadManager::newSimpleThreadManager(15);
	boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<
			PosixThreadFactory> (new PosixThreadFactory());
	threadManager->threadFactory(threadFactory);
	threadManager->start();

	TNonblockingServer server(processor, protocolFactory, 9090, threadManager);

	printf("Starting the server...\n");
	server.serve();
	printf("done.\n");
	return 0;
}
