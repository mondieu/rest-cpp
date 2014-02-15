#include <rest/rest.h>

#include "resources/hello_world_service.cpp"

create_json_service(adder) {
  int a = service->request->parameter("a", 0);
  int b = service->request->parameter("b", 0);

  service->response->data["result"] = a+b;

  std::cout << "ok\n";
}

create_service(hole) {
  throw MethodNotAllowed();
}

create_service(secret) {
  service->ensure_authorization("Some secret", [](std::string username, std::string password) {
    return username == "root" && password == "spam";
  });

  service->response->raw = "You are awesome!";
}

namespace Resources {
  class BarResource : public REST::Service {
    public:
      void read() {
        response->raw = "OMG!";
      }
  };
}

void routes(REST::Router* r) {
  r->resource<Resources::BarResource>();
  r->resource<HelloWorldService>("/przywitanie");

  r->path("/", hole);
  r->path("/lol", hole);
  r->path("/sum/:a/:b", adder);
  r->path("/secret", secret);

  r->path("/fibonacci/:fib", [](REST::Service* service) {
    int num = service->request->parameter("fib", 0);
    int fib1=1, fib2=1, res=0;
    if(num ==0 || num == 1 || num == 2)
      res = 1;
    else {
      num -= 2;
      while(num--) {
        res = fib1 + fib2;
        fib1 = fib2;
        fib2 = res;
      }
    }
    service->response->use_json();
    service->response->data["result"] = res;
  });
}
