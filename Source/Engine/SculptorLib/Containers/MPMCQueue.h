#pragma once

#include "Utility/Templates/TypeStorage.h"
#include "Containers/StaticArray.h"

#include <optional>
#include <atomic>


namespace spt::lib
{

// multi-producer/single-consumer waitfree queue
// Based on implementation provided by Dmitry Vyukov:
// https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
template<typename TType, SizeType size>
class MPMCQueue
{
    static_assert((size >= 2) && ((size & (size - 1)) == 0));
    static constexpr SizeType bufferMask = size - 1;

public:

    MPMCQueue()
    {
        for (SizeType i = 0; i != size; i += 1)
        {
            m_buffer[i].sequence.store(i, std::memory_order_relaxed);
        }

        m_enqueuePos.store(0, std::memory_order_relaxed);
        m_dequeuePos.store(0, std::memory_order_relaxed);
    }

    template<typename... TArgs>
    bool Enqueue(TArgs&&... args)
    {
        Node* node = nullptr;
        SizeType pos = m_enqueuePos.load(std::memory_order_relaxed);

        while(true)
        {
            node = &m_buffer[pos & bufferMask];

            const SizeType seq = node->sequence.load(std::memory_order_acquire);
            const IntPtr dif = static_cast<IntPtr>(seq) - static_cast<IntPtr>(pos);

            if (dif == 0)
            {
                if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (dif < 0)
            {
                return false;
            }
            else
            {
                pos = m_enqueuePos.load(std::memory_order_relaxed);
            }
        }

        node->value.Construct(std::forward<TArgs>(args)...);
        node->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    std::optional<TType> Dequeue()
    {
        Node* node = nullptr;
        SizeType pos = m_dequeuePos.load(std::memory_order_relaxed);

        while(true)
        {
            node = &m_buffer[pos & bufferMask];

            const SizeType seq = node->sequence.load(std::memory_order_acquire);
            const IntPtr dif = static_cast<IntPtr>(seq) - static_cast<IntPtr>(pos + 1);

            if (dif == 0)
            {
                if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (dif < 0)
            {
                return std::nullopt;
            }
            else
            {
                pos = m_dequeuePos.load(std::memory_order_relaxed);
            }
        }

        std::optional<TType> result = node->value.Get();
        node->sequence.store(pos + bufferMask + 1, std::memory_order_release);

        return result;
    }

private:

    struct Node
    {
        std::atomic<SizeType>   sequence;
        lib::TypeStorage<TType> value;
    };

    lib::StaticArray<Node, size>    m_buffer;

    std::atomic<SizeType>           m_enqueuePos;
    std::atomic<SizeType>           m_dequeuePos;
};

} // spt::lib
