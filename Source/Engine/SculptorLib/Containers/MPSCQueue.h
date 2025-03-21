#pragma once

#include "Utility/Threading/ThreadUtils.h"
#include "Utility/Templates/TypeStorage.h"

#include <optional>


namespace spt::lib
{

// multi-producer/single-consumer waitfree queue
// Based on implementation provided by Dmitry Vyukov:
// https://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
template<typename TType>
class MPSCQueue
{
public:

    MPSCQueue()
    {
        Node* stub = new Node();
        m_head.store(stub, std::memory_order_release);
        m_tail = stub;
    }

    ~MPSCQueue()
    {
        Node* current = m_tail;
        while (current)
        {
            Node* next = current->next.load(std::memory_order_acquire);
            delete current;
            current = next;
        }
    }

    template<typename... TArgs>
    void Enqueue(TArgs&&... args)
    {
        Node* newNode = new Node(std::forward<TArgs>(args)...);

        Node* prev = m_head.exchange(newNode, std::memory_order_acq_rel); // serialization-point wrt producers, acquire-release
        prev->next.store(newNode, std::memory_order_release); // serialization-point wrt consumer, release
    }

    std::optional<TType> Dequeue()
    {
        Node* tail = m_tail;
        Node* next = tail->next.load(std::memory_order_acquire);

        if (next)
        {
            const std::optional<TType> res = next->value;
            delete tail;
            m_tail = next;
            return res;
        }

        return std::nullopt;
    }

private:

    struct Node
    {
        Node()
            : next(nullptr)
        { }

        template<typename... TArgs>
        explicit Node(TArgs&&... args)
            : next(nullptr)
            , value(std::forward<TArgs>(args)...)
        { }

        std::atomic<Node*>  next = nullptr;
        TType               value;
    };

    // Producer only data 

    std::atomic<Node*>  m_head;
    
    // Consumer only data ======== 

    SPT_ALIGNAS_CACHE_LINE Node* m_tail;
};

} // spt::lib