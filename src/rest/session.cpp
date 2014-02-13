#include "session.h"

namespace REST {

  sessions_map* Session::sessions = new sessions_map;

  // 24 hours
  const int Session::LIFETIME = 86400;
  int Session::last_cleaning = time(0);

  Session::Session(std::string i) : id(i) {
    data = new std::map<std::string, std::string>();
    created_at = modified_at = time(0);
  }

  Session::~Session() {
    delete data;
  }

  sessions_map* Session::Sessions() {
    if(time(0) - last_cleaning > LIFETIME)
      killSessions();
    return sessions;
  }

  std::shared_ptr<Session> Session::getSession(std::string id) {
    auto iter = Sessions()->find(id);

    if(iter == Sessions()->end()) {
      std::shared_ptr<Session> s = std::make_shared<Session>(id);
      Sessions()->insert(std::make_pair(id, s));
      std::cout << "nowa sesja dla '"<< id <<"'\n";
      return s;
    }

    std::cout << "mam sesje dla '"<<id<<"'\n";
    iter->second->modified_at = time(0);
    return iter->second;
  }

  std::string Session::generateId() {
    srand(time(NULL) + rand());
    std::string result = "";
    for(int i=0; i<64; i++) {
      int j = rand() % 127;
      while(i < 32)
        j = rand() % 127;
      result += char(j);
    }
    return result;
  }

  void Session::killSessions() {
    for(sessions_map::iterator iter = sessions->begin(); iter != sessions->end();) {
      if(time(0) - iter->second->modified_at > LIFETIME)
        sessions->erase(iter);
      else
        iter++;
    }
  }


}
