#pragma once
#include <atomic>
#include <vector>
#include <cstring>

template <typename T>
class LockFreeRingBuffer {
public:
    explicit LockFreeRingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), readPos_(0), writePos_(0) {}

    size_t capacity() const { return capacity_; }

    size_t availableToRead() const {
        auto w = writePos_.load(std::memory_order_acquire);
        auto r = readPos_.load(std::memory_order_relaxed);
        return (w >= r) ? (w - r) : (capacity_ - r + w);
    }

    size_t availableToWrite() const {
        return capacity_ - 1 - availableToRead();
    }

    bool write(const T* data, size_t count) {
        if (availableToWrite() < count) return false;

        auto pos = writePos_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            buffer_[pos] = data[i];
            pos = (pos + 1) % capacity_;
        }
        writePos_.store(pos, std::memory_order_release);
        return true;
    }

    bool read(T* data, size_t count) {
        if (availableToRead() < count) return false;

        auto pos = readPos_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            data[i] = buffer_[pos];
            pos = (pos + 1) % capacity_;
        }
        readPos_.store(pos, std::memory_order_release);
        return true;
    }

    void reset() {
        readPos_.store(0, std::memory_order_relaxed);
        writePos_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<size_t> readPos_;
    std::atomic<size_t> writePos_;
};
