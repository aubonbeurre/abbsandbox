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

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>

#include "../gen-cpp/Calculator.h"

#include <event2/event.h>
#include <event2/thread.h>

#ifdef WIN32
#define snprintf _snprintf_c
#endif

static event_base *base;

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace apache::thrift::concurrency;

using namespace tutorial;
using namespace shared;

using namespace boost;
using namespace boost::gil;

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

static void test_mandelbrot() {
	point_t dims(200,200);

	// Construct a Mandelbrot view with a locator, taking top-left corner (0,0) and step (1,1)
	my_virt_view_t mandel(dims, locator_t(point_t(0,0), point_t(1,1), mandelbrot_fn(dims)));

	jpeg_write_view("J:\\mandel.jpg", mandel);
}

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
	test_mandelbrot();
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
	boost::shared_ptr<CalculatorHandler> handler(new CalculatorHandler());
	boost::shared_ptr<TProcessor> processor(new CalculatorProcessor(handler));

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
