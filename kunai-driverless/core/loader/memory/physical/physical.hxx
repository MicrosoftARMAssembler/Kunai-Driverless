#pragma once
#include <set>

namespace physical {
    static constexpr std::size_t mapping_size = 0x80000000ULL;
    static constexpr std::size_t max_mappings = 128;
    static constexpr std::size_t max_pa = mapping_size * max_mappings;

    struct phys_mapping_t {
        std::uint64_t m_base_address = 0;
        std::uint64_t m_read_vad = 0;
        std::uint64_t m_write_vad = 0;
    };

    struct handle_entry_t {
        std::uint64_t m_flink = 0;
        std::uint64_t m_blink = 0;
        std::uint64_t m_list_entry = 0;
    };

    struct vad_patch_t {
        std::uint64_t m_node_va = 0;
        std::uint32_t m_orig_start = 0;
        std::uint32_t m_orig_end = 0;
        std::uint8_t  m_orig_start_h = 0;
        std::uint8_t  m_orig_end_h = 0;
        std::uint32_t m_orig_flags = 0;
        bool          m_vpn_zeroed = false;
    };

    class c_physical {
    public:
        c_physical( ) { }
        ~c_physical( ) {
            if ( m_mapping_count )
                restore( );
        }

        bool setup( ) {
            this->m_nt_open_section = g_pdb->get_symbol_address( oxorany( "NtOpenSection" ) );
            this->m_mi_locate_address = g_pdb->get_symbol_address( oxorany( "MiLocateAddress" ) );
            this->m_mm_pfn_database = g_pdb->get_symbol_address( oxorany( "MmPfnDatabase" ) );
            this->m_ke_current_thread = g_pdb->get_symbol_address( oxorany( "KeGetCurrentThread" ) );

            this->m_mmpfn_size = g_pdb->get_struct_size( oxorany( "_MMPFN" ) );
            this->m_share_count_offset = g_pdb->get_struct_member( oxorany( "_MMPFN" ), oxorany( "u2" ) );
            this->m_previous_mode_offset = g_pdb->get_struct_member( oxorany( "_KTHREAD" ), oxorany( "PreviousMode" ) );
            this->m_obj_table_offset = g_pdb->get_struct_member( oxorany( "_EPROCESS" ), oxorany( "ObjectTable" ) );
            this->m_table_list_offset = g_pdb->get_struct_member( oxorany( "_HANDLE_TABLE" ), oxorany( "HandleTableList" ) );
            this->m_table_code_offset = g_pdb->get_struct_member( oxorany( "_HANDLE_TABLE" ), oxorany( "TableCode" ) );
            this->m_starting_vpn_offset = g_pdb->get_struct_member( oxorany( "_MMVAD_SHORT" ), oxorany( "StartingVpn" ) );
            this->m_ending_vpn_offset = g_pdb->get_struct_member( oxorany( "_MMVAD_SHORT" ), oxorany( "EndingVpn" ) );
            this->m_starting_vpn_high_offset = g_pdb->get_struct_member( oxorany( "_MMVAD_SHORT" ), oxorany( "StartingVpnHigh" ) );
            this->m_ending_vpn_high_offset = g_pdb->get_struct_member( oxorany( "_MMVAD_SHORT" ), oxorany( "EndingVpnHigh" ) );
            this->m_vad_flags_offset = g_pdb->get_struct_member( oxorany( "_MMVAD_SHORT" ), oxorany( "u" ) );

            if ( !m_nt_open_section || !m_mi_locate_address ||
                !m_mm_pfn_database || !m_ke_current_thread ||
                !m_mmpfn_size || !m_previous_mode_offset ||
                !m_obj_table_offset || !m_table_list_offset ) {
                logging::print( oxorany( "could not resolve pdb symbols" ) );
                return false;
            }

            const auto ps_lookup = g_pdb->get_symbol_address( oxorany( "PsLookupProcessByProcessId" ) );
            const auto pid = static_cast< std::uint64_t >( GetCurrentProcessId( ) );
            std::uint64_t eprocess = 0;
            g_syscall->call_kernel<NTSTATUS>( ps_lookup,
                reinterpret_cast< void* >( pid ),
                &eprocess
            );

            this->m_eprocess = eprocess;
            if ( !m_eprocess )
                return false;

            return open_physical_memory( );
        }

        void unload( ) { release( ); }

        bool read( std::uint64_t pa, void* buf, std::size_t size ) const {
            if ( pa + size > max_pa ) return false;
            const auto base = m_read_views[ pa >> 31 ];
            if ( !base ) return false;
            const auto src = base + ( pa & ( mapping_size - 1 ) );
            switch ( size ) {
                case 1:  *static_cast< std::uint8_t* >( buf ) = *reinterpret_cast< const std::uint8_t* >( src ); return true;
                case 2:  *static_cast< std::uint16_t* >( buf ) = *reinterpret_cast< const std::uint16_t* >( src ); return true;
                case 4:  *static_cast< std::uint32_t* >( buf ) = *reinterpret_cast< const std::uint32_t* >( src ); return true;
                case 8:  *static_cast< std::uint64_t* >( buf ) = *reinterpret_cast< const std::uint64_t* >( src ); return true;
                default:
                    __try { std::memcpy( buf, src, size ); }
                    __except ( 1 ) { return false; }
                    return true;
            }
        }

        bool write( std::uint64_t pa, void* buf, std::size_t size ) const {
            if ( pa + size > max_pa ) return false;
            const auto base = m_write_views[ pa >> 31 ];
            if ( !base ) return false;
            const auto dst = base + ( pa & ( mapping_size - 1 ) );
            switch ( size ) {
                case 1:  *reinterpret_cast< std::uint8_t* >( dst ) = *static_cast< const std::uint8_t* >( buf ); return true;
                case 2:  *reinterpret_cast< std::uint16_t* >( dst ) = *static_cast< const std::uint16_t* >( buf ); return true;
                case 4:  *reinterpret_cast< std::uint32_t* >( dst ) = *static_cast< const std::uint32_t* >( buf ); return true;
                case 8:  *reinterpret_cast< std::uint64_t* >( dst ) = *static_cast< const std::uint64_t* >( buf ); return true;
                default:
                    __try { std::memcpy( dst, buf, size ); }
                    __except ( 1 ) { return false; }
                    return true;
            }
        }

        template<typename value_t>
        __forceinline value_t read( std::uint64_t pa ) const {
            if ( pa + sizeof( value_t ) > max_pa ) return value_t{};
            const auto base = m_read_views[ pa >> 31 ];
            if ( !base ) return value_t{};
            value_t value{};
            if constexpr ( sizeof( value_t ) <= 8 )
                value = *reinterpret_cast< const value_t* >( base + ( pa & ( mapping_size - 1 ) ) );
            else
                std::memcpy( &value, base + ( pa & ( mapping_size - 1 ) ), sizeof( value_t ) );
            return value;
        }

        template<typename value_t>
        __forceinline bool write( std::uint64_t pa, value_t value ) const {
            if ( pa + sizeof( value_t ) > max_pa ) return false;
            const auto base = m_write_views[ pa >> 31 ];
            if ( !base ) return false;
            if constexpr ( sizeof( value_t ) <= 8 )
                *reinterpret_cast< value_t* >( base + ( pa & ( mapping_size - 1 ) ) ) = value;
            else
                std::memcpy( base + ( pa & ( mapping_size - 1 ) ), &value, sizeof( value_t ) );
            return true;
        }

        HANDLE get_handle( ) const { return m_handle; }
        std::optional<handle_entry_t> hide_handle( HANDLE handle ) {
            std::uint64_t our_table_va = 0;
            memory::read_virtual( m_eprocess + m_obj_table_offset, &our_table_va, sizeof( our_table_va ) );
            if ( !our_table_va ) return std::nullopt;

            std::uint64_t table_code = 0;
            memory::read_virtual( our_table_va + m_table_code_offset, &table_code, 8 );

            const auto level = table_code & 3;
            const auto table_ptr = table_code & ~3ULL;
            const auto handle_index = reinterpret_cast< std::uint64_t >( handle ) >> 2;
            constexpr auto entry_size = 0x10ULL;

            std::uint64_t entry_pa = 0;
            if ( level == 0 ) {
                entry_pa = table_ptr + handle_index * entry_size;
            }
            else if ( level == 1 ) {
                constexpr auto entries_per_table = 256ULL;
                const auto sub_table_index = handle_index / entries_per_table;
                const auto sub_entry_index = handle_index % entries_per_table;

                std::uint64_t sub_table_ptr = 0;
                memory::read_virtual( table_ptr + sub_table_index * sizeof( std::uint64_t ),
                    &sub_table_ptr, 8 );
                sub_table_ptr &= ~3ULL;

                entry_pa = sub_table_ptr + sub_entry_index * entry_size;
            }
            else {
                logging::print( oxorany( "handle level=%llu not supported" ), level );
                return std::nullopt;
            }

            handle_entry_t entry{};
            memory::read_virtual( entry_pa, &entry.m_flink, 8 );
            memory::read_virtual( entry_pa + 8, &entry.m_blink, 8 );
            entry.m_list_entry = entry_pa;

            std::uint64_t zero = 0;
            memory::write_virtual( entry_pa, &zero, 8 );
            memory::write_virtual( entry_pa + 8, &zero, 8 );
            m_hidden_handles.emplace_back( entry );

            logging::print( oxorany( "protected handle -> 0x%llx" ), entry_pa );
            return entry;
        }

        void restore( ) {
            restore_vad_patches( );
            for ( auto idx = 0; idx < m_mapping_count; idx++ ) {
                if ( m_read_views[ idx ] ) {
                    UnmapViewOfFile( m_read_views[ idx ] );
                    m_read_views[ idx ] = nullptr;
                }
                if ( m_write_views[ idx ] ) {
                    UnmapViewOfFile( m_write_views[ idx ] );
                    m_write_views[ idx ] = nullptr;
                }
                m_mappings[ idx ].m_base_address = 0;
            }

            m_mapping_count = 0;
            m_hidden_handles.clear( );
            m_handle = nullptr;
        }

    private:
        HANDLE        m_handle = nullptr;
        std::uint64_t m_eprocess = 0;
        alignas( 64 ) std::uint8_t* m_read_views[ max_mappings ]{};
        alignas( 64 ) std::uint8_t* m_write_views[ max_mappings ]{};
        std::array<phys_mapping_t, max_mappings> m_mappings{};
        std::vector<handle_entry_t> m_hidden_handles{};
        std::vector<vad_patch_t>    m_vad_patches{};
        std::size_t   m_mapping_count = 0;

        std::uint64_t m_nt_open_section = 0;
        std::uint64_t m_mi_locate_address = 0;
        std::uint64_t m_mm_pfn_database = 0;
        std::uint64_t m_ke_current_thread = 0;

        std::size_t   m_mmpfn_size = 0;
        std::uint64_t m_share_count_offset = 0;
        std::uint64_t m_previous_mode_offset = 0;
        std::uint64_t m_obj_table_offset = 0;
        std::uint64_t m_table_list_offset = 0;
        std::uint64_t m_table_code_offset = 0;
        std::uint64_t m_starting_vpn_offset = 0;
        std::uint64_t m_ending_vpn_offset = 0;
        std::uint64_t m_starting_vpn_high_offset = 0;
        std::uint64_t m_ending_vpn_high_offset = 0;
        std::uint64_t m_vad_flags_offset = 0;

        bool open_physical_memory( ) {
            if ( !m_eprocess ) return false;

            auto kthread = g_syscall->call_kernel<std::uint64_t>( m_ke_current_thread );
            const auto prev_mode = kthread + m_previous_mode_offset;
            std::uint8_t original_mode = 1, kernel_mode = 0;
            memory::read_virtual( prev_mode, &original_mode, 1 );
            memory::write_virtual( prev_mode, &kernel_mode, 1 );

            UNICODE_STRING name{};
            RtlInitUnicodeString( &name, L"\\Device\\PhysicalMemory" );
            OBJECT_ATTRIBUTES attr{};
            InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, nullptr, nullptr );

            const auto result = g_syscall->call_kernel<NTSTATUS>(
                m_nt_open_section, &m_handle,
                reinterpret_cast< void* >( SECTION_MAP_READ | SECTION_MAP_WRITE ),
                &attr
            );
            memory::write_virtual( prev_mode, &original_mode, 1 );

            if ( result ) {
                logging::print( oxorany( "NtOpenSection failed: 0x%08X" ), result );
                return false;
            }

            for ( auto idx = 0; idx < max_mappings; idx++ ) {
                const auto base_pa = static_cast< std::uint64_t >( idx ) * mapping_size;
                const auto off_hi = static_cast< std::uint32_t >( base_pa >> 32 );
                const auto off_lo = static_cast< std::uint32_t >( base_pa & 0xFFFFFFFF );

                logging::print_line( oxorany( "creating physical mappings... %d%%" ),
                    static_cast< int >( ( idx * 100 ) / max_mappings ) );

                auto read_view = MapViewOfFile( m_handle, FILE_MAP_READ, off_hi, off_lo, mapping_size );
                auto write_view = MapViewOfFile( m_handle, FILE_MAP_WRITE, off_hi, off_lo, mapping_size );
                if ( !read_view || !write_view ) break;

                m_read_views[ idx ] = static_cast< std::uint8_t* >( read_view );
                m_write_views[ idx ] = static_cast< std::uint8_t* >( write_view );
                m_mappings[ idx ].m_base_address = base_pa;
                m_mapping_count++;
            }

            logging::print_line( oxorany( "creating physical mappings... done\n" ) );

            if ( !hide_physical_mappings( ) )
                return false;

            return m_mapping_count > 0;
        }

        bool hide_physical_mappings( ) {
            std::set<std::uint64_t> nodes_seen;
            std::uint32_t hidden = 0;

            for ( auto idx = 0; idx < m_mapping_count; idx++ ) {
                const auto pct = static_cast< int >( ( idx * 100 ) / m_mapping_count );
                logging::print_line( oxorany( "hiding physical mappings... %d%%" ), pct );

                for ( auto view : { m_read_views[ idx ], m_write_views[ idx ] } ) {
                    const auto node_va = g_syscall->call_kernel<std::uint64_t>(
                        m_mi_locate_address,
                        reinterpret_cast< void* >( view )
                    );
                    if ( !node_va || node_va < 0xFFFF000000000000ULL ) continue;
                    if ( !nodes_seen.insert( node_va ).second ) continue;

                    std::uint64_t left = 0, right = 0, parent = 0;
                    memory::read_virtual( node_va + 0x00, &left, 8 );
                    memory::read_virtual( node_va + 0x08, &right, 8 );
                    memory::read_virtual( node_va + 0x10, &parent, 8 );

                    auto is_valid = [ ]( std::uint64_t p ) {
                        return p == 0 || p >= 0xFFFF000000000000ULL;
                        };
                    if ( !is_valid( left & ~7ULL ) || !is_valid( right & ~7ULL ) || !is_valid( parent & ~3ULL ) ) {
                        logging::print( oxorany( "corrupt node -> 0x%llx, skipping" ), node_va );
                        continue;
                    }

                    std::uint32_t flags = 0;
                    memory::read_virtual( node_va + m_vad_flags_offset, &flags, 4 );

                    std::uint32_t secure_flags = 0;
                    memory::read_virtual( node_va + 0x30, &secure_flags, 4 );
                    const bool is_secured = ( secure_flags & 4u ) != 0;

                    if ( is_secured )
                        logging::print( oxorany( "skipping secured node -> 0x%llx" ), node_va );

                    vad_patch_t patch{};
                    patch.m_node_va = node_va;
                    patch.m_orig_flags = flags;
                    memory::read_virtual( node_va + m_starting_vpn_offset, &patch.m_orig_start, 4 );
                    memory::read_virtual( node_va + m_ending_vpn_offset, &patch.m_orig_end, 4 );
                    memory::read_virtual( node_va + m_starting_vpn_high_offset, &patch.m_orig_start_h, 1 );
                    memory::read_virtual( node_va + m_ending_vpn_high_offset, &patch.m_orig_end_h, 1 );
                    patch.m_vpn_zeroed = !is_secured;

                    flags |= ( 1u << 20 );
                    flags &= ~( 1u << 25 );
                    memory::write_virtual( node_va + m_vad_flags_offset, &flags, 4 );

                    if ( !is_secured ) {
                        std::uint32_t fake_vpn = hidden + 1;
                        std::uint8_t  zero8 = 0;
                        memory::write_virtual( node_va + m_starting_vpn_offset, &fake_vpn, 4 );
                        memory::write_virtual( node_va + m_ending_vpn_offset, &fake_vpn, 4 );
                        memory::write_virtual( node_va + m_starting_vpn_high_offset, &zero8, 1 );
                        memory::write_virtual( node_va + m_ending_vpn_high_offset, &zero8, 1 );
                    }

                    m_vad_patches.push_back( patch );
                    hidden++;
                }
            }

            logging::print_line( oxorany( "hiding physical mappings... done\n" ) );
            return true;
        }

        void restore_vad_patches( ) {
            if ( m_vad_patches.empty( ) ) return;

            for ( auto& patch : m_vad_patches ) {
                memory::write_virtual( patch.m_node_va + m_vad_flags_offset, &patch.m_orig_flags, 4 );
                memory::write_virtual( patch.m_node_va + m_starting_vpn_offset, &patch.m_orig_start, 4 );
                memory::write_virtual( patch.m_node_va + m_ending_vpn_offset, &patch.m_orig_end, 4 );
                memory::write_virtual( patch.m_node_va + m_starting_vpn_high_offset, &patch.m_orig_start_h, 1 );
                memory::write_virtual( patch.m_node_va + m_ending_vpn_high_offset, &patch.m_orig_end_h, 1 );
            }

            m_vad_patches.clear( );
        }

        void release( ) {
            if ( !m_mapping_count ) return;

            restore_vad_patches( );

            for ( auto idx = 0; idx < m_mapping_count; idx++ ) {
                if ( m_read_views[ idx ] ) {
                    UnmapViewOfFile( m_read_views[ idx ] );
                    m_read_views[ idx ] = nullptr;
                }
                if ( m_write_views[ idx ] ) {
                    UnmapViewOfFile( m_write_views[ idx ] );
                    m_write_views[ idx ] = nullptr;
                }
                m_mappings[ idx ].m_base_address = 0;
            }
            m_mapping_count = 0;

            restore_handles( );
            if ( m_handle ) {
                CloseHandle( m_handle );
                m_handle = nullptr;
            }
        }

        void restore_handles( ) {
            for ( auto& entry : m_hidden_handles ) {
                memory::write_virtual( entry.m_list_entry, &entry.m_flink, 8 );
                memory::write_virtual( entry.m_list_entry + 8, &entry.m_blink, 8 );
            }
            m_hidden_handles.clear( );
        }
    };
}