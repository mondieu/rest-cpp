#include "authorization.h"

namespace REST {
namespace Features {

void Authorization::feature_push() {
  auto auth_header = request->headers.find("Authorization");
  if (auth_header != request->headers.end()) {
    if (auth_header->second.find("Basic") == 0) {
      std::string decoded = Utils::base64_decode(auth_header->second.substr(auth_header->second.find(" ")+1));
      size_t colon = decoded.find(":");
      authorization = std::make_pair(decoded.substr(0, colon), decoded.substr(colon+1));
    }
  }
}

void Authorization::ensure_authorization(std::string const& realm, std::function<bool(std::string, std::string)> handler) {
  if (authorization.first.empty() || !handler(authorization.first, authorization.second)) {
    response->headers["WWW-Authenticate"] = "Basic realm=\"" + realm + "\"";
    throw HTTP::NotAuthorized();
  }
}

void Authorization::feature_pop() {
  authorization.first.clear();
  authorization.second.clear();
}

}
}
