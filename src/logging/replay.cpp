#include "logging/logger.hpp"
#include "data/event.hpp"
#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

using namespace elob;

// Simple replay runner: reads a log file and replays events; verifies that trades produced match logged trades order.
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: replay <logfile>\n";
        return 2;
    }

    std::string path = argv[1];
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open " << path << '\n';
        return 2;
    }

    OrderBook book;
    MatchingEngine engine(book);
    EventIngestor ingestor(book, engine);

    std::string line;
    std::vector<Trade> expected_trades;
    std::vector<Trade> produced_trades;

    uint64_t next_event_id = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        char type;
        ss >> type;
        if (type == 'E') {
            // E <event_id> <timestamp> <type> ...
            uint64_t eid, ts;
            ss >> eid >> ts;
            std::string etype;
            ss >> etype;
            if (etype == "NEWORDER") {
                uint64_t oid; char sidec; int64_t price; uint64_t qty; uint64_t otrs;
                ss >> oid >> sidec >> price >> qty >> otrs;
                Order o(oid, (sidec=='B')?Side::Buy:Side::Sell, price, qty, otrs);
                NewOrder no{o};
                Event ev(eid, ts, EventPayload(no));
                auto ts_trades = ingestor.process(ev);
                produced_trades.insert(produced_trades.end(), ts_trades.begin(), ts_trades.end());
            } else if (etype == "CANCEL") {
                uint64_t oid; ss >> oid;
                Cancel c{oid};
                Event ev(eid, ts, EventPayload(c));
                ingestor.process(ev);
            } else if (etype == "MODIFY") {
                uint64_t oid; int64_t np; uint64_t nq; ss >> oid >> np >> nq;
                Modify m{oid, nq, np};
                Event ev(eid, ts, EventPayload(m));
                ingestor.process(ev);
            } else {
                // unknown event type, ignore
            }
        } else if (type == 'T') {
            uint64_t tid, ts, maker, taker, price, qty;
            ss >> tid >> ts >> maker >> taker >> price >> qty;
            expected_trades.emplace_back(tid, maker, taker, (int64_t)price, qty, ts);
        }
    }

    // Compare sequences
    bool ok = (expected_trades.size() == produced_trades.size());
    if (!ok) {
        std::cerr << "Mismatch: expected " << expected_trades.size() << " trades but produced " << produced_trades.size() << "\n";
    } else {
        for (size_t i = 0; i < expected_trades.size(); ++i) {
            const Trade &e = expected_trades[i];
            const Trade &p = produced_trades[i];
            if (e.maker_order_id != p.maker_order_id || e.taker_order_id != p.taker_order_id || e.quantity != p.quantity || e.price != p.price) {
                ok = false; break;
            }
        }
    }

    if (ok) {
        std::cout << "REPLAY OK: produced trades match logged trades (" << produced_trades.size() << ")\n";
        return 0;
    } else {
        std::cerr << "REPLAY MISMATCH" << std::endl;
        return 1;
    }
}
