#pragma once

#include <memory>


namespace spt::lib
{

template<typename TType>
using UniquePtr = std::unique_ptr<TType>;

template<typename TType>
using SharedPtr = std::shared_ptr<TType>;

template<typename TType>
using WeakPtr = std::weak_ptr<TType>;

template<typename TType>
using SharedFromThis = std::enable_shared_from_this<TType>;

}