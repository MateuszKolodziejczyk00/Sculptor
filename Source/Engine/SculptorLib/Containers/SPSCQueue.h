#pragma once

#include "Utility/Threading/ThreadUtils.h"
#include "Utility/Templates/TypeStorage.h"

#include <optional>


namespace spt::lib
{

// single-producer/single-consumer waitfree queue
// Based on implementation provided by Dmitry Vyukov:
// https://www.1024cores.net/home/lock-free-algorithms/queues/unbounded-spsc-queue
template<typename TType> 
class SPSCQueue
{
public:

    SPSCQueue()
    {
        Node* initial = new Node;
        initial->next.exchange(nullptr, std::memory_order_relaxed);
        m_tail = m_head = m_first = m_tailCopy = initial;
    }

    ~SPSCQueue()
    {
        Node* current = m_first;

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
        Node* newNode = AllocNode();
        newNode->next.exchange(nullptr, std::memory_order_relaxed);
        newNode->value.Construct(std::forward<TArgs>(args)...);
        m_head->next.exchange(newNode, std::memory_order_release);
        m_head = newNode;
    }

    std::optional<TType> Dequeue()
    {
        const Node* tailPtr = m_tail.load(std::memory_order_relaxed);
        Node* tailNext = tailPtr->next.load(std::memory_order_acquire);
        if (tailNext)
        {
            const std::optional<TType> result = std::move(tailNext->value.Get());
            tailNext->value.Destroy();
            m_tail.exchange(tailNext);
            return result;
        }
        else
        {
            return std::nullopt;
        }
    }

private:

    // internal node structure 
    struct Node
    {
        std::atomic<Node*>      next;
        lib::TypeStorage<TType> value;
    };

    Node* AllocNode()
    {
        // first tries to allocate node from internal node cache, 
        // if attempt fails, allocates node via ::operator new() 
        if (m_first != m_tailCopy)
        {
            Node* n = m_first;
            m_first = m_first->next.load(std::memory_order_relaxed);
            return n;
        }

        m_tailCopy = m_tail.load(std::memory_order::acquire);

        if (m_first != m_tailCopy)
        {
            Node* n = m_first;
            m_first = m_first->next.load(std::memory_order_relaxed);
            return n;
        }

        Node* n = new Node();
        return n;
    }

    // consumer part 
    // accessed mainly by consumer, infrequently be producer 

    std::atomic<Node*> m_tail; // tail of the queue 

    // producer part 
    // accessed only by producer 

    alignas(InterferenceProps::destructiveInterferenceSize) Node* m_head; // head of the queue 

    Node* m_first; // last unused node (tail of node cache) 

    Node* m_tailCopy; // helper (points somewhere between first_ and tail_) 

    //SPSCQueue(SPSCQueue const&);

    //SPSCQueue& operator = (SPSCQueue const&);
};

} // spt::lib