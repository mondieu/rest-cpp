#include "worker.h"
#include "service.h"
#include "router.h"

#include <csignal>
#include <iostream>

namespace REST {

int Worker::POOL_SIZE = 256;

Worker::Worker(int i, int sc, size_t* cc) :
 id(i), streamers_count(sc), clients_count(cc) {
  THREAD_NAME("rest-cpp - main thread");
  *cc = 0;
  server_header = "rest-cpp, worker " + std::to_string(id);
  run();
}

void Worker::run() {
  should_run = true;

  thread = std::thread([this] () {
    THREAD_NAME(server_header.c_str());

    signal(SIGPIPE, SIG_IGN);

    // while worker is alive
    while (should_run) {
      Request::client client;
      {
        std::unique_lock<std::mutex> queue_lock(clients_queue_lock);

        // wait for new request
        clients_queue_ready.wait(queue_lock, [this] { return !should_run || !clients_queue.empty(); });
        client = clients_queue.front();
        clients_queue.pop();
      }

      if (!should_run)
        break;

      // make request
      Request::shared request = Request::make(client);

      Response::shared response(new Response(request, &streamers));
      response->headers["Server"] = server_header + ", waiting " + std::to_string(*clients_count);

      try {
        // std::cout << "Request '" << request->path << "' - worker #"<<id<<", handle #"<<request->handle<<"\n";

        make_action(request, response);

        response->send();

      } catch (HTTP::Error &e) {
        Response::unique error_response(new Response(request, e));
        error_response->headers.insert(response->headers.begin(), response->headers.end());
        error_response->send();
      }

      if (streamers.size() >= streamers_count) {
        for (auto& s : streamers)
          s.join();
        streamers.clear();
      }

      if ((*clients_count) > 0)
        (*clients_count)--;
    }

    std::cout << "Stopped worker #" << id << std::endl;
  });
}

void Worker::make_action(Request::shared request, Response::shared response) {
  std::shared_ptr<Service> service = Router::find(request, id);

  if (service == nullptr)
    throw HTTP::NotFound();

  service->request = request;
  service->response = response;

  service->make_action();
}

void Worker::stop() {
  should_run = false;
  for (auto& s : streamers)
    s.join();
  streamers.clear();
  clients_queue_ready.notify_one();
  thread.join();
}

}
