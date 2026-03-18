# order_system_ABC_upgraded
- A. 주문 전송 계층 (`OrderTransmissionService`)
- B. 주문 상태 저장소 / 조회 계층 (`OrderStateStore`)
- C. 거래소 추상화 계층 (`ExchangeConnector`, `MockExchangeConnector`)
- %%%%%%%%%%%%%%%%%%%%%%%%현재 버전은 여기까지%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
- D. 체결 이벤트 스트림
- E. 포트폴리오 / 계좌 평가 손익(PnL)
- F. 리스크 체크 / 주문 가능 수량 제한
- G. 실제 거래소 어댑터(BinanceConnector 등)
