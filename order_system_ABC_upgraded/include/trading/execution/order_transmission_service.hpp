#pragma once

#include "trading/exchange/exchange_connector.hpp"
#include "trading/state/order_state_store.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace trading {

struct OrderTransmissionConfig {
    bool enable_logging = true;
    bool reject_invalid_request = true;
    bool refresh_from_connector_on_query = true;
    std::string venue_name = "default-venue";
};

class OrderTransmissionService {
public:
    OrderTransmissionService(std::shared_ptr<ExchangeConnector> connector,
                             OrderStateStorePtr state_store,
                             OrderTransmissionConfig config = {})
        : connector_(std::move(connector)),
          state_store_(std::move(state_store)),
          config_(std::move(config)) {}

    SendResult send(const OrderRequest& request) {
        if (config_.reject_invalid_request && !is_valid_request(request)) {
            Order rejected = build_order(request);
            rejected.status = OrderStatus::Rejected;
            rejected.updated_at = Clock::now();

            if (state_store_) {
                state_store_->register_new_order(rejected);
                state_store_->apply_send_result(SendResult{false, "INVALID_REQUEST", rejected});
            }

            log("Rejected invalid request for symbol=" + request.symbol);
            return SendResult{false, "INVALID_REQUEST", rejected};
        }

        Order order = build_order(request);

        if (state_store_) {
            state_store_->register_new_order(order);
        }

        SendResult result = connector_->send_order(order);

        if (state_store_) {
            state_store_->apply_send_result(result);
        }

        std::ostringstream oss;
        oss << "Sent order id=" << result.order.id
            << ", symbol=" << result.order.symbol
            << ", status=" << to_string(result.order.status)
            << ", message=" << result.message;
        log(oss.str());

        return result;
    }

    bool cancel(OrderId order_id) {
        const bool success = connector_->cancel_order(order_id);
        if (success && state_store_) {
            state_store_->mark_canceled(order_id, "Canceled by OrderTransmissionService");
        }

        log(std::string("Cancel request for id=") + std::to_string(order_id) +
            (success ? " succeeded" : " failed"));
        return success;
    }

    std::optional<Order> query(OrderId order_id) const {
        std::optional<Order> cached;
        if (state_store_) {
            cached = state_store_->get_order(order_id);
        }

        if (!config_.refresh_from_connector_on_query) {
            return cached;
        }

        const auto remote = connector_->query_order(order_id);
        if (remote.has_value() && state_store_) {
            state_store_->upsert_order(*remote, OrderEventType::Refreshed, "Refreshed from connector on query");
        }

        if (remote.has_value()) {
            return remote;
        }
        return cached;
    }

    void update_market_price(const std::string& symbol, double price) {
        connector_->set_market_price(symbol, price);

        if (!state_store_) {
            return;
        }

        const auto same_symbol_orders = state_store_->get_orders_by_symbol(symbol);
        for (const auto& cached_order : same_symbol_orders) {
            if (is_terminal_status(cached_order.status)) {
                continue;
            }

            const auto refreshed = connector_->query_order(cached_order.id);
            if (!refreshed.has_value()) {
                continue;
            }

            if (refreshed->status != cached_order.status ||
                refreshed->filled_quantity != cached_order.filled_quantity ||
                refreshed->average_fill_price != cached_order.average_fill_price) {
                const OrderEventType event_type =
                    refreshed->status == OrderStatus::Filled ? OrderEventType::Filled : OrderEventType::Refreshed;
                state_store_->upsert_order(*refreshed, event_type, "Updated after market price change");
            }
        }
    }

    double get_cash_balance() const {
        return connector_->get_cash_balance();
    }

    std::optional<Position> get_position(const std::string& symbol) const {
        return connector_->get_position(symbol);
    }

    std::vector<Position> get_all_positions() const {
        return connector_->get_all_positions();
    }

private:
    Order build_order(const OrderRequest& request) {
        Order order;
        order.id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
        order.symbol = request.symbol;
        order.side = request.side;
        order.type = request.type;
        order.quantity = request.quantity;
        order.limit_price = request.limit_price;
        order.status = OrderStatus::Pending;
        order.created_at = Clock::now();
        order.updated_at = order.created_at;
        return order;
    }

    void log(const std::string& message) const {
        if (!config_.enable_logging) {
            return;
        }
        (void)message;
    }

private:
    std::shared_ptr<ExchangeConnector> connector_;
    OrderStateStorePtr state_store_;
    OrderTransmissionConfig config_;
    std::atomic<OrderId> next_order_id_{1};
};

}  // namespace trading
