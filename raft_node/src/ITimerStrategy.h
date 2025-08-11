#pragma once

/// Интерфейс «Стратегии» таймера: задаёт, через сколько мс сработать,
/// и что делать при срабатывании.
class ITimerStrategy {
public:
    virtual ~ITimerStrategy() = default;
    virtual int nextTimeout() = 0;
    virtual void onTimeout() = 0;
};