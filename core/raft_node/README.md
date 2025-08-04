# 📦 raft_node

**`raft_node`** — лёгкая C++ библиотека для реализации алгоритма Raft с поддержкой gRPC. Модуль предоставляет:

- Логику узла Raft: состояния Follower, Candidate, Leader и обработку RPC (`AppendEntries`, `RequestVote`, `GetLeader`, `InstallSnapshot`).
- Таймеры выборов и heartbeat на основе `std::thread` и `std::condition_variable`.
- Простую интеграцию в проекты через CMake.
- Возможность расширения: персистентность лога, TLS-шифрование и снапшоты.

