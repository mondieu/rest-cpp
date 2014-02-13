#ifndef REST_REST_H
#define REST_REST_H

#define STR_QUOTE(name) #name
#define STR(macro) STR_QUOTE(macro)

#ifndef SERVER_BIND
#define SERVER_BIND 0.0.0.0
#endif

#ifndef SERVER_PORT
#define SERVER_PORT 8080
#endif

#ifndef SERVER_WORKERS
#define SERVER_WORKERS 4
#endif

#include <iostream>
#include <signal.h>

#include "server.h"

void routes(REST::Router*);

namespace REST {

REST::Server *server_instance;

void main_stop_server(int a = 0) {
  delete server_instance;
}

int main(int argc, char **argv) {
  signal(SIGINT, main_stop_server);

  server_instance = new REST::Server(STR(SERVER_BIND), SERVER_PORT, SERVER_WORKERS);

  ::routes(server_instance->router());

  server_instance->run();

  return 0;
}

}

int main(int argc, char **argv) {
  return REST::main(argc, argv);
}

#endif
