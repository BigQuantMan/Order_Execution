#pragma once

#include "trading/exchange/exchange_connector.hpp"

#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace trading {

struct MockExchangeConfig {
    double ack_latency_ms = 1.0;
    double initial_cash_balance = 100000.0;
    double fee_rate = 0.001;
    bool allow_short_selling = false;    // 롱숏 중 일단 롱만
};

class MockExchangeConnector final : public ExchangeConnector {
public:
    explicit MockExchangeConnector(double ack_latency_ms = 1.0)
        : config_{ack_latency_ms, 100000.0, 0.001, false},
          cash_balance_(config_.initial_cash_balance) {}

    explicit MockExchangeConnector(MockExchangeConfig config)
        : config_(config),
          cash_balance_(config_.initial_cash_balance) {}

    SendResult send_order(const Order& order) override {
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(config_.ack_latency_ms));

        std::lock_guard<std::mutex> lock(mutex_);

        Order accepted = order;
        accepted.updated_at = Clock::now();

        if (accepted.quantity <= 0.0 || accepted.symbol.empty()) {
            accepted.status = OrderStatus::Rejected;
            orders_[accepted.id] = accepted;
            return SendResult{false, "INVALID_ORDER_FIELDS", accepted};
        }

        if (accepted.type == OrderType::Market && !market_prices_.contains(accepted.symbol)) {
            accepted.status = OrderStatus::Rejected;
            orders_[accepted.id] = accepted;
            return SendResult{false, "NO_MARKET_PRICE_FOR_MARKET_ORDER", accepted};
        }

        if ((accepted.type == OrderType::Limit || accepted.type == OrderType::Ioc) && accepted.limit_price <= 0.0) {
            accepted.status = OrderStatus::Rejected;
            orders_[accepted.id] = accepted;
            return SendResult{false, "MISSING_LIMIT_PRICE", accepted};
        }

        accepted.status = OrderStatus::Open;
        orders_[accepted.id] = accepted;

        auto& stored_order = orders_.at(accepted.id);

        if (try_fill_order_locked(stored_order)) {
            return SendResult{true, "FILLED_IMMEDIATELY", stored_order};
        }

        if (stored_order.type == OrderType::Ioc) {
            stored_order.status = OrderStatus::Canceled;
            stored_order.updated_at = Clock::now();
            return SendResult{true, "IOC_NOT_FILLED_AND_CANCELED", stored_order};
        }

        return SendResult{true, "ACK_FROM_MOCK_EXCHANGE", stored_order};
    }

    bool cancel_order(OrderId order_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }

        if (is_terminal_status(it->second.status)) {
            return false;
        }

        it->second.status = OrderStatus::Canceled;
        it->second.updated_at = Clock::now();
        return true;
    }

    std::optional<Order> query_order(OrderId order_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::string connector_name() const override {
        return "MockExchangeConnector";
    }

    void set_market_price(const std::string& symbol, double price) override {
        std::lock_guard<std::mutex> lock(mutex_);
        market_prices_[symbol] = price;

        for (auto& [order_id, order] : orders_) {
            (void)order_id;
            if (order.symbol != symbol) {
                continue;
            }
            if (order.status != OrderStatus::Open) {
                continue;
            }
            try_fill_order_locked(order);
        }

        auto pos_it = positions_.find(symbol);
        if (pos_it != positions_.end()) {
            pos_it->second.last_mark_price = price;
        }
    }

    double get_cash_balance() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return cash_balance_;
    }

    std::optional<Position> get_position(const std::string& symbol) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        if (it == positions_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<Position> get_all_positions() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Position> result;
        result.reserve(positions_.size());
        for (const auto& [symbol, position] : positions_) {
            (void)symbol;
            if (std::abs(position.quantity) > 1e-12) {
                result.push_back(position);
            }
        }
        return result;
    }

    std::optional<double> get_market_price(const std::string& symbol) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = market_prices_.find(symbol);
        if (it == market_prices_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    bool try_fill_order_locked(Order& order) {
        if (order.status != OrderStatus::Open) {
            return false;
        }

        const auto market_it = market_prices_.find(order.symbol);
        if (market_it == market_prices_.end()) {
            return false;
        }
        const double market_price = market_it->second;

        bool should_fill = false;
        double execution_price = market_price;

        switch (order.type) {
            case OrderType::Market:
                should_fill = true;
                execution_price = market_price;
                break;

            case OrderType::Limit:
            case OrderType::Ioc:
                if (order.side == OrderSide::Buy) {
                    should_fill = market_price <= order.limit_price;
                } else {
                    should_fill = market_price >= order.limit_price;
                }
                execution_price = order.limit_price;
                break;
        }

        if (!should_fill) {
            return false;
        }

        return apply_fill_locked(order, execution_price);
    }

    bool apply_fill_locked(Order& order, double execution_price) {
        const double gross_notional = order.quantity * execution_price;
        const double fee = gross_notional * config_.fee_rate;

        if (order.side == OrderSide::Buy) {
            const double total_cost = gross_notional + fee;
            if (cash_balance_ + 1e-12 < total_cost) {
                order.status = OrderStatus::Rejected;
                order.updated_at = Clock::now();
                return false;
            }

            cash_balance_ -= total_cost;

            auto& position = positions_[order.symbol];
            const double old_qty = position.quantity;
            const double new_qty = old_qty + order.quantity;

            position.symbol = order.symbol;
            position.last_mark_price = market_prices_[order.symbol];
            if (new_qty > 0.0) {
                position.average_entry_price =
                    ((old_qty * position.average_entry_price) + (order.quantity * execution_price)) / new_qty;
            } else {
                position.average_entry_price = 0.0;
            }
            position.quantity = new_qty;
        } else {
            auto pos_it = positions_.find(order.symbol);
            const double available_qty = (pos_it == positions_.end()) ? 0.0 : pos_it->second.quantity;

            if (!config_.allow_short_selling && available_qty + 1e-12 < order.quantity) {
                order.status = OrderStatus::Rejected;
                order.updated_at = Clock::now();
                return false;
            }

            cash_balance_ += (gross_notional - fee);

            auto& position = positions_[order.symbol];
            position.symbol = order.symbol;
            position.last_mark_price = market_prices_[order.symbol];
            position.quantity -= order.quantity;
            if (std::abs(position.quantity) <= 1e-12) {
                position.quantity = 0.0;
                position.average_entry_price = 0.0;
            }
        }

        order.filled_quantity = order.quantity;
        order.average_fill_price = execution_price;
        order.status = OrderStatus::Filled;
        order.updated_at = Clock::now();
        return true;
    }

private:
    MockExchangeConfig config_{};
    mutable std::mutex mutex_;
    std::unordered_map<OrderId, Order> orders_;
    std::unordered_map<std::string, double> market_prices_;
    std::unordered_map<std::string, Position> positions_;
    double cash_balance_ = 0.0;
};

}  // namespace trading
