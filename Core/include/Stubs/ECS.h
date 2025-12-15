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
        // Statics 
        constexpr static uint8_t componentID = 0;

        // Per-Instance
        void serialize(char* buff) const {
            // buff.writeFloat(x);
            // buff.writeFloat(y);
            // buff.writeFloat(z);
        }
        void deserialize(char* buff) {
            // x = buff.readFloat();
            // y = buff.readFloat();
            // z = buff.readFloat();
        }

        float x{};
        float y{};
        float z{};
    };

    struct NetworkComponent {
        constexpr static uint8_t componentID = 1;

        NetworkComponent() : netID{ generateEntityID() }, dirty{ true } {}
        void serialize(char* buff) const {
            // buff.write_u32(netID);
        }
        void deserialize(char* buff) {
            // netID = buff.read_u32();
        }

        uint32_t netID;
        bool dirty;
    };

    class Archetype
    {
    public:
        Entity CreateEntity() {
            Entity e = m_Count++;

            m_Transforms.emplace_back();
            m_Networks.emplace_back();

            return e;
        }

        std::vector<TransformComponent>& GetTransforms() { return m_Transforms; }
        std::vector<NetworkComponent>& GetNetworks() { return m_Networks; }

        uint32_t Count() const { return m_Count; }

        void serialize(char* buff) {
            // Based on Component ID sorting Call Serialize on Each component
            for (size_t i{}; i < m_Count; i++)
            {
                m_Transforms[i].serialize(buff);
                m_Networks[i].serialize(buff);
            }
        }
        void deserialize(char* buff) {
            for (size_t i{}; i < m_Count; i++)
            {
                m_Transforms[i].deserialize(buff);
                m_Networks[i].deserialize(buff);
            }
        }

    private:
        std::vector<TransformComponent> m_Transforms;
        std::vector<NetworkComponent>   m_Networks;

        inline static uint32_t m_Count{ 0 };
        inline static uint16_t s_ArchetypeID{ generateArchetypeID() }; // seems unsafe??
    };
}