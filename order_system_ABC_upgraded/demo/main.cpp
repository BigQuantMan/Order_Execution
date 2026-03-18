#include "trading/exchange/mock_exchange_connector.hpp"
#include "trading/execution/order_transmission_service.hpp"
#include "trading/state/order_state_store.hpp"

#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

using namespace trading;

namespace {

void print_account(const OrderTransmissionService& service) {
    std::cout << "=== ACCOUNT SNAPSHOT ===\n";
    std::cout << std::fixed;
    std::cout << "cash_balance=" << std::setprecision(2) << service.get_cash_balance() << "\n";

    const auto positions = service.get_all_positions();
    if (positions.empty()) {
        std::cout << "positions=(none)\n\n";
        return;
    }

    for (const auto& pos : positions) {
        std::cout << "symbol=" << pos.symbol
                  << ", qty=" << std::setprecision(6) << pos.quantity
                  << ", avg_entry=" << std::setprecision(2) << pos.average_entry_price
                  << ", mark=" << std::setprecision(2) << pos.last_mark_price << "\n";
    }
    std::cout << "\n";
}

void print_send_result(const SendResult& result) {
    std::cout << std::fixed;
    std::cout << "order_id=" << result.order.id
              << ", symbol=" << result.order.symbol
              << ", side=" << to_string(result.order.side)
              << ", type=" << to_string(result.order.type)
              << ", status=" << to_string(result.order.status)
              << ", filled_qty=" << std::setprecision(6) << result.order.filled_quantity
              << ", avg_fill_price=" << std::setprecision(2) << result.order.average_fill_price
              << ", message=" << result.message << "\n";
}

}  // namespace

int main() {
    MockExchangeConfig exchange_config{};
    exchange_config.ack_latency_ms = 0.5;
    exchange_config.initial_cash_balance = 10000.0;
    exchange_config.fee_rate = 0.001;

    auto connector = std::make_shared<MockExchangeConnector>(exchange_config);

    auto state_store = std::make_shared<OrderStateStore>();

    OrderTransmissionConfig transmission_config{};
    transmission_config.enable_logging = true;
    transmission_config.reject_invalid_request = true;
    transmission_config.refresh_from_connector_on_query = true;
    transmission_config.venue_name = "BINANCE_SPOT_SIMULATION";

    OrderTransmissionService service(connector, state_store, transmission_config);

    service.update_market_price("BTCUSDT", 90000.0);
    service.update_market_price("ETHUSDT", 3200.0);

    OrderRequest request_1{"BTCUSDT", OrderSide::Buy, OrderType::Market, 0.01, 0.0};
    SendResult result_1 = service.send(request_1);

    std::cout << "=== SEND RESULT 1 ===\n";
    print_send_result(result_1);
    print_account(service);

    OrderRequest request_2{"BTCUSDT", OrderSide::Sell, OrderType::Limit, 0.005, 95000.0};
    SendResult result_2 = service.send(request_2);

    std::cout << "=== SEND RESULT 2 ===\n";
    print_send_result(result_2);
    std::cout << "\n";

    std::cout << "=== STATE STORE SUMMARY AFTER SEND ===\n";
    std::cout << state_store->summary_string() << "\n\n";

    service.update_market_price("BTCUSDT", 96000.0);

    std::cout << "=== AFTER PRICE UPDATE TO 96,000 ===\n";
    const auto queried_2 = service.query(result_2.order.id);
    if (queried_2.has_value()) {
        print_send_result(SendResult{true, "QUERY_RESULT", *queried_2});
    }
    print_account(service);

    std::cout << "=== STATE STORE SUMMARY AFTER FILL ===\n";
    std::cout << state_store->summary_string() << "\n\n";

    OrderRequest request_3{"ETHUSDT", OrderSide::Buy, OrderType::Limit, 1.0, 3000.0};
    SendResult result_3 = service.send(request_3);

    std::cout << "=== SEND RESULT 3 ===\n";
    print_send_result(result_3);
    std::cout << "\n";

    const bool canceled = service.cancel(result_3.order.id);
    std::cout << "=== CANCEL RESULT ===\n";
    std::cout << "order_id=" << result_3.order.id
              << ", canceled=" << (canceled ? "true" : "false") << "\n\n";

    std::cout << "=== EVENT HISTORY FOR ORDER " << result_3.order.id << " ===\n";
    const auto history = state_store->get_event_history(result_3.order.id);
    for (const auto& event : history) {
        std::cout << "order_id=" << event.order_id
                  << ", event=" << to_string(event.type)
                  << ", message=" << event.message << "\n";
    }
    std::cout << "\n";

    std::cout << "=== FINAL STATE STORE SUMMARY ===\n";
    std::cout << state_store->summary_string() << "\n\n";

    return 0;
}
