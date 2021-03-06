#include "router.h"
#include "service.h"
#include "resource.h"
#include "lambda_service.h"
#include <string>
#include <iostream>
#include <algorithm>

namespace REST {
  Router::Node* Router::root = new Router::Node();

  Router::Router() {
  }

  Router::~Router() {
    delete root;
  }

  void Router::print() {
    std::cout << "Available routes:\n";
    root->print(1);
  }

  Router* Router::instance() {
    static Router router;
    return &router;
  }


  Service::shared Router::find(Request::shared request, int worker_id) {
    Node* node = unify(request->path, request->parameters);

    if (node == nullptr)
      return nullptr;

    Service::shared service = node->find_service(worker_id);

    return service;
  }

  void Router::match(std::string const& path, LambdaService::function lambda) {
    Node* node = Router::Node::from_path(path);
    node->end()->add_service(std::make_shared<LambdaService>(lambda));
    root->merge(node);
  }

  Router::Node::Node(std::string p, Node* const& pr) : path(p), parent(pr) {
  }

  Router::Node::~Node() {
    for (auto c : children) {
      delete c;
    }
    children.clear();
  }

  Service::shared Router::Node::find_service(int worker_id) {
    if (service.empty())
      return nullptr;
    return service[worker_id];
  }

  std::string Router::Node::uri() {
    std::string address;
    Node* previous = this;

    while (previous != nullptr) {
      address = (previous->is_root() ? "" : "/" + previous->path) + address;
      previous = previous->parent;
    }
    return address;
  }

  Router::Node* Router::Node::start() {
    Node* previous = this;

    while (true) {
      if (previous->parent == nullptr)
        return previous;
      else
        previous = previous->parent;
    }
  }

  Router::Node* Router::Node::end() {
    Node* current = this;

    while (true) {
      if (current->is_last())
        return current;
      else
        current = current->next();
    }
  }

  bool Router::Node::is_last() {
    return children.empty();
  }

  Router::Node* Router::Node::next() {
    return *(children.begin());
  }

  void Router::Node::print(int level) {
    std::cout << std::string(level * 2, ' ') << "/" << path;
    if (!service.empty()) {
      if (dynamic_cast< LambdaService* >(service[0].get()) != nullptr) {
        std::cout << " - lambda";
      } else
      if (dynamic_cast< Resource* >(service[0].get()) != nullptr) {
        std::cout << " - resource";
      } else {
        std::cout << " - service";
      }

      if (service[0]->features.size() > 0) {
        std::cout << " (";
        int s = service[0]->features.size();

        for (auto feature : service[0]->features) {
          std::cout << feature->feature_name();
          if (--s)
            std::cout << ", ";
        }
        std::cout << ")";
      }
    }
    std::cout << std::endl;
    for (auto next : children)
      next->print(level+1);
  }

  bool Router::Node::merge(Router::Node* const path) {
    if (Router::Node::equal(path, this)) {
      if (path->service.size() > 0) {
        service = path->service;
      }

      std::vector< Node* > common_paths(path->children.size());
      auto irfit = std::set_intersection(path->children.begin(), path->children.end(), children.begin(), children.end(), common_paths.begin(), Router::Node::less);
      common_paths.resize(irfit - common_paths.begin());

      for (auto next : common_paths) {
        auto common = std::find_if(children.begin(), children.end(), [&next](const Node* c) { return Router::Node::equal(next, c); });

        (*common)->merge(next);
      }

      // see whats new to add
      //   - set difference
      std::vector< Node* > new_paths(path->children.size());
      auto difit = std::set_difference(path->children.begin(), path->children.end(), children.begin(), children.end(), new_paths.begin(), Router::Node::less);
      new_paths.resize(difit - new_paths.begin());

      for (auto p : new_paths) {
        p->parent = this;
      }

      children.insert(new_paths.begin(), new_paths.end());
    }

    return true;
  }

  Router::Node* Router::unify(std::string const& path, std::unordered_map<std::string, std::string>& params) {
    return root->unify(root, path.empty() ? "/" : path, params);
  }

  bool Router::Node::is_splat() {
    return path[0] == '*';
  }

  bool Router::Node::is_root() {
    return parent == nullptr;
  }

  Router::Node* Router::Node::unify(Router::Node* const& root, std::string const& path, std::unordered_map<std::string, std::string>& params) {
    Node* node = from_path(path);
    Node* match = unify(root, node, params);

    delete node;

    // if (match != nullptr) {
    //   std::cout << "Matched '"<< path <<"' to '"<<match->uri()<<"'\n";
    // }

    return match;
  }

  Router::Node* Router::Node::unify(Router::Node* const& root, Router::Node* const path, std::unordered_map<std::string, std::string>& params) {
    if (Router::Node::unifiable(root, path)) {
      if ((root->is_last() && (path->is_last() || root->is_splat())) ||
          (path->is_last())) {
        return root;
      } else
      if (root->is_last() && !path->is_last()) {
        return nullptr;
      }
    }

    Node* match;

    for (auto next : root->children) {
      if (!Router::Node::unifiable(next, path->next()))
        continue;

      if ((match = unify(next, path->next(), params)) != nullptr) {
        if (match->service.empty())
          continue;
        next->inject(path->next(), params);
        return match;
      }
    }

    return nullptr;
  }

  void Router::Node::inject(Router::Node* const& rhs, std::unordered_map<std::string, std::string>& params) {
    if (path[0] == ':') {
      params[path.substr(1)] = Utils::uri_decode(rhs->path);
    } else
    if (path[0] == '*') {
      int i = 1;
      std::string splat;
      Node* current = rhs;
      while (true) {
        std::string d = Utils::uri_decode(current->path);
        splat += d;
        params[std::to_string(i++)] = d;
        if (!current->is_last()) {
          splat += "/";
        } else {
          break;
        }
        current = current->next();
      };
      params["0"] = splat;
    }
  }

  Router::Node* Router::Node::from_path(std::string const& p) {
    std::string path = ((!p.empty()) && (p[0] != '/')) ? "/" + p : p;
    size_t position_of_dot = path.find_last_of(".");
    if (position_of_dot != std::string::npos) {
      path[position_of_dot] = '/';
    }

    Node* root = new Node();
    Node* current = root;

    while (path.find("/") == 0) {
      path.erase(0, 1);
      if (path.empty()) {
        break;
      }

      size_t next = path.find("/");
      if (next == 0)
        continue;

      std::string name = path;

      if (next != std::string::npos) {
        name = path.substr(0, next);
        path = path.substr(next);
      }

      Node* node = new Node(name, current);
      current->children.insert(node);
      current = node;
    }

    return root;
  }
}
