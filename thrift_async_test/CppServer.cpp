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

#include "ImagingHandler.h"

#include <boost/program_options.hpp>

#include <event2/event.h>
#include <event2/thread.h>

static event_base *base;

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace apache::thrift::concurrency;

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

static void usage(const boost::program_options::options_description& optionsDesc, const std::exception*e = NULL) {
	std::stringstream ss (std::stringstream::out);
	optionsDesc.print(ss);
	fprintf(stderr, "INFO Synopsis: %s\n", ss.str().c_str());
	if(e) {
		fprintf(stderr, "ERROR Exception: %s\n", e->what());
	}
}

int main(int argc, char **argv) {
	int errcode = 0;

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

	int port;
	std::string server;
	int threads;

	boost::program_options::options_description optionsDesc("Allowed options");
	optionsDesc.add_options()
		("help,h", "Help message")
		("port,p", boost::program_options::value<int>(&port)->default_value(9090), "Server port")
		("threads,t", boost::program_options::value<int>(&threads)->default_value(15), "# of threads")
		("server,s", boost::program_options::value<std::string>(&server)->default_value("nb"), "Server nb, http");

	try {
		boost::program_options::variables_map optionsMap;
		boost::program_options::store(
			boost::program_options::command_line_parser(argc, argv).
			options(optionsDesc).
			run(), 
			optionsMap);

		if( optionsMap.count("help")) {
			usage(optionsDesc);
			return 0;
		}

		boost::program_options::notify(optionsMap);
	}
	catch (const std::exception & e) {
		usage(optionsDesc, &e);
		return -1;
	}	

	if(server == "nb") {
		boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
		boost::shared_ptr<imaging::ImagingHandler> handler(new imaging::ImagingHandler());
		boost::shared_ptr<TProcessor> processor(new imaging::ImagingProcessor(handler));

		// using thread pool with maximum 'threads' threads to handle incoming requests
		boost::shared_ptr<ThreadManager> threadManager =
				ThreadManager::newSimpleThreadManager(threads);
		boost::shared_ptr<PosixThreadFactory> threadFactory = boost::shared_ptr<
				PosixThreadFactory> (new PosixThreadFactory());
		threadManager->threadFactory(threadFactory);
		threadManager->start();

		TNonblockingServer server(processor, protocolFactory, port, threadManager);

		printf("Starting the server...\n");
		server.serve();
	}

	printf("done.\n");
	return errcode;
}
