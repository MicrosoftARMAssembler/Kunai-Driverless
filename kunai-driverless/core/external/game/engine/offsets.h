#pragma once

namespace game {
    namespace offsets {
        constexpr std::uint32_t gworld{ 0xC1AD2A0 };
        constexpr std::uint32_t fname_pool{ 0xc34b800 };
        constexpr std::uint32_t bone_array{ 0x0738 };
        constexpr std::uint32_t bone_array_cache{ 0x0748 };
        constexpr std::uint32_t component_to_world{ 0x02D0 };
        constexpr std::uint32_t bone_count{ 0x0740 };
        constexpr std::uint32_t owning_game_instance{ 0x1d8 };
        constexpr std::uint32_t game_state{ 0x178 };
        constexpr std::uint32_t levels{ 0x190 };
        constexpr std::uint32_t actor_clustor{ 0xa0 };
        constexpr std::uint32_t local_players{ 0x40 };
        constexpr std::uint32_t player_controller{ 0x38 };
        constexpr std::uint32_t acknowledged_pawn{ 0x510 };
        constexpr std::uint32_t player_camera_manager{ 0x520 };
        constexpr std::uint32_t player_state{ 0x460 };
        constexpr std::uint32_t mesh{ 0x4e8 };
        constexpr std::uint32_t team_component{ 0x6a0 };
        constexpr std::uint32_t b_is_player_character{ 0x150d };
        constexpr std::uint32_t b_ai_controlled{ 0x150c };
        constexpr std::uint32_t view_target{ 0x580 };
        constexpr std::uint32_t pov{ 0x10 };
        constexpr std::uint32_t camera_location{ 0x00 };
        constexpr std::uint32_t camera_rotation{ 0x18 };
        constexpr std::uint32_t camera_fov{ 0x30 };
        constexpr std::uint32_t was_ally{ 0xf58 };
        constexpr std::uint32_t damage_handler{ 0xc68 };
        constexpr std::uint32_t actor_id{ 0x38 };
        constexpr std::uint32_t alive{ 0x1f9 };
        constexpr std::uint32_t health{ 0x200 };
        constexpr std::uint32_t shield{ health + sizeof( std::uint64_t ) };
        constexpr std::uint32_t max_shield{ health + sizeof( std::uint64_t ) + sizeof( std::uint32_t ) };

    }
}