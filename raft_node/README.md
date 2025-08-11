# Raft Node Implementation

Простая реализация Raft консенсуса на C++ с gRPC.

---

## Основные компоненты

### 1. `RaftNode`

Ядро реализации алгоритма Raft:

- Лидеры / фолловеры / кандидаты
- Выборы лидера
- Репликация лога
- Обработка RPC-запросов

---

### 2. `TimerDelegate`

Умный таймер с поддержкой стратегий:

```cpp
class TimerDelegate {
    void start(std::unique_ptr<ITimerStrategy>);
    void stop();
    void reset();
    void changeStrategy(std::unique_ptr<ITimerStrategy>);
};
```

**Как работает:**

- Автоматически перезапускается при смене стратегии
- Потокобезопасный (`mutex` + `condition_variable`)
- Поддерживает разные режимы работы через стратегии

---

### 3. Стратегии таймера

- `ElectionTimerStrategy` — рандомные таймауты для выборов
- `HeartbeatTimerStrategy` — фиксированные интервалы для heartbeat'ов
- `NullTimerStrategy` — заглушка (для пауз)

---

## gRPC API

```proto
service RaftNodeService {
  rpc AppendEntries(AppendEntriesRequest) returns (AppendEntriesResponse);
  rpc RequestVote(RequestVoteRequest) returns (RequestVoteResponse);
  rpc GetLeader(GetLeaderRequest) returns (GetLeaderResponse);
  rpc InstallSnapshot(InstallSnapshotRequest) returns (InstallSnapshotResponse);
}
```

---
