#pragma once

#include <atomic>
#include <memory>

class AsyncCancellationToken {
public:
    AsyncCancellationToken() = default;

    explicit AsyncCancellationToken(std::shared_ptr<std::atomic<bool>> cancellationFlag)
        : m_cancellationFlag(std::move(cancellationFlag)) {}

    bool isCancellationRequested() const {
        return m_cancellationFlag
            && m_cancellationFlag->load(std::memory_order_relaxed);
    }

private:
    std::shared_ptr<std::atomic<bool>> m_cancellationFlag;
};