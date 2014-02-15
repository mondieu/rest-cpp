#include "router.h"
#include "service.h"
#include "lambda_service.h"
#include <string>
#include <iostream>
#include <algorithm>

namespace REST {
  workers_services_vector Router::workers_services;
  Router* Router::pInstance = NULL;
  lambda_patterns* Router::patterns = new lambda_patterns();
  int Router::WORKERS = 256;
  std::shared_ptr<Router::Node> Router::root = std::make_shared<Router::Node>();

  Router::Router() {
    workers_services.resize(WORKERS);

    for (int i = 0; i < WORKERS; i++)
      workers_services[i] = std::make_shared<names_services_map>();
  }

  Router* Router::Instance() {
    if(pInstance == NULL){
      pInstance = new Router();
    }
    return pInstance;
  }


  std::shared_ptr<Service> Router::getResource(std::shared_ptr<Request> request, int worker_id) {
    std::shared_ptr<Node> node = match(request->path, request->parameters);

    if (node == nullptr)
      return NULL;

    return node->find_service(worker_id);
  }

  void Router::path(std::string const& path, service_lambda lambda) {
    std::shared_ptr<Router::Node> node = Router::Node::from_path(path);
    node->end()->add_service(std::make_shared<LambdaService>(lambda));
    root->merge(node);
  }

  Router::Node::Node(std::string p, std::shared_ptr<Node> const& pr) : path(p), parent(pr) {
  }

  Router::Node::~Node() {
    children.clear();
  }

  std::shared_ptr<Service> Router::Node::find_service(int worker_id) {
    return service[worker_id];
  }

  std::string Router::Node::uri() {
    std::string address;
    std::shared_ptr<Node> previous = shared_from_this();

    while (previous != nullptr) {
      address = (previous->is_root() ? "" : "/" + previous->path) + address;
      previous = previous->parent;
    }
    return address;
  }

  std::shared_ptr<Router::Node> Router::Node::start() {
    std::shared_ptr<Node> previous = shared_from_this();

    while (true) {
      if (previous->parent == nullptr)
        return previous;
      else
        previous = previous->parent;
    }
  }

  std::shared_ptr<Router::Node> Router::Node::end() {
    std::shared_ptr<Node> current = shared_from_this();

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

  std::shared_ptr<Router::Node> Router::Node::next() {
    return *(children.begin());
  }

  void Router::Node::print(int level) {
    std::cout << std::string(level*2, ' ') << "/" << path << std::endl;
    for (auto next : children)
      next->print(level+1);
  }

  bool Router::Node::merge(std::shared_ptr<Router::Node> const path) {
    if (Router::Node::equal(path, shared_from_this())) {
      std::vector< std::shared_ptr<Node> > common_paths(path->children.size());
      auto irfit = std::set_intersection(path->children.begin(), path->children.end(), children.begin(), children.end(), common_paths.begin(), Router::Node::less);
      common_paths.resize(irfit - common_paths.begin());

      for (auto next : common_paths) {
        auto common = std::find_if(children.begin(), children.end(), [&next](const std::shared_ptr<Node> c) { return Router::Node::equal(next, c); });

        (*common)->merge(next);
      }

      // see whats new to add
      //   - set difference
      std::vector< std::shared_ptr<Node> > new_paths(path->children.size());
      auto difit = std::set_difference(path->children.begin(), path->children.end(), children.begin(), children.end(), new_paths.begin(), Router::Node::less);
      new_paths.resize(difit - new_paths.begin());

      for (auto p : new_paths) {
        p->parent = shared_from_this();
      }

      children.insert(new_paths.begin(), new_paths.end());
    }

    return true;
  }

  std::shared_ptr<Router::Node> Router::match(std::string const& path, params_map& params) {
    return root->unify(root, path.empty() ? "/" : path, params);
  }

  bool Router::Node::is_splat() {
    return path[0] == '*';
  }

  bool Router::Node::is_root() {
    return parent == nullptr;
  }

  std::shared_ptr<Router::Node> Router::Node::unify(std::shared_ptr<Router::Node> const& root, std::string const& path, params_map& params) {
    std::shared_ptr<Node> match = unify(root, from_path(path), params);

    if (match != nullptr) {
      std::cout << "matched '"<< path <<"' to '"<<match->uri()<<"'\n";
    }

    std::cout << "services " << match->service.size() << std::endl;

    return match;
  }

  std::shared_ptr<Router::Node> Router::Node::unify(std::shared_ptr<Router::Node> const& root, std::shared_ptr<Router::Node> const path, params_map& params) {
    if (Router::Node::unifiable(root, path)) {
      if ((root->is_last() && (path->is_last() || root->is_splat())) ||
          (path->is_last())) {
        return root;
      } else
      if (root->is_last() && !path->is_last()) {
        return nullptr;
      }
    }

    std::shared_ptr<Node> match;

    for (auto next : root->children) {
      if (!Router::Node::unifiable(next, path->next()))
        continue;

      if ((match = unify(next, path->next(), params)) != nullptr) {
        next->inject(path->next(), params);
        return match;
      }
    }

    return nullptr;
  }

  void Router::Node::inject(std::shared_ptr<Router::Node> const& rhs, params_map& params) {
    if (path[0] == ':') {
      params[path.substr(1)] = rhs->path;
    } else
    if (path[0] == '*') {
      int i = 1;
      std::string splat;
      std::shared_ptr<Node> current = rhs;
      while (true) {
        splat += current->path;
        params[std::to_string(i++)] = current->path;
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

  std::shared_ptr<Router::Node> Router::Node::from_path(std::string const& p) {
    std::string path = p;

    std::shared_ptr<Node> root = std::make_shared<Node>();
    std::shared_ptr<Node> current = root;

    while (path.find("/") == 0) {
      path.erase(0, 1);
      if (path.empty()) {
        std::cout << "pusty" << std::endl;
        break;
      }

      size_t next = path.find("/");

      std::string name = path;

      if (next != std::string::npos) {
        name = path.substr(0, next);
        path = path.substr(next);
      }

      std::shared_ptr<Node> node = std::make_shared<Node>(name, current);
      current->children.insert(node);
      current = node;
    }

    return root;
  }
}
