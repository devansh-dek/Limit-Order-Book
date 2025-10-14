// Event parser for reading orders from text files
// Supports CSV format for realistic order replay
#pragma once

#include "data/event.hpp"
#include "engine/order.hpp"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>
#include <iostream>

namespace elob {

// Parse a single CSV line into an Event (NEW_ORDER)
// Format: timestamp,order_id,side,price,quantity
// Example: 0,1,BUY,10000,100
std::optional<Event> parse_csv_order(const std::string& line) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
        return std::nullopt;
    }

    std::istringstream iss(line);
    std::string field;
    std::vector<std::string> fields;

    // Split by comma
    while (std::getline(iss, field, ',')) {
        // Trim whitespace
        size_t start = field.find_first_not_of(" \t");
        size_t end = field.find_last_not_of(" \t");
        if (start != std::string::npos) {
            fields.push_back(field.substr(start, end - start + 1));
        } else {
            fields.push_back("");
        }
    }

    // Require exactly 5 fields
    if (fields.size() != 5) {
        throw std::runtime_error("CSV line must have 5 fields: timestamp,order_id,side,price,quantity");
    }

    try {
        uint64_t timestamp = std::stoull(fields[0]);
        uint64_t order_id = std::stoull(fields[1]);
        std::string side_str = fields[2];
        int64_t price = std::stoll(fields[3]);
        uint64_t quantity = std::stoull(fields[4]);

        // Parse side
        Side side;
        if (side_str == "BUY" || side_str == "B") {
            side = Side::Buy;
        } else if (side_str == "SELL" || side_str == "S") {
            side = Side::Sell;
        } else {
            throw std::runtime_error("Unknown side: " + side_str + " (must be BUY/B or SELL/S)");
        }

        // Create order and wrap in event
        Order order{order_id, side, price, quantity, timestamp};
        Event event;
        event.event_id = order_id;
        event.timestamp = timestamp;
        event.payload = NewOrder{order};
        return event;
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Failed to parse CSV line: " + line + " - " + e.what());
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Numeric value out of range in CSV line: " + line);
    }
}

// Parse action commands from CSV
// Format: action,order_id[,new_price,new_quantity]
// Examples:
//   CANCEL,1
//   MODIFY,1,10050,75
std::optional<Event> parse_csv_action(const std::string& line) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
        return std::nullopt;
    }

    std::istringstream iss(line);
    std::string field;
    std::vector<std::string> fields;

    while (std::getline(iss, field, ',')) {
        size_t start = field.find_first_not_of(" \t");
        size_t end = field.find_last_not_of(" \t");
        if (start != std::string::npos) {
            fields.push_back(field.substr(start, end - start + 1));
        } else {
            fields.push_back("");
        }
    }

    if (fields.empty()) {
        return std::nullopt;
    }

    std::string action = fields[0];

    try {
        if (action == "CANCEL") {
            if (fields.size() != 2) {
                throw std::runtime_error("CANCEL requires: CANCEL,order_id");
            }
            uint64_t order_id = std::stoull(fields[1]);
            Event event;
            event.event_id = order_id;
            event.timestamp = 0;
            event.payload = Cancel{order_id};
            return event;
        } else if (action == "MODIFY") {
            if (fields.size() != 4) {
                throw std::runtime_error("MODIFY requires: MODIFY,order_id,new_price,new_quantity");
            }
            uint64_t order_id = std::stoull(fields[1]);
            int64_t new_price = std::stoll(fields[2]);
            uint64_t new_quantity = std::stoull(fields[3]);
            Event event;
            event.event_id = order_id;
            event.timestamp = 0;
            event.payload = Modify{order_id, new_quantity, new_price};
            return event;
        } else {
            throw std::runtime_error("Unknown action: " + action);
        }
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Failed to parse action line: " + line + " - " + e.what());
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Numeric value out of range in action line: " + line);
    }
}

// Parse a generic CSV line that could be either order or action
std::optional<Event> parse_csv_line(const std::string& line) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
        return std::nullopt;
    }

    // Detect type from first field
    std::istringstream iss(line);
    std::string first_field;
    std::getline(iss, first_field, ',');

    size_t start = first_field.find_first_not_of(" \t");
    size_t end = first_field.find_last_not_of(" \t");
    if (start != std::string::npos) {
        first_field = first_field.substr(start, end - start + 1);
    }

    // Check if it's an action or numeric (order)
    if (first_field == "CANCEL" || first_field == "MODIFY") {
        return parse_csv_action(line);
    } else {
        return parse_csv_order(line);
    }
}

// Load events from a CSV file
// Returns vector of parsed events
std::vector<Event> load_events_from_csv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::vector<Event> events;
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;
        
        try {
            auto maybe_event = parse_csv_line(line);
            if (maybe_event.has_value()) {
                events.push_back(maybe_event.value());
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning at line " << line_number << ": " << e.what() << "\n";
        }
    }

    file.close();
    return events;
}

}  // namespace elob
