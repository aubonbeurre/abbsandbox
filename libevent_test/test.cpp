#include <stdio.h>
#include <event.h>
#include <iostream>

static event_base *base;

static void _log_cb(int severity, const char *msg) {
  static const char* sev[4] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
  };
  struct timeval tv;
  event_base_gettimeofday_cached(base, &tv);
  std::cout << sev[severity] << ": " << msg << std::endl;
}

static void cb_func(evutil_socket_t fd, short what, void *arg)
{
  const char *data = (const char*)arg;
  printf("Got an event on socket %d:%s%s%s%s [%s]\n",
	 (int) fd,
	 (what&EV_TIMEOUT) ? " timeout" : "",
	 (what&EV_READ)    ? " read" : "",
	 (what&EV_WRITE)   ? " write" : "",
	 (what&EV_SIGNAL)  ? " signal" : "",
	 data);
}

int main(int argc, char*argv[])
{
  printf("Hello, world!\n");

  event_config *conf = event_config_new();
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

  /* The caller has already set up fd1, fd2 somehow, and make them
     nonblocking. */

  evutil_socket_t fd1 = 1;
  evutil_socket_t fd2 = 1;
  struct timeval five_seconds = {5,0};
  struct event *ev1 = event_new(base, fd1, EV_TIMEOUT|EV_READ/*|EV_PERSIST*/, cb_func, (char*)"Reading event");
  struct event *ev2 = event_new(base, fd2, EV_WRITE/*|EV_PERSIST*/, cb_func, (char*)"Writing event");

  event_add(ev1, &five_seconds);
  event_add(ev2, NULL);
  
  std::cout << "\nEntering loop" << std::endl;
  int loop = event_base_loop(base, 0);
  std::cout << "Exiting loop: " << loop << std::endl;

  event_base_free(base);
  event_config_free(conf);

  return 0;
}
