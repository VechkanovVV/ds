#include "TimerDelegate.h"
#include <chrono>

TimerDelegate::TimerDelegate() = default;

TimerDelegate::~TimerDelegate() {
    stop();
}

void TimerDelegate::start(std::unique_ptr<ITimerStrategy> strategy) {
    {
        std::lock_guard lk(mutex_);
        current_strategy_ = std::move(strategy);
        strategy_changed_ = true;
        reset_flag_ = false;
        stop_flag_ = false;
    }
    cv_.notify_one();

    worker_ = std::thread(&TimerDelegate::runLoop, this);
}

void TimerDelegate::stop() {
    {
        std::lock_guard lk(mutex_);
        stop_flag_ = true;
    }
    cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void TimerDelegate::reset() {
    {
        std::lock_guard lk(mutex_);
        reset_flag_ = true;
    }
    cv_.notify_one();
}

void TimerDelegate::changeStrategy(std::unique_ptr<ITimerStrategy> strategy) {
    {
        std::lock_guard lk(mutex_);
        current_strategy_ = std::move(strategy);
        strategy_changed_ = true;
    }
    cv_.notify_one();
}

void TimerDelegate::runLoop() {
    while (true) {
        {
            std::unique_lock lk(mutex_);
            if (stop_flag_) break;

            cv_.wait(lk, [&] {
                return stop_flag_ || current_strategy_ != nullptr;
            });

            if (stop_flag_) break;

            const std::shared_ptr<ITimerStrategy> strategy_to_use = current_strategy_;

            if (strategy_changed_ || reset_flag_) {
                strategy_changed_ = false;
                reset_flag_ = false;
                continue;
            }

            const int timeout = strategy_to_use->nextTimeout();
            if (timeout < 0) {
                cv_.wait(lk, [&] {
                    return stop_flag_ || strategy_changed_ || reset_flag_;
                });
                continue;
            }

            const bool expired = !cv_.wait_for(
                lk,
                std::chrono::milliseconds(timeout),
                [&] { return stop_flag_ || reset_flag_ || strategy_changed_; }
            );

            if (stop_flag_) break;
            if (strategy_changed_ || reset_flag_) continue;

            if (expired) {
                lk.unlock();
                strategy_to_use->onTimeout();
                lk.lock();
            }
        }
    }
}