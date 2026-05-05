#pragma once

#define current_class reinterpret_cast<uint64_t>( this )

#define declare_member(type, name, offset) type name() { return g_process->read<type>( current_class + offset ); } 
#define declare_member_bit(Bit, Name, Offset) bool Name( ) { return bool( g_process->read<char>( current_class + Offset) & (1 << Bit)); }

#define apply_member(type, name, offset) void name( type val ) { g_process->write<type>( current_class + offset, val); }
#define apply_member_bit(Bit, Name, Offset) void Name( bool Value ) { g_process->write<char>( g_process->read<char>( current_class + Offset) | (1 << Bit), Value); }
#define apply_member_array(type, name, offset) void name(int index, type val) { g_process->write<type>(current_class + offset + (index * sizeof(type)), val); }

namespace game {
	struct bone_info {
		std::int32_t index{};
		f_vector position{};
		bool valid{};
	};

	class u_object {
	public:
		declare_member( u_object*, class_private, 0x08 );
		declare_member( std::uint32_t, name_private, 0x18 );
	};

	class u_actor_component : public u_object {
	public:
	};

	class u_scene_component : public u_actor_component {
	public:
		declare_member( f_transform, component_to_world, offsets::component_to_world );
		declare_member( f_rotator, relative_rotation, 0x188 );
	};

	class a_actor : public u_object {
	public:
		declare_member( u_scene_component*, root_component, 0x288 );
		declare_member( uint32_t, actor_id, offsets::actor_id );
	};

	class u_base_team_component : public u_object {
	public:
	};

	class a_player_state : public u_object {
	public:
		declare_member( std::uint32_t, ping, 0x464 );
		declare_member( tarray<wchar_t>, player_name_private_raw, 0x500 );

		std::string get_player_name( ) {
			auto name_array = this->player_name_private_raw( );
			if ( !name_array.is_valid( ) || name_array.count <= 0 || name_array.count > 64 )
				return {};

			// name_array.data is a remote pointer-to-pointer — read the actual buffer address first
			const auto buf_addr = g_process->read<std::uintptr_t>(
				reinterpret_cast< std::uintptr_t >( name_array.data )
			);
			if ( !utility::is_valid_pointer( buf_addr ) )
				return {};

			std::vector<wchar_t> buf( static_cast< std::size_t >( name_array.count ) );
			if ( !g_process->read_memory(
				buf_addr,
				buf.data( ),
				static_cast< std::size_t >( name_array.count ) * sizeof( wchar_t ) ) )
				return {};

			while ( !buf.empty( ) && buf.back( ) == L'\0' )
				buf.pop_back( );

			std::string result;
			result.reserve( buf.size( ) );
			for ( wchar_t wc : buf )
				result += ( wc < 128 && wc > 0 ) ? static_cast< char >( wc ) : '?';
			return result;
		}
	};

	class u_platform_player : public u_object {
	public:
	};

	class a_ares_player_state : public a_player_state {
	public:
		declare_member( u_base_team_component*, team_component, offsets::team_component );
	};

	class u_primitive_component : public u_scene_component {
	public:
	};

	class u_mesh_component : public u_primitive_component {
	public:
		bool was_recently_rendered( float tung_tung_tung_sahur ) {
			// UPrimitiveComponent::WasRecentlyRendered
			// 40 53 48 83 EC ? 48 8B 81 ? ? ? ? 48 8B D9 0F 29 74 24 ? 0F 28 F1 48 85 C0 75
			auto world = g_process->read<uintptr_t>( current_class + 0xd0 );
			auto delta_time_seconds = g_process->read<float>( world + 0x764 );
			auto time_seconds = g_process->read<double>( world + 0x738 );

			auto last_render_time = g_process->read<float>( current_class + 0x484 ); // BoundsScale + 0x8
			auto time_since = time_seconds - last_render_time;

			auto render_time_threshold = fmaxf( tung_tung_tung_sahur, delta_time_seconds + 0.000099999997f );
			return time_since <= render_time_threshold;
		}
	};

	class u_skeletal_mesh_component : public u_mesh_component {
	public:
#define get_bone( gender, b ) resolve_bone( gender, game::female_bones::b, game::male_bones::b )

		declare_member( tarray<f_transform>, bone_array, offsets::bone_array );
		declare_member( tarray<f_transform>, bone_array_cache, offsets::bone_array_cache );

		tarray<f_transform> get_bone_array( ) {
			auto bone_array = this->bone_array( );
			if ( !bone_array.is_valid( ) )
				bone_array = this->bone_array_cache( );
			return bone_array;
		}

		f_vector get_bone_location( std::uint32_t bone_index ) {
			auto bone_array = this->get_bone_array( );
			auto component_to_world = this->component_to_world( );

			auto bone = bone_array.get( bone_index );
			auto matrix = bone.to_matrix( ).to_multiplication(
				component_to_world.to_matrix( )
			);

			return matrix.w_plane;
		}

		bone_gender get_gender( ) {
			auto bone_array = this->get_bone_array( );
			if ( bone_array.count == static_cast< int >( game::female_bones::bone_max ) ) return bone_gender::female;
			if ( bone_array.count == static_cast< int >( game::male_bones::bone_max ) )   return bone_gender::male;
			return bone_gender::unknown;
		}

		std::uint32_t resolve_bone( bone_gender gender, game::female_bones fb, game::male_bones mb ) {
			return gender == bone_gender::female
				? static_cast< std::uint32_t >( fb )
				: static_cast< std::uint32_t >( mb );
		}
	};

	class a_pawn : public a_actor {
	public:
		declare_member( a_ares_player_state*, player_state, offsets::player_state );
	};

	class a_character : public a_pawn {
	public:
		declare_member( u_skeletal_mesh_component*, mesh, offsets::mesh );
	};

	class a_ares_item : public a_actor {
	public:
	};

	class a_ares_equippable : public a_ares_item {
		//struct TArray<struct USkeletalMeshComponent*> Meshes; // 0xe50(0x10)
	};

	class u_ares_inventory : public u_object {
	public:
		declare_member( a_ares_equippable*, current_equippable, 0x248 );
	};

	class u_damageable_component : public u_object {
	public:
		declare_member( bool, alive, offsets::alive );
		declare_member( float, health, offsets::health );
		declare_member( float, shield, offsets::shield );
		declare_member( float, max_shield, offsets::max_shield );
	};

	class a_shooter_character : public a_character {
	public:
		declare_member( bool, was_ally, offsets::was_ally );
		declare_member( u_damageable_component*, damage_handler, offsets::damage_handler );
		declare_member( float, shield_degeneration_per_second, 0x1118 );
		declare_member( u_ares_inventory*, inventory, 0xc08 );

		bool is_shooter_character( ) {
			return this->shield_degeneration_per_second( ) == 1;
		}
	};

	class f_minimal_view_info : public u_object {
	public:
		declare_member( f_vector, camera_location, offsets::camera_location );
		declare_member( f_rotator, camera_rotation, offsets::camera_rotation );
		declare_member( float, camera_fov, offsets::camera_fov );
	};

	class ft_view_target : public u_object {
	public:
		declare_member( f_minimal_view_info*, pov, offsets::pov );
	};

	class a_player_camera_manager : public a_actor {
	public:
		declare_member( ft_view_target*, view_target, offsets::view_target );
	};

	class a_controller : public a_actor { };
	class a_player_controller : public a_controller {
	public:
		declare_member( a_shooter_character*, acknowledged_pawn, offsets::acknowledged_pawn );
		declare_member( a_player_camera_manager*, player_camera_manager, offsets::player_camera_manager );
		apply_member( f_rotator, rotation_input, 0x708 + sizeof( std::uint64_t ) );


		// 	struct FRotator TargetViewRotation; // 0x538(0x18)

	};

	class u_player : public u_object {
	public:
		declare_member( a_player_controller*, player_controller, offsets::player_controller );
	};

	class u_localplayer : public u_player {
	public:
	};

	class a_game_state_base {
	public:
	};

	class u_level {
	public:
		declare_member( tarray<a_actor*>, actor_clustor, offsets::actor_clustor );
	};

	class u_game_instance {
	public:
		declare_member( tarray<u_localplayer*>, localplayers, offsets::local_players );

		auto get_localplayer( ) -> u_localplayer* {
			return this->localplayers( ).get( 0 );
		}
	};

	class u_world : public u_object {
	public:
		declare_member( a_game_state_base*, game_state, offsets::game_state );
		declare_member( u_game_instance*, owning_game_instance, offsets::owning_game_instance );
		declare_member( tarray< u_level* >, levels, offsets::levels );

		std::vector<bone_info> get_bones( std::uint64_t mesh_ptr, f_vector actor_pos ) {
			std::vector<bone_info> bones{};
			if ( !mesh_ptr )
				return bones;

			const auto bone_array_ptr = g_process->read< std::uint64_t >( mesh_ptr + offsets::bone_array );
			const auto bone_count = g_process->read< std::int32_t >( mesh_ptr + offsets::bone_count );

			if ( !bone_array_ptr || bone_count <= 0 || bone_count > 256 )
				return bones;

			f_transform component_to_world{};
			if ( !g_process->read_memory( mesh_ptr + offsets::component_to_world, &component_to_world, sizeof( component_to_world ) ) )
				return bones;

			const auto abs_value = [ ]( double v ) { return v < 0.0 ? -v : v; };
			const auto is_nan = [ ]( double v ) { return v != v; };

			double ctw_x = actor_pos.x;
			double ctw_y = actor_pos.y;
			double ctw_z = actor_pos.z;
			double qx = 0.0;
			double qy = 0.0;
			double qz = 0.0;
			double qw = 1.0;

			{
				const double tx = component_to_world.translation.x;
				const double ty = component_to_world.translation.y;
				const double tz = component_to_world.translation.z;
				const double rx = component_to_world.rotation.x;
				const double ry = component_to_world.rotation.y;
				const double rz = component_to_world.rotation.z;
				const double rw = component_to_world.rotation.w;
				const double q_mag2 = rx * rx + ry * ry + rz * rz + rw * rw;
				const bool pos_ok = abs_value( tx - actor_pos.x ) < 2000.0
					&& abs_value( ty - actor_pos.y ) < 2000.0
					&& abs_value( tz - actor_pos.z ) < 2000.0;
				const bool rot_ok = q_mag2 > 0.9 && q_mag2 < 1.1;
				const bool any_nan = is_nan( tx ) || is_nan( ty ) || is_nan( tz )
					|| is_nan( rx ) || is_nan( ry ) || is_nan( rz ) || is_nan( rw );

				if ( !any_nan && pos_ok && rot_ok ) {
					ctw_x = tx;
					ctw_y = ty;
					ctw_z = tz;
					qx = rx;
					qy = ry;
					qz = rz;
					qw = rw;
				}
			}

			std::vector<f_transform> bone_arr( static_cast< std::size_t >( bone_count ) );
			if ( !g_process->read_memory(
				bone_array_ptr,
				bone_arr.data( ),
				static_cast< std::size_t >( bone_count ) * sizeof( f_transform ) ) ) {
				return bones;
			}

			bones.reserve( static_cast< std::size_t >( bone_count ) );
			for ( std::int32_t i = 0; i < bone_count; ++i ) {
				const auto& transform = bone_arr[ static_cast< std::size_t >( i ) ];

				const double local_x = transform.translation.x;
				const double local_y = transform.translation.y;
				const double local_z = transform.translation.z;

				bool valid = !is_nan( local_x ) && !is_nan( local_y ) && !is_nan( local_z )
					&& abs_value( local_x ) < 1000.0
					&& abs_value( local_y ) < 1000.0
					&& abs_value( local_z ) < 1000.0;

				f_vector world_pos{};
				if ( valid ) {
					const double tx = 2.0 * ( qy * local_z - qz * local_y );
					const double ty = 2.0 * ( qz * local_x - qx * local_z );
					const double tz = 2.0 * ( qx * local_y - qy * local_x );

					world_pos = {
						ctw_x + local_x + qw * tx + qy * tz - qz * ty,
						ctw_y + local_y + qw * ty + qz * tx - qx * tz,
						ctw_z + local_z + qw * tz + qx * ty - qy * tx
					};

					const double dx = world_pos.x - ctw_x;
					const double dy = world_pos.y - ctw_y;
					const double dz = world_pos.z - ctw_z;
					if ( ( dx * dx + dy * dy + dz * dz ) > ( 500.0 * 500.0 ) )
						valid = false;
				}

				bone_info info{};
				info.index = i;
				info.position = world_pos;
				info.valid = valid;
				bones.push_back( info );
			}

			return bones;
		}

		f_vector get_bone_position( std::uint64_t mesh_ptr, std::int32_t bone_index, f_vector actor_pos ) {
			const auto bones = this->get_bones( mesh_ptr, actor_pos );
			if ( bone_index >= 0
				&& bone_index < static_cast< std::int32_t >( bones.size( ) )
				&& bones[ static_cast< std::size_t >( bone_index ) ].valid ) {
				return bones[ static_cast< std::size_t >( bone_index ) ].position;
			}

			return {};
		}

		std::int32_t find_bone_by_name( std::uint64_t mesh_ptr, const std::string& name_hint, f_vector actor_pos ) {
			const auto bones = this->get_bones( mesh_ptr, actor_pos );

			static const std::array<std::pair<std::string, std::int32_t>, 21> known_bones{ {
				{ "head", 6 },
				{ "neck_01", 5 },
				{ "spine_03", 4 },
				{ "spine_02", 3 },
				{ "spine_01", 2 },
				{ "pelvis", 1 },
				{ "root", 0 },
				{ "clavicle_l", 11 },
				{ "clavicle_r", 37 },
				{ "upperarm_l", 12 },
				{ "upperarm_r", 38 },
				{ "lowerarm_l", 13 },
				{ "lowerarm_r", 39 },
				{ "hand_l", 14 },
				{ "hand_r", 40 },
				{ "thigh_l", 7 },
				{ "thigh_r", 33 },
				{ "calf_l", 8 },
				{ "calf_r", 34 },
				{ "foot_l", 9 },
				{ "foot_r", 35 }
			} };

			for ( const auto& bone : known_bones ) {
				if ( bone.first != name_hint )
					continue;

				const auto bone_index = bone.second;
				if ( bone_index >= 0
					&& bone_index < static_cast< std::int32_t >( bones.size( ) )
					&& bones[ static_cast< std::size_t >( bone_index ) ].valid ) {
					return bone_index;
				}

				break;
			}

			return -1;
		}

		std::vector<bone_info> GetBones( std::uint64_t mesh_ptr, f_vector actor_pos ) {
			return this->get_bones( mesh_ptr, actor_pos );
		}

		f_vector GetBonePosition( std::uint64_t mesh_ptr, std::int32_t bone_index, f_vector actor_pos ) {
			return this->get_bone_position( mesh_ptr, bone_index, actor_pos );
		}

		std::int32_t FindBoneByName( std::uint64_t mesh_ptr, const std::string& name_hint, f_vector actor_pos ) {
			return this->find_bone_by_name( mesh_ptr, name_hint, actor_pos );
		}

		static u_world* get( ) {
			return g_process->read< u_world* >(
				g_process->read( g_process->base_address( ) + offsets::gworld )
			);
		}
	};
}