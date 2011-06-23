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

#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/time.h>
#endif

#include <protocol/TBinaryProtocol.h>
#include <transport/TSocket.h>
#include <transport/TTransportUtils.h>

#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

//#include "gen-cpp/Calculator.h"
#include "gen-cpp/Imaging.h"

#include <iostream>
#include <fstream>

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

static rgb8_image_t image_from_string(const std::string& img) {
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

int main(int argc, char** argv) {
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
        /* Winsock DLL.                                  */
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

	boost::shared_ptr<TTransport> socket(new TSocket("localhost", 9090));
	//shared_ptr<TTransport> transport(new TBufferedTransport(socket));
	boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
	boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

	ImagingClient client(protocol);

	try {
		transport->open();

		std::string imgstr;
		client.mandelbrot(imgstr, 200, 200);
		client.transform(imgstr, Transform::ROTATE90CCW, imgstr);

		rgb8_image_t i = image_from_string(imgstr);

#ifdef WIN32
		jpeg_write_view("J:\\mandelbrot_ccw.jpg", const_view(i));
#else
		jpeg_write_view("/tmp/mandelbrot_ccw.jpg", const_view(i));
#endif
		transport->close();
	} catch (InvalidOperation &io) {
		printf("InvalidOperation: %s\n", io.why.c_str());
	} catch (TException &tx) {
		printf("ERROR: %s\n", tx.what());
	}

#if 0
	CalculatorClient client(protocol);

	try {
		transport->open();

		client.ping();
		printf("ping()\n");

		int32_t sum = client.add(1, 1);
		printf("1+1=%d\n", sum);

		Work work;
		work.op = Operation::DIVIDE;
		work.num1 = 1;
		work.num2 = 0;

		try {
			int32_t quotient = client.calculate(1, work);
			printf("Whoa? We can divide by zero!\n");
		} catch (InvalidOperation &io) {
			printf("InvalidOperation: %s\n", io.why.c_str());
		}

		work.op = Operation::SUBTRACT;
		work.num1 = 15;
		work.num2 = 10;
		int32_t diff = client.calculate(1, work);
		printf("15-10=%d\n", diff);

		// Note that C++ uses return by reference for complex types to avoid
		// costly copy construction
		SharedStruct ss;
		client.getStruct(ss, 1);
		printf("Check log: %s\n", ss.value.c_str());

		transport->close();
	} catch (TException &tx) {
		printf("ERROR: %s\n", tx.what());
	}
#endif

}
