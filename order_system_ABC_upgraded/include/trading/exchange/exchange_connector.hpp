#pragma once

#include "trading/core/order_types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace trading {

class ExchangeConnector {
public:
    virtual ~ExchangeConnector() = default;

    virtual SendResult send_order(const Order& order) = 0;
    virtual bool cancel_order(OrderId order_id) = 0;
    virtual std::optional<Order> query_order(OrderId order_id) const = 0;
    virtual std::string connector_name() const = 0;

    virtual void set_market_price(const std::string& symbol, double price) {
        (void)symbol;
        (void)price;
    }

    virtual double get_cash_balance() const {
        return 0.0;
    }

    virtual std::optional<Position> get_position(const std::string& symbol) const {
        (void)symbol;
        return std::nullopt;
    }

    virtual std::vector<Position> get_all_positions() const {
        return {};
    }

    virtual std::optional<double> get_market_price(const std::string& symbol) const {
        (void)symbol;
        return std::nullopt;
    }
};

}  // namespace trading
