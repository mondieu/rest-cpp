#include "response.h"
#include <thread>
#include <future>
#include <csignal>

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
# ifdef SO_NOSIGPIPE
#  define CEPH_USE_SO_NOSIGPIPE
# else
#  error "Cannot block SIGPIPE!"
# endif
#endif

namespace REST {

Response::Response(Request::shared request, std::vector<std::thread>* s) {
  streamers = s;
  handle = request->handle;
  start_time = request->time;
  headers["Content-Type"] = "text/plain; charset=utf-8";
  headers["Connection"] = "close";
}

Response::Response(Request::shared request, HTTP::Error &error) : Response(request, nullptr) {
  status = error.code();
  status_message = error.what();
  use_json();
  data["status"] = "error";
  data["error"]["code"] = status;
  data["error"]["message"] = status_message;
}

void Response::use_json() {
  headers["Content-Type"] = "application/json; charset=utf-8";
  is_json = true;
}

void Response::stream_async(std::function<void(int)> streamer) {
  stream(streamer, true);
}

void Response::stream(std::function<void(int)> streamer, bool async) {
  is_streamed = true;

  std::string content = "HTTP/1.1 " + std::to_string(status) + " " + status_message + "\r\n";
  headers.insert(std::make_pair("Date", Utils::rfc1123_datetime(time(0))));

  // add headers
  for (auto header : headers)
    content += header.first + ": " + header.second + "\r\n";

  content += "\r\n";

  // send every byte
  ::send(handle, content.c_str(), content.size(), MSG_DONTWAIT | MSG_NOSIGNAL);

  if (async) {
    int h = handle;
    streamers->emplace_back([streamer, h]() {
      signal(SIGPIPE, SIG_IGN);
      try {
        // std::this_thread::yield();
        streamer(h);
        close(h);
      } catch (...) {
      }
    });
  } else {
    std::this_thread::yield();
    streamer(handle);
    close(handle);
  }
}

size_t Response::send() {
  if (is_streamed)
    return 0;

  size_t bytes_sent = 0;

  std::string payload;

  if (is_json) {
    Json::FastWriter json_writer;
    payload = json_writer.write(data);
  } else {
    payload = raw;
  }

  // start http
  std::string content = "HTTP/1.1 " + std::to_string(status) + " " + status_message + "\r\n";

  // content size
  headers["Content-Length"] = std::to_string(payload.size());
  headers["Server"] += ", took " + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count() / 1000.0f) + "ms";
  headers.insert(std::make_pair("Date", Utils::rfc1123_datetime(time(0))));

  // add headers
  for (auto header : headers)
    content += header.first + ": " + header.second + "\r\n";

  content += "\r\n";

  content += payload;

  // send every byte 
  bytes_sent = ::send(handle, content.c_str(), content.size(), MSG_DONTWAIT | MSG_NOSIGNAL);

  // close connection with client
  shutdown(handle, SHUT_WR);
  for(;;) {
    char buffer[4000];
    int res=read(handle, buffer, 4000);
    if(res < 0) {
        perror("reading");
        exit(1);
    }
    if(!res)
        break;
  }

  close(handle);
  
  return bytes_sent;
}

Response::~Response() {
}

}
