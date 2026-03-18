#pragma once

#include "trading/core/order_types.hpp"

#include <algorithm>
#include <memory>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace trading {

struct OrderStateStoreConfig {
    bool enable_event_history = true;
    std::size_t max_events_per_order = 32;
};

class OrderStateStore {
public:
    explicit OrderStateStore(OrderStateStoreConfig config = {})
        : config_(config) {}

    void register_new_order(const Order& order) {
        std::unique_lock lock(mutex_);
        orders_[order.id] = order;
        append_event_locked(order.id, OrderEventType::Created, "Order registered in state store");
    }

    void apply_send_result(const SendResult& result) {
        std::unique_lock lock(mutex_);
        orders_[result.order.id] = result.order;
        append_event_locked(result.order.id,
                            result.success ? OrderEventType::Sent : OrderEventType::Rejected,
                            result.message);
    }

    bool mark_canceled(OrderId order_id, const std::string& message) {
        std::unique_lock lock(mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }

        it->second.status = OrderStatus::Canceled;
        it->second.updated_at = Clock::now();
        append_event_locked(order_id, OrderEventType::Canceled, message);
        return true;
    }

    bool upsert_order(const Order& order, OrderEventType event_type, const std::string& message) {
        std::unique_lock lock(mutex_);
        orders_[order.id] = order;
        append_event_locked(order.id, event_type, message);
        return true;
    }

    std::optional<Order> get_order(OrderId order_id) const {
        std::shared_lock lock(mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<Order> get_orders_by_status(OrderStatus status) const {
        std::shared_lock lock(mutex_);
        std::vector<Order> result;
        for (const auto& [id, order] : orders_) {
            (void)id;
            if (order.status == status) {
                result.push_back(order);
            }
        }
        return result;
    }

    std::vector<Order> get_orders_by_symbol(const std::string& symbol) const {
        std::shared_lock lock(mutex_);
        std::vector<Order> result;
        for (const auto& [id, order] : orders_) {
            (void)id;
            if (order.symbol == symbol) {
                result.push_back(order);
            }
        }
        return result;
    }

    std::size_t size() const {
        std::shared_lock lock(mutex_);
        return orders_.size();
    }

    std::vector<OrderEvent> get_event_history(OrderId order_id) const {
        std::shared_lock lock(mutex_);
        auto it = event_history_.find(order_id);
        if (it == event_history_.end()) {
            return {};
        }
        return it->second;
    }

    std::string summary_string() const {
        std::shared_lock lock(mutex_);

        std::size_t pending = 0;
        std::size_t open = 0;
        std::size_t partially_filled = 0;
        std::size_t filled = 0;
        std::size_t canceled = 0;
        std::size_t rejected = 0;

        for (const auto& [id, order] : orders_) {
            (void)id;
            switch (order.status) {
                case OrderStatus::Pending: ++pending; break;
                case OrderStatus::Open: ++open; break;
                case OrderStatus::PartiallyFilled: ++partially_filled; break;
                case OrderStatus::Filled: ++filled; break;
                case OrderStatus::Canceled: ++canceled; break;
                case OrderStatus::Rejected: ++rejected; break;
            }
        }

        std::ostringstream oss;
        oss << "total=" << orders_.size()
            << ", pending=" << pending
            << ", open=" << open
            << ", partially_filled=" << partially_filled
            << ", filled=" << filled
            << ", canceled=" << canceled
            << ", rejected=" << rejected;
        return oss.str();
    }

private:
    void append_event_locked(OrderId order_id,
                             OrderEventType event_type,
                             const std::string& message) {
        if (!config_.enable_event_history) {
            return;
        }

        auto& events = event_history_[order_id];
        events.push_back(OrderEvent{order_id, event_type, Clock::now(), message});

        if (events.size() > config_.max_events_per_order) {
            const auto erase_count = events.size() - config_.max_events_per_order;
            events.erase(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(erase_count));
        }
    }

private:
    OrderStateStoreConfig config_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<OrderId, Order> orders_;
    std::unordered_map<OrderId, std::vector<OrderEvent>> event_history_;
};

using OrderStateStorePtr = std::shared_ptr<OrderStateStore>;

}  // namespace trading
