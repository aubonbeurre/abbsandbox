#include <stdio.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

static event_base *base;

static void _log_cb(int severity, const char *msg) {
	static const char* sev[4] = { "DEBUG", "INFO", "WARN", "ERROR" };
	struct timeval tv;
	event_base_gettimeofday_cached(base, &tv);
	std::cout << sev[severity] << ": " << msg << std::endl;
}

static void cb_func(evutil_socket_t fd, short what, void *arg) {
	const char *data = (const char*) arg;
	printf("Got an event on socket %d:%s%s%s%s [%s]\n", (int) fd,
			(what & EV_TIMEOUT) ? " timeout" : "",
			(what&EV_READ) ? " read" : "",
			(what&EV_WRITE) ? " write" : "",
			(what&EV_SIGNAL) ? " signal" : "",
			data);
		}

static void http_handle_blob_get(struct evhttp_request *req) {
	evkeyvalq* out_headers = evhttp_request_get_output_headers(req);
	if (!out_headers) {
		evhttp_send_reply(req, HTTP_INTERNAL, "can't get headers", 0);
		return;
	}

	int r = evhttp_add_header(out_headers, "Content-Type", "text/plain");
	if (r == -1) {
		evhttp_send_reply(req, HTTP_INTERNAL, "can't add header", 0);
		return;
	}

	evbuffer* out = evbuffer_new();
	if (!out) {
		evhttp_send_reply(req, HTTP_INTERNAL, "can't make buffer", 0);
		return;
	}

	std::string hello("hello world");

	evbuffer_add(out, &*hello.begin(), hello.length());
	evhttp_send_reply(req, HTTP_OK, "OK", out);
	evbuffer_free(out);
}

static void http_handle_blob_post(struct evhttp_request *req) {
	event_base_loopbreak(base);
	evhttp_send_reply(req, HTTP_OK, "OK", 0);
}

static void http_handle_blob(struct evhttp_request *req) {

	switch (req->type) {
	case EVHTTP_REQ_GET:
		std::cout << "get" << std::endl;
		http_handle_blob_get(req);
		break;

	case EVHTTP_REQ_POST:
		std::cout << "post" << std::endl;
		http_handle_blob_post(req);
		break;

	default:
		std::cout << "not allowed " << req->type << std::endl;
		evhttp_send_reply(req, 405, "Method not allowed", 0);
		break;
	}
}

static void http_handle_generic(struct evhttp_request *req, void *arg) {
	std::string uri(req->uri);

	if (boost::starts_with(uri, "/")) {
		http_handle_blob(req);
	} else {
		evhttp_send_reply(req, HTTP_NOTFOUND, "Not found", 0);
	}
}

int main(int argc, char*argv[]) {
	printf("Hello, world!\n");

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
	int loop;

	std::cout << "Version: " << event_get_version() << std::endl;
	std::cout << "Method: " << event_base_get_method(base) << std::endl;
	std::cout << "Features: 0x" << std::hex << event_base_get_features(base)
			<< std::endl;
	std::cout << "Base: " << base << std::endl;
	while (*methods) {
		std::cout << "Method: " << *methods++ << std::endl;
	}

	event_set_log_callback(_log_cb);

	/* The caller has already set up fd1, fd2 somehow, and make them
	 nonblocking. */

	if (0) {
		evutil_socket_t fd1 = 1;
		evutil_socket_t fd2 = 1;
		struct timeval five_seconds = { 5, 0 };
		struct event *ev1 = event_new(base, fd1,
				EV_TIMEOUT | EV_READ/*|EV_PERSIST*/, cb_func,
				(char*) "Reading event");
		struct event *ev2 = event_new(base, fd2, EV_WRITE/*|EV_PERSIST*/, cb_func,
				(char*) "Writing event");

		event_add(ev1, &five_seconds);
		event_add(ev2, NULL);

		std::cout << "\nEntering loop" << std::endl;
		loop = event_base_loop(base, 0);
		std::cout << "Exiting loop: " << loop << std::endl;
	}

	// http server
	evhttp *ev_http = evhttp_new(base);
	int http_port = 9090;

	// evhttp_bind_socket expects its PORT param in host byte order. Sigh.
	int r = evhttp_bind_socket(ev_http, "0.0.0.0", http_port);
	// This return value is undocumented (!), but this seems to work.
	if (r == -1) {
		std::cerr << "could not open port " << http_port << std::endl;
		return 3;
	}

	evhttp_set_gencb(ev_http, http_handle_generic, 0);
	//evhttp_set_cb(ev_http, "/", http_handle_root);

	std::cout << "\nEntering loop" << std::endl;
	loop = event_base_loop(base, 0);
	std::cout << "Exiting loop: " << loop << std::endl;

	evhttp_free(ev_http);

	event_base_free(base);
	event_config_free(conf);

	return 0;
}
