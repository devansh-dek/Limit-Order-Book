#include <iostream>
#include <iomanip>
#include "../src/utils/event_parser.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file>\n";
        return 1;
    }

    std::string csv_file = argv[1];
    
    std::cout << "=== Event Parser Demo ===\n";
    std::cout << "Loading events from: " << csv_file << "\n\n";

    try {
        auto events = elob::load_events_from_csv(csv_file);
        
        std::cout << "Total events loaded: " << events.size() << "\n\n";
        
        // Display parsed events
        std::cout << std::left 
                  << std::setw(12) << "Timestamp"
                  << std::setw(12) << "Order ID"
                  << std::setw(10) << "Type"
                  << std::setw(8) << "Side"
                  << std::setw(10) << "Price"
                  << std::setw(10) << "Quantity"
                  << "\n";
        std::cout << std::string(60, '-') << "\n";
        
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& event = events[i];
            
            std::cout << std::setw(12) << event.event_id;

            // Print event type
            if (std::holds_alternative<elob::NewOrder>(event.payload)) {
                const auto& new_order = std::get<elob::NewOrder>(event.payload);
                std::cout << std::setw(12) << new_order.order.order_id
                          << std::setw(10) << "NEW";
                if (new_order.order.side == elob::Side::Buy) {
                    std::cout << std::setw(8) << "BUY";
                } else {
                    std::cout << std::setw(8) << "SELL";
                }
                std::cout << std::setw(10) << new_order.order.price
                          << std::setw(10) << new_order.order.quantity;
            } else if (std::holds_alternative<elob::Cancel>(event.payload)) {
                const auto& cancel = std::get<elob::Cancel>(event.payload);
                std::cout << std::setw(12) << cancel.order_id
                          << std::setw(10) << "CANCEL"
                          << std::setw(8) << "-"
                          << std::setw(10) << "-"
                          << std::setw(10) << "-";
            } else if (std::holds_alternative<elob::Modify>(event.payload)) {
                const auto& modify = std::get<elob::Modify>(event.payload);
                std::cout << std::setw(12) << modify.order_id
                          << std::setw(10) << "MODIFY"
                          << std::setw(8) << "-"
                          << std::setw(10) << modify.new_price
                          << std::setw(10) << modify.new_quantity;
            } else {
                std::cout << std::setw(12) << "-"
                          << std::setw(10) << "UNKNOWN"
                          << std::setw(8) << "-"
                          << std::setw(10) << "-"
                          << std::setw(10) << "-";
            }
            
            std::cout << "\n";
        }
        
        std::cout << "\n✓ Successfully parsed " << events.size() << " events\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
