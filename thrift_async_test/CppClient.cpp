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

#include "../gen-cpp/Calculator.h"

#include <iostream>

#include <event2/event.h>
#include <event2/thread.h>

static event_base *base;

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

using namespace tutorial;
using namespace shared;

using namespace boost;

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

}
