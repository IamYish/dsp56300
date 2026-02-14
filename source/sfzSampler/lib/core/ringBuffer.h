#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <type_traits>

namespace sfz
{
    // Single-producer single-consumer lock-free ring buffer.
    // Used for audio thread <-> MIDI thread communication.
    template<typename T, size_t Capacity>
    class RingBuffer
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    public:
        bool push(const T& item)
        {
            const size_t w = m_write.load(std::memory_order_relaxed);
            const size_t next = (w + 1) & Mask;

            if (next == m_read.load(std::memory_order_acquire))
                return false; // full

            m_data[w] = item;
            m_write.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& item)
        {
            const size_t r = m_read.load(std::memory_order_relaxed);

            if (r == m_write.load(std::memory_order_acquire))
                return false; // empty

            item = m_data[r];
            m_read.store((r + 1) & Mask, std::memory_order_release);
            return true;
        }

        bool empty() const
        {
            return m_read.load(std::memory_order_acquire)
                == m_write.load(std::memory_order_acquire);
        }

        size_t size() const
        {
            const size_t w = m_write.load(std::memory_order_acquire);
            const size_t r = m_read.load(std::memory_order_acquire);
            return (w - r) & Mask;
        }

    private:
        static constexpr size_t Mask = Capacity - 1;
        std::array<T, Capacity> m_data{};
        std::atomic<size_t>     m_write{0};
        std::atomic<size_t>     m_read{0};
    };

} // namespace sfz
