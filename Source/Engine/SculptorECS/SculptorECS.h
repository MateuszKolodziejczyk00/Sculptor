#pragma once

#include "entt/entity/registry.hpp"
#include "entt/entity/handle.hpp"


namespace spt::ecs
{

using namespace entt;

using GenericRegistry   = entt::registry;
using GenericEntity     = entt::entity;
using GenericNullEntity = entt::null_t;

} // spt::ecs


namespace std
{

template<>
struct hash<entt::type_info>
{
    size_t operator()(const entt::type_info& info) const
    {
        return static_cast<size_t>(info.hash());
    }
};

} // std
