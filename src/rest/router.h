#ifndef REST_CPP_ROUTER_H
#define REST_CPP_ROUTER_H

#include <set>
#include <memory>
#include <iostream>
#include <map>
#include <functional>
#include <vector>
#include "service.h"
#include "lambda_service.h"
#include "worker.h"

namespace REST {

/**
 * Router resolves URL to Request::parameters and
 * finds Service corresponding to the URL.
 *
 * @see Request
 * @see Service
 */
class Router {
  private:
    class Node {
      friend class Router;
      public:
        Node() { };
        Node(std::string p, Node* const& pr);
        ~Node();

        Node* unify(Node* const& root, std::string const& path, std::unordered_map<std::string, std::string>& params);
        Node* unify(Node* const& root, Node* const path, std::unordered_map<std::string, std::string>& params);
        bool merge(Node* const path);
        static Node* from_path(std::string const& path);

        void inject(Node* const& rhs, std::unordered_map<std::string, std::string>& params);

        void add_service(std::shared_ptr<LambdaService> srv) {
          service.clear();
          service.resize(Worker::POOL_SIZE);

          for (int i = 0; i < Worker::POOL_SIZE; i++)
            service[i] = std::make_shared<LambdaService>(srv);
        }

        template <class T>
        void add_service() {
          service.clear();
          service.resize(Worker::POOL_SIZE);
          for (int i = 0; i < Worker::POOL_SIZE; i++)
            service[i] = std::make_shared<T>();
        };
        Service::shared find_service(int worker_id);

        bool is_root();
        bool is_last();
        bool is_splat();
        Node* next();
        Node* start();
        Node* end();

        std::string uri();

        void print(int level);

      public:
        static struct Less {
          bool operator()(const Node* a, const Node* b) const {
            if (a->path[0] == '*')
              return false;
            if (b->path[0] == '*')
              return true;

            if (a->path[0] == ':')
              if (b->path[0] != ':')
                return false;
            if (b->path[0] == ':')
              if (a->path[0] != ':')
                return true;

            return a->path < b->path;
          }
        } less;

        static struct Unifiable {
          bool operator()(const Node* a, const Node* b) const {
            if (a == nullptr || b == nullptr)
              return false;

            if (!b->path.empty() && (b->path[0] == '*' || b->path[0] == ':'))
              return false;
            if (!a->path.empty() && (a->path[0] == '*' || a->path[0] == ':'))
              return true;

            return a->path == b->path;
          }
        } unifiable;

        static struct Equal {
          bool operator()(const Node* a, const Node* b) const {
            return (!less(a,b)) && (!less(b,a));
          }
        } equal;

      protected:
        std::string path;
        Node* parent = nullptr;
        std::set<Node*, Less> children;

        std::vector< Service::shared > service;
    };

  public:
    static Router* instance();
    static Service::shared find(Request::shared, int);


    void match(std::string const &, LambdaService::function);

    template <class R>
    void mount(std::string const& path, bool exact) {
      Router::Node* node = Router::Node::from_path(path);
      node->end()->add_service<R>();
      root->merge(node);

      if (exact)
        return;

      Router::Node* splat_node = Router::Node::from_path(path == "/" ? "/*" : (path+"/*"));
      splat_node->end()->service = node->end()->service;

      root->merge(splat_node);
    }

    template <class R>
    void mount(std::string const& path) {
      mount<R>(path, false);
    }

    template <class R>
    void resource(std::string const& path) {
      mount<R>(path, true);
    }

    template <class R>
    void resources(std::string const& path) {
      mount<R>(path);
    }

    template <class R, int N>
    void resources() {
      resources<R>(to_path<R, N>());
    }

    template <class R>
    void resources() {
      resources<R, 0>();
    }

    template <class R, int N>
    void resource() {
      resource<R>(to_path<R, N>());
    }

    template <class R>
    void resource() {
      resource<R, 0>();
    }

    void print();

    ~Router();

  private:
    static Node* unify(std::string const&, std::unordered_map<std::string, std::string>&);

    template <class R, int N>
    static std::string to_path() {
      std::string name = typeid(R).name();
      std::string path = "";
      path.reserve(name.size());
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      size_t number_position = 0;
      int strip = N;
      while ((number_position = name.find_first_of("0123456789")) != std::string::npos) {
        if (number_position != 0)
          name = name.substr(number_position);
        std::string number;

        for (size_t next_digit = 0; ::isdigit(name[next_digit]); next_digit++)
          number += name[next_digit];

        size_t name_length = std::stol(number);

        name = name.substr(number.size());
        if (strip <= 0)
          path += "/"+name.substr(0, name_length);
        else
          strip--;
        name = name.substr(name_length);
      }

      auto s = path.rfind("service");
      if (s == (path.size() - 7))
        path.erase(path.rfind("service"));

      s = path.rfind("resource");
      if (s == (path.size() - 8))
        path.erase(path.rfind("resource"));

      while (strip < 0) {
        strip++;

        //std::cout << path << std::endl;
        path.erase(path.rfind("/"));
      }

      return path;
    }

    Router();

    static Node* root;
};

}

#endif
