#include "elob/metrics.hpp"
#include <sstream>

namespace elob {

std::string Metrics::summary() const {
    std::ostringstream os;
    os << "orders=" << orders_ingested << " trades=" << trades_executed
       << " cancels=" << cancels << " modifies=" << modifies;
    return os.str();
}

} // namespace elob
