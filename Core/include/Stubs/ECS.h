#pragma once

#include <cstdint>
#include <vector>

using Entity = uint32_t;
namespace Carnival::ECS {

    uint32_t generateEntityID() {
        static uint32_t nextEntityID = 0; // not thread-safe
        return ++nextEntityID;
    }

    uint16_t generateArchetypeID() {
        static uint16_t nextArchetypeID = 0; // not thread-safe
        return ++nextArchetypeID;
    }

    struct TransformComponent {
        constexpr static uint8_t componentID = 0;

        float x{};
        float y{};
        float z{};
    };

    struct NetworkComponent {
        constexpr static uint8_t componentID = 1;

        NetworkComponent() : netID{ generateEntityID() }, dirty{ true } {}

        uint32_t netID;
        bool dirty;
    };

    class Archetype
    {
    public:
        Entity CreateEntity() {
            Entity e = s_count++;

            m_transforms.emplace_back();
            m_networks.emplace_back();

            return e;
        }

        std::vector<TransformComponent>& GetTransforms() { return m_transforms; }
        std::vector<NetworkComponent>& GetNetworks() { return m_networks; }

        uint32_t Count() const { return s_count; }

    private:
        std::vector<TransformComponent> m_transforms;
        std::vector<NetworkComponent>   m_networks;
        inline static uint32_t s_count{ 0 };
        inline static uint16_t s_ArchetypeID{ generateArchetypeID() }; // seems unsafe??
    };
}