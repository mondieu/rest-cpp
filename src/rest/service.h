#ifndef REST_CPP_SERVICE_H
#define REST_CPP_SERVICE_H

#include <iostream>
#include <map>
using namespace std;

namespace REST {

/**
 * Service provice RESTful stuff to other classes.
 *
 * Classes must inherit REST::Service to be open to public.
 */
class Service {

};


template <typename T>
Service * createService() {
  return new T;
}

typedef map<string, Service*(*)()> services_map;
class ServiceFactory {
  public:
    static Service * createInstance(string const&);
    static services_map* Map();
  private:
    static services_map* services;
};

template <typename C>
struct ServiceRegister : ServiceFactory {
  ServiceRegister(string const& name) {
    Map()->insert(make_pair(name, &createService<C>);
  }
}

}

#endif
