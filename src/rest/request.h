#ifndef REST_CPP_REQUEST_H
#define REST_CPP_REQUEST_H

#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <map>
#include <sstream>

namespace REST {

/**
 * Basic REST request.
 *
 * Request defines request method, parameters
 * and headers.
 */
class Request {
  public:
    enum class Method { GET, HEAD, POST, PUT, DELETE, TRACE, CONNECT, OPTIONS, UNDEFINED };

    static const size_t BUFFER_SIZE = 16384;

    Request(int client, struct sockaddr_storage client_addr);
    ~Request();

    Method method = Method::UNDEFINED;
    std::string path;
    std::multimap< std::string, std::string > headers;
    std::map< std::string, std::string > parameters;

  private:
    void parse_header(std::string line);

    char buffer[BUFFER_SIZE];
    size_t length;

    int handle;
    struct sockaddr_storage addr;
};

}

#endif
