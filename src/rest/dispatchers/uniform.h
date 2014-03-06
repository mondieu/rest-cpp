#ifndef REST_CPP_DISPATCHER_UNIFORM_H
#define REST_CPP_DISPATCHER_UNIFORM_H

#include <random>
#include "../dispatcher.h"

namespace REST {

namespace Dispatchers {

/**
 * Selects next random Worker, using uniform distribution.
 */
class Uniform final : public Dispatcher {
  public:
    Uniform(int workers_count) : Dispatcher(workers_count) {};

  private:
    int next_worker_id();
};

}

}

#endif
