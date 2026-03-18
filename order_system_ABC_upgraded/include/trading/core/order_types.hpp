#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trading {

using Clock = std::chrono::system_clock;
using Timestamp = Clock::time_point;
using OrderId = std::uint64_t;

enum class OrderSide : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

enum class OrderType : std::uint8_t {
    Market = 0,
    Limit = 1,
    Ioc = 2,
};

// 주문 상태
// Pending: 아직 내부적으로만 생성됨
// Open: 거래소가 접수함
// PartiallyFilled: 일부 체결
// Filled: 전량 체결
// Canceled: 취소 완료
// Rejected: 거부됨
enum class OrderStatus : std::uint8_t {
    Pending = 0,
    Open = 1,
    PartiallyFilled = 2,
    Filled = 3,
    Canceled = 4,
    Rejected = 5,
};

struct OrderRequest {
    std::string symbol;
    OrderSide side = OrderSide::Buy;
    OrderType type = OrderType::Market;
    double quantity = 0.0;
    double limit_price = 0.0;

    OrderRequest() = default;

    OrderRequest(std::string_view sym,
                 OrderSide s,
                 OrderType t,
                 double qty,
                 double limit)
        : symbol(sym), side(s), type(t), quantity(qty), limit_price(limit) {}
};

struct Order {
    OrderId id = 0;
    std::string symbol;
    OrderSide side = OrderSide::Buy;
    OrderType type = OrderType::Market;
    double quantity = 0.0;
    double limit_price = 0.0;
    double filled_quantity = 0.0;
    double average_fill_price = 0.0;
    OrderStatus status = OrderStatus::Pending;
    Timestamp created_at{};
    Timestamp updated_at{};
};

struct Position {
    std::string symbol;
    double quantity = 0.0;
    double average_entry_price = 0.0;
    double last_mark_price = 0.0;
};

struct AccountSnapshot {
    double cash_balance = 0.0;
    std::vector<Position> positions;
};

struct SendResult {
    bool success = false;
    std::string message;
    Order order;
};

enum class OrderEventType : std::uint8_t {
    Created = 0,
    Sent,
    Rejected,
    Canceled,
    PartiallyFilled,
    Filled,
    Refreshed,
};

struct OrderEvent {
    OrderId order_id = 0;
    OrderEventType type = OrderEventType::Created;
    Timestamp event_time{};
    std::string message;
};

inline std::string to_string(OrderSide side) {
    return side == OrderSide::Buy ? "BUY" : "SELL";
}

inline std::string to_string(OrderType type) {
    switch (type) {
        case OrderType::Market: return "MARKET";
        case OrderType::Limit: return "LIMIT";
        case OrderType::Ioc: return "IOC";
    }
    return "UNKNOWN";
}

inline std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::Pending: return "PENDING";
        case OrderStatus::Open: return "OPEN";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Canceled: return "CANCELED";
        case OrderStatus::Rejected: return "REJECTED";
    }
    return "UNKNOWN";
}

inline std::string to_string(OrderEventType event_type) {
    switch (event_type) {
        case OrderEventType::Created: return "CREATED";
        case OrderEventType::Sent: return "SENT";
        case OrderEventType::Rejected: return "REJECTED";
        case OrderEventType::Canceled: return "CANCELED";
        case OrderEventType::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderEventType::Filled: return "FILLED";
        case OrderEventType::Refreshed: return "REFRESHED";
    }
    return "UNKNOWN";
}

inline bool is_valid_request(const OrderRequest& request) {
    if (request.symbol.empty()) return false;
    if (request.quantity <= 0.0) return false;
    if ((request.type == OrderType::Limit || request.type == OrderType::Ioc) && request.limit_price <= 0.0) {
        return false;
    }
    return true;
}

inline bool is_terminal_status(OrderStatus status) {
    return status == OrderStatus::Filled ||
           status == OrderStatus::Canceled ||
           status == OrderStatus::Rejected;
}

}  // namespace trading
