#include "elob/order_book.hpp"

namespace elob {
// OrderBook implementation: cancel and modify logic using the order_index_

bool OrderBook::cancel(uint64_t order_id) {
	auto it = order_index_.find(order_id);
	if (it == order_index_.end()) return false;

	Locator loc = it->second;
	// erase order from its price level
	loc.level->erase_order(loc.it);
	// remove price level if empty
	remove_level_if_empty(loc.side, loc.price);
	// erase index entry
	order_index_.erase(it);
	return true;
}

bool OrderBook::modify(uint64_t order_id, Price new_price, uint64_t new_quantity, uint64_t new_timestamp) {
	auto it = order_index_.find(order_id);
	if (it == order_index_.end()) return false;

	Locator &loc = it->second;
	OrderIterator old_it = loc.it;
	Order &ord = *old_it;

	// Update quantity in place, preserving already-filled amount
	uint64_t filled = 0;
	if (ord.quantity >= ord.remaining) filled = ord.quantity - ord.remaining;
	ord.quantity = new_quantity;
	ord.remaining = (new_quantity > filled) ? (new_price == ord.price ? (new_quantity - filled) : (new_quantity - filled)) : 0;
	ord.timestamp = new_timestamp;

	if (ord.price == new_price) {
		// price unchanged; timestamp updated — repositioning within same level not performed to keep deterministic behavior
		loc.timestamp = new_timestamp;
		return true;
	}

	// Price changed: remove from old level and insert at tail of new level (new time priority)
	Side side = loc.side;
	Price old_price = loc.price;
	PriceLevel *old_level = loc.level;

	// erase from old level
	old_level->erase_order(old_it);
	remove_level_if_empty(side, old_price);

	// create or find new level and append order
	PriceLevel *new_level = nullptr;
	if (side == Side::Buy) {
		auto nit = bids_.find(new_price);
		if (nit == bids_.end()) {
			auto up = std::make_unique<PriceLevel>(new_price);
			new_level = up.get();
			auto res = bids_.emplace(new_price, std::move(up));
			nit = res.first;
		} else {
			new_level = nit->second.get();
		}
	} else {
		auto nit = asks_.find(new_price);
		if (nit == asks_.end()) {
			auto up = std::make_unique<PriceLevel>(new_price);
			new_level = up.get();
			auto res = asks_.emplace(new_price, std::move(up));
			nit = res.first;
		} else {
			new_level = nit->second.get();
		}
	}

	Order new_order = ord; // copy updated order
	new_order.price = new_price;
	new_order.timestamp = new_timestamp;

	auto new_it = new_level->add_order(new_order);

	// update locator to point to new location; the stored Order object lives inside the price level list
	loc.level = new_level;
	loc.price = new_price;
	loc.it = new_it;
	loc.timestamp = new_timestamp;

	return true;
}
}
