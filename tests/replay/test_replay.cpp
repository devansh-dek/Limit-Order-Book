#include "logging/logger.hpp"
#include "engine/order.hpp"
#include "engine/order_book.hpp"
#include "engine/matching_engine.hpp"
#include "data/ingestor.hpp"

#include <sstream>
#include <string>
#include <cassert>
#include <fstream>
#include <iostream>

using namespace elob;

int main() {
    const std::string logfile = "replay_test.log";

    // Setup a book and engine and logger
    OrderBook book;
    MatchingEngine engine(book);
    EventIngestor ingestor(book, engine);
    Logger logger(logfile);

    // Create a resting sell at 100 qty 5
    Order s1(1, Side::Sell, 100, 5, 1);
    NewOrder no1{s1};
    Event ev1(1, 1, EventPayload(no1));
    logger.log_event(ev1);
    auto trades1 = ingestor.process(ev1);
    for (auto &t : trades1) logger.log_trade(t);

    // Create buy taker that crosses
    Order b2(2, Side::Buy, 100, 3, 2);
    NewOrder no2{b2};
    Event ev2(2, 2, EventPayload(no2));
    logger.log_event(ev2);
    auto trades2 = ingestor.process(ev2);
    for (auto &t : trades2) logger.log_trade(t);

    // Now replay using the replay runner internals: read file and ensure produced trades match logged trades
    // We'll invoke the replay.cpp main-like logic inline for convenience

    // Read file
    std::ifstream ifs(logfile);
    assert(ifs.is_open());

    OrderBook rbook;
    MatchingEngine rengine(rbook);
    EventIngestor ringestor(rbook, rengine);

    std::string line;
    std::vector<Trade> expected;
    std::vector<Trade> produced;

    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        char c; ss >> c;
        if (c == 'E') {
            uint64_t eid, ts; ss >> eid >> ts;
            std::string etype; ss >> etype;
            if (etype == "NEWORDER") {
                uint64_t oid; char sidec; int64_t price; uint64_t qty; uint64_t otrs;
                ss >> oid >> sidec >> price >> qty >> otrs;
                Order o(oid, (sidec=='B')?Side::Buy:Side::Sell, price, qty, otrs);
                NewOrder no{o};
                Event ev(eid, ts, EventPayload(no));
                auto tr = ringestor.process(ev);
                produced.insert(produced.end(), tr.begin(), tr.end());
            } else if (etype == "CANCEL") {
                uint64_t oid; ss >> oid; Cancel c{oid}; Event ev(eid, ts, EventPayload(c)); ringestor.process(ev);
            } else if (etype == "MODIFY") {
                uint64_t oid; int64_t np; uint64_t nq; ss >> oid >> np >> nq; Modify m{oid,nq,np}; Event ev(eid,ts,EventPayload(m)); ringestor.process(ev);
            }
        } else if (c == 'T') {
            uint64_t tid, ts, maker, taker, price, qty;
            ss >> tid >> ts >> maker >> taker >> price >> qty;
            expected.emplace_back(tid, maker, taker, (int64_t)price, qty, ts);
        }
    }

    assert(expected.size() == produced.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert(expected[i].maker_order_id == produced[i].maker_order_id);
        assert(expected[i].taker_order_id == produced[i].taker_order_id);
        assert(expected[i].quantity == produced[i].quantity);
        assert(expected[i].price == produced[i].price);
    }

    std::cout << "REPLAY TEST PASSED\n";
    return 0;
}
