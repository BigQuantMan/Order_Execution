# order_system_ABC_upgraded

이 프로젝트는 기존 A+B 예제를 바탕으로, 다음 기능까지 한 단계 확장한 학습용 C++ 프로젝트입니다.

- A. 주문 전송 계층 (`OrderTransmissionService`)
- B. 주문 상태 저장소 / 조회 계층 (`OrderStateStore`)
- C. 거래소 추상화 계층 (`ExchangeConnector`, `MockExchangeConnector`)

## 이번에 C 단계에서 실제로 추가된 점

기존 A+B 원본에도 `ExchangeConnector` / `MockExchangeConnector`가 있었지만, 당시 구현은 사실상 **주문 접수/취소용 얇은 껍데기**에 가까웠습니다.
이번 버전에서는 C 계층이 실제로 다음 상태를 들고 있도록 확장했습니다.

- 주문 상태 (`OPEN`, `FILLED`, `CANCELED`, `REJECTED`)
- 시세 (`set_market_price`)
- 현금 잔고 (`cash_balance`)
- 포지션 수량 / 평균단가 (`Position`)
- 지정가 주문의 조건부 체결

즉, 이제는 `main.cpp`가 주문을 return하는 구조가 아니라,
**거래소 객체가 내부적으로 주문과 계좌 상태를 관리하는 구조**가 분명해졌습니다.

## 핵심 구조

- `OrderTransmissionService`
  - 전략이 만든 `OrderRequest`를 받아 주문 ID를 부여하고 거래소로 전송합니다.
  - 시세 갱신 후, 거래소에서 바뀐 주문 상태를 `OrderStateStore`에 다시 반영합니다.
- `OrderStateStore`
  - 주문들의 현재 상태와 이벤트 이력을 저장합니다.
- `ExchangeConnector`
  - 실제 거래소 / 모의 거래소가 공통으로 따라야 하는 인터페이스입니다.
- `MockExchangeConnector`
  - 메모리 안에서 주문, 시세, 잔고, 포지션을 관리하는 모의 거래소입니다.

## 왜 `main()`은 여전히 `return 0;`으로 끝나는가?

C++에서 `main()`의 반환값은 **운영체제에게 프로그램 종료 상태를 알려주는 코드**입니다.

- `return 0;` : 정상 종료
- `return 1;` 등 : 비정상 종료 또는 오류 종료를 표현할 수 있음

따라서 주문은 `main()`이 반환하는 것이 아닙니다.
주문은 프로그램이 실행되는 동안 다음 흐름으로 이동합니다.

```text
전략 -> A(OrderTransmissionService) -> C(ExchangeConnector / MockExchangeConnector)
                                  \-> B(OrderStateStore)
```

## 파일 설명

- `include/trading/core/order_types.hpp`
  - 공통 타입, enum, `Order`, `OrderRequest`, `Position` 정의
- `include/trading/exchange/exchange_connector.hpp`
  - 거래소 인터페이스
- `include/trading/exchange/mock_exchange_connector.hpp`
  - 모의 거래소 (주문/시세/잔고/포지션 관리)
- `include/trading/state/order_state_store.hpp`
  - B 기능의 핵심 구현
- `include/trading/execution/order_transmission_service.hpp`
  - A 기능의 핵심 구현
- `demo/main.cpp`
  - A+B+C 통합 사용 예제

## 빌드 방법

### Linux / macOS / WSL

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude demo/main.cpp -o order_system_demo
./order_system_demo
```

### Windows (MinGW g++)

```powershell
g++ -std=c++20 -Wall -Wextra -Wpedantic -Iinclude demo/main.cpp -o order_system_demo.exe
.\order_system_demo.exe
```

### Visual Studio 2022 / VS Code + CMake

기존과 동일하게 폴더를 열고 `order_system_demo` 타겟을 실행하면 됩니다.

## 현재 단순화한 가정

학습용 첫 버전이므로 아래는 아직 단순화했습니다.

- 부분 체결 없음
- 슬리피지 없음
- 오더북 깊이 없음
- 기본적으로 공매도 없음
- 시장가 주문은 최근 시세로 즉시 체결
- 지정가 주문은 가격 조건을 만족하면 전량 체결

이 구조를 바탕으로 다음 단계에서 확장할 수 있습니다.

- D. 체결 이벤트 스트림
- E. 포트폴리오 / 계좌 평가 손익(PnL)
- F. 리스크 체크 / 주문 가능 수량 제한
- G. 실제 거래소 어댑터(BinanceConnector 등)
