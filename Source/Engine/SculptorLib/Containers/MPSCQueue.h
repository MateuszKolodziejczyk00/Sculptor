#pragma once

#include "Utility/Threading/ThreadUtils.h"
#include "Utility/Templates/TypeStorage.h"

#include <optional>


namespace spt::lib
{

// multi-producer/single-consumer waitfree queue
// Based on implementation provided by Dmitry Vyukov:
// https://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
template<typename TType>
class MPSCQueue
{
public:

    MPSCQueue()
    {
        m_stub = new Node();
        m_head.store(m_stub, std::memory_order_release);
        m_tail = m_stub;
    }

    ~MPSCQueue()
    {
        Node* current = m_tail;
        while (current && current != m_stub) //  delete stub after the loop as we have to do this anyway because stub may be before tail
        {
            Node* next = current->next.load(std::memory_order_acquire);
            delete current;
            current = next;
        }

        delete m_stub;
    }

    template<typename... TArgs>
    void Enqueue(TArgs&&... args)
    {
        Node* newNode = new Node(std::forward<TArgs>(args)...);

        EnqueueNode(newNode);
    }

    std::optional<TType> Dequeue()
    {
        Node* tail = m_tail;
        Node* next = tail->next.load(std::memory_order_acquire);

        if (tail == m_stub)
        {
            if (next == nullptr)
            {
                return std::nullopt;
            }

            m_tail = next;
            tail = next;
            next = next->next.load(std::memory_order_acquire);
        }

        if (next)
        {
            const std::optional<TType> res = tail->value;
            delete tail;
            m_tail = next;
            return res;
        }

        Node* head = m_head.load(std::memory_order_acquire);
        if (tail != head)
        {
            return std::nullopt;
        }

        m_stub->next = nullptr;
        EnqueueNode(m_stub);
        next = tail->next;

        if (next)
        {
            const std::optional<TType> res = tail->value;
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

    void EnqueueNode(Node* node)
    {
        Node* prev = m_head.exchange(node, std::memory_order_acq_rel); // serialization-point wrt producers, acquire-release
        prev->next.store(node, std::memory_order_release); // serialization-point wrt consumer, release
    }

    // Producer and (rarely) consumer data 

    std::atomic<Node*>  m_head;
    
    // Consumer only data ======== 

    alignas(InterferenceProps::destructiveInterferenceSize) Node* m_tail;
    Node* m_stub;
};

} // spt::lib