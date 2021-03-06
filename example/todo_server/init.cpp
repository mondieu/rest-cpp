#include <rest/rest.h>
#include "resources/list.cpp"
#include "resources/task.cpp"

void routes(REST::Router* r) {
  r->resource<List>("/");
  r->resource<List>("/:list_id");
  r->resources<Task>("/:list_id/task");
  r->match("/foo", [](REST::LambdaService* s) {
    s->response->use_json();
    s->response->data["foo"] = 0;
  });
}

