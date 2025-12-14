#pragma once

#include <cstdint>
#include <vector>

using Entity = uint32_t;
namespace Carnival::ECS {
    struct TransformComponent {
        float x{};
        float y{};
        float z{};
    };

    struct NetworkComponent {
        uint32_t netID{};
        bool dirty{ false };
    };


    class Archetype
    {
    public:
        Entity CreateEntity()
        {
            Entity e = s_count++;

            m_transforms.emplace_back();
            m_networks.emplace_back();

            return e;
        }

        TransformComponent& GetTransform(Entity e)
        {
            return m_transforms[e];
        }

        NetworkComponent& GetNetwork(Entity e)
        {
            return m_networks[e];
        }

        uint32_t Count() const
        {
            return s_count;
        }

    private:
        std::vector<TransformComponent> m_transforms;
        std::vector<NetworkComponent>   m_networks;
        inline static uint32_t s_count{ 0 };
    };
}