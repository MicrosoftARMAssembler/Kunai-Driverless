#pragma once

namespace process {
    struct batch_read_t {
        std::uint64_t va;
        void* buf;
        std::size_t   size;
        bool          success;
    };

    struct pt_cache_entry_t {
        std::uint64_t phys_addr = 0;
        std::uint64_t raw = 0;
    };

    static constexpr std::size_t cache_size = 512;
    static constexpr std::uint64_t cache_mask = cache_size - 1; // 0x1FF

	class c_process {
        alignas( 64 ) pt_cache_entry_t m_pml4_cache[ 512 ]{};
        alignas( 64 ) pt_cache_entry_t m_pdpt_cache[ 512 ]{};
        alignas( 64 ) pt_cache_entry_t m_pd_cache[ 512 ]{};
        alignas( 64 ) pt_cache_entry_t m_pt_cache[ 512 ]{};

	public:
        c_process( std::wstring name ) : m_name( name ) {
            std::memset( m_pml4_cache, 0, sizeof( m_pml4_cache ) );
            std::memset( m_pdpt_cache, 0, sizeof( m_pdpt_cache ) );
            std::memset( m_pd_cache, 0, sizeof( m_pd_cache ) );
            std::memset( m_pt_cache, 0, sizeof( m_pt_cache ) );
        }
        c_process( std::uint32_t pid ) : m_pid( pid ) {
            std::memset( m_pml4_cache, 0, sizeof( m_pml4_cache ) );
            std::memset( m_pdpt_cache, 0, sizeof( m_pdpt_cache ) );
            std::memset( m_pd_cache, 0, sizeof( m_pd_cache ) );
            std::memset( m_pt_cache, 0, sizeof( m_pt_cache ) );
        }
		c_process ( ) { }

		bool open( ) {
            this->m_pid = utility::find_pid( m_name );
            if ( !m_pid ) {
                logging::print( oxorany( "could not find process name" ) );
                return false;
            }

            this->m_eprocess = find_eprocess( m_pid );
            if ( !m_eprocess ) {
                logging::print( oxorany( "could not find eprocess" ) );
                return false;
            }

            this->m_peb = find_peb( m_eprocess );
            if ( !m_peb ) {
                logging::print( oxorany( "could not find peb" ) );
                return false;
            }

            this->m_base_address = find_base_address( m_eprocess );
            if ( !m_base_address ) {
                logging::print( oxorany( "could not find base address" ) );
                return false;
            }

            this->m_dtb = find_dtb( m_eprocess );
            if ( !m_dtb ) {
                logging::print( oxorany( "could not find directory table base" ) );
                return false;
            }

            return true;
		}

        bool enable_ppl( ) {
            const auto protection_offset = g_pdb->get_struct_member(
                oxorany( "_EPROCESS" ), oxorany( "Protection" )
            );
            if ( !protection_offset ) {
                return false;
            }

            const auto eprocess = find_eprocess( m_pid );
            if ( !eprocess ) {
                logging::print( oxorany( "could not find own EPROCESS" ) );
                return false;
            }

            ps_protection_t target{};
            target.type = ps_protected_light;
            target.audit = 0;
            target.signer = ps_signer_antimalware;

            if ( !memory::write_virtual( eprocess + protection_offset, &target.level, 1 ) ) {
                logging::print( oxorany( "failed to write Protection field" ) );
                return false;
            }

            ps_protection_t prot{};
            DWORD ret_len = 0;
            auto result = NtQueryInformationProcess(
                GetCurrentProcess( ),
                static_cast< PROCESSINFOCLASS >( 61 ),
                &prot,
                sizeof( prot ),
                &ret_len
            );

            if ( NT_SUCCESS( result ) && prot.level == target.level ) {
                logging::print( oxorany( "protected process -> 0x%llx (pid: %u)" ), eprocess, m_pid );
                return true;
            }

            logging::print( oxorany( "could not protect process." ) );
            return false;
        }

        std::uint32_t pid( )  const { return m_pid; }
        std::uint64_t eprocess( )  const { return m_eprocess; }
        std::uint64_t peb( )  const { return m_peb; }
        std::uint64_t base_address( )  const { return m_base_address; }
        std::uint64_t dtb( )  const { return m_dtb; }

        template <typename ret_t = std::uint64_t, typename addr_t>
        ret_t read( addr_t va ) {
            std::uint64_t va64;
            if constexpr ( std::is_pointer_v<addr_t> )
                va64 = reinterpret_cast< std::uint64_t >( va );
            else if constexpr ( std::is_integral_v<addr_t> )
                va64 = static_cast< std::uint64_t >( va );
            else
                static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
                    "addr_t must be pointer or integral" );
            ret_t ret{};
            read_memory( va64, &ret, sizeof( ret ) );
            return ret;
        }

        template <typename val_t, typename addr_t>
        bool write( addr_t va, val_t val ) {
            std::uint64_t va64;
            if constexpr ( std::is_pointer_v<addr_t> )
                va64 = reinterpret_cast< std::uint64_t >( va );
            else if constexpr ( std::is_integral_v<addr_t> )
                va64 = static_cast< std::uint64_t >( va );
            else
                static_assert( std::is_pointer_v<addr_t> || std::is_integral_v<addr_t>,
                    "addr_t must be pointer or integral" );

            return write_memory( va64, &val, sizeof( val_t ) );
        }

        bool read_memory( std::uint64_t va, void* buf, std::size_t size ) {
            auto phys = this->translate_linear( va );
            if ( !phys ) return false;
            return memory::read_physical( phys, buf, size );
        }

        bool write_memory( std::uint64_t va, void* buf, std::size_t size ) {
            auto phys = this->translate_linear( va );
            if ( !phys ) return false;
            return memory::write_physical( phys, buf, size );
        }

        bool read_memory_batch( batch_read_t* requests, std::size_t count ) {
            for ( auto idx = 0; idx < count; ++idx ) {
                auto& req = requests[ idx ];
                auto phys = this->translate_linear( req.va );
                if ( !phys ) { req.success = false; continue; }
                req.success = memory::read_physical( phys, req.buf, req.size );
            }
            return true;
        }

        void benchmark_rps( ) {
            auto read_buffer = 0ull;
            auto ops = 0ull;
            auto fails = 0ull;

            for ( auto i = 0; i < 10000; i++ )
                this->read_memory( this->m_base_address + 0x1000, &read_buffer,
                    sizeof( std::uint64_t ) );

            auto start = std::chrono::steady_clock::now( );
            auto deadline = start + std::chrono::seconds( 1 );

            while ( std::chrono::steady_clock::now( ) < deadline ) {
                for ( auto i = 0; i < 1000; i++ ) {
                    if ( this->read_memory( this->m_base_address + 0x1000, &read_buffer,
                        sizeof( std::uint64_t ) ) )
                        ++ops;
                    else
                        ++fails;
                }
            }

            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now( ) - start ).count( );
            auto rps = static_cast< std::uint64_t >(
                static_cast< double >( ops ) / elapsed );

            logging::print( oxorany( "%s reads/sec, with %s fails, in %.fs" ),
                utility::add_commas( rps ).c_str( ),
                utility::add_commas( fails ).c_str( ),
                elapsed );

            const auto magic = this->read<std::uint16_t>( this->m_base_address );
            logging::print( oxorany( "pe header -> addr=0x%llx, magic=0x%04X" ),
                this->m_base_address, magic );
        }

        std::uint64_t translate_linear( std::uint64_t virt_addr, std::uint32_t* page_size = nullptr ) {
            virt_addr_t va{ .value = virt_addr };

            std::uint64_t pdpt_base = 0;
            const auto pml4_pa = m_dtb + ( va.pml4e_index * sizeof( pml4e ) );
            const auto pml4_slot = va.pml4e_index & cache_mask;
            if ( m_pml4_cache[ pml4_slot ].phys_addr == pml4_pa ) {
                pdpt_base = m_pml4_cache[ pml4_slot ].raw;
            }
            else {
                auto entry = g_paging->read_pt_entry<pml4e>( pml4_pa );
                if ( !entry || !entry->hard.present ) return 0;
                pdpt_base = entry->hard.pfn << 12;
                m_pml4_cache[ pml4_slot ] = { pml4_pa, pdpt_base };
            }

            const auto pdpt_entry_pa = pdpt_base + ( va.pdpte_index * sizeof( pdpte ) );
            const auto pdpt_slot = ( pdpt_entry_pa >> 3 ) & cache_mask;
            std::uint64_t pdpt_child_pa;
            if ( m_pdpt_cache[ pdpt_slot ].phys_addr == pdpt_entry_pa ) {
                pdpt_child_pa = m_pdpt_cache[ pdpt_slot ].raw;
            }
            else {
                auto entry = g_paging->read_pt_entry<pdpte>( pdpt_entry_pa );
                if ( !entry || !entry->hard.present ) return 0;
                if ( entry->hard.page_size ) {
                    if ( page_size ) *page_size = paging::page_1gb_size;
                    return ( entry->hard.pfn << 12 ) + ( virt_addr & paging::page_1gb_mask );
                }
                pdpt_child_pa = entry->hard.pfn << 12;
                m_pdpt_cache[ pdpt_slot ] = { pdpt_entry_pa, pdpt_child_pa };
            }

            const auto pd_entry_pa = pdpt_child_pa + ( va.pde_index * sizeof( pde ) );
            const auto pd_slot = ( pd_entry_pa >> 3 ) & cache_mask;
            std::uint64_t pd_child_pa;
            if ( m_pd_cache[ pd_slot ].phys_addr == pd_entry_pa ) {
                pd_child_pa = m_pd_cache[ pd_slot ].raw;
            }
            else {
                auto entry = g_paging->read_pt_entry<pde>( pd_entry_pa );
                if ( !entry || !entry->hard.present ) return 0;
                if ( entry->hard.page_size ) {
                    if ( page_size ) *page_size = paging::page_2mb_size;
                    return ( entry->hard.pfn << 12 ) + ( virt_addr & paging::page_2mb_mask );
                }
                pd_child_pa = entry->hard.pfn << 12;
                m_pd_cache[ pd_slot ] = { pd_entry_pa, pd_child_pa };
            }

            const auto pt_entry_pa = pd_child_pa + ( va.pte_index * sizeof( pte ) );
            const auto pt_slot = ( pt_entry_pa >> 3 ) & cache_mask;
            std::uint64_t page_pa;
            if ( m_pt_cache[ pt_slot ].phys_addr == pt_entry_pa ) {
                page_pa = m_pt_cache[ pt_slot ].raw;
            }
            else {
                auto entry = g_paging->read_pt_entry<pte>( pt_entry_pa );
                if ( !entry || !entry->hard.present ) return 0;
                page_pa = entry->hard.pfn << 12;
                m_pt_cache[ pt_slot ] = { pt_entry_pa, page_pa };
            }

            if ( page_size ) *page_size = paging::page_4kb_size;
            return page_pa + ( virt_addr & paging::page_4kb_mask );
        }

	private:
		std::wstring  m_name;
		std::uint32_t m_pid;
        std::uint64_t m_eprocess;
        std::uint64_t m_peb;
        std::uint64_t m_base_address;
        std::uint64_t m_dtb;

        bool is_hvci_enabled( ) {
            auto mi_flags = g_pdb->get_symbol_address(
                oxorany( "MiFlags" )
            );

            std::uint64_t value = 0;
            memory::read_virtual( mi_flags, &value, sizeof( value ) );
            return ( value & 0x10000 ) != 0;
        }

        std::uint64_t find_eprocess( std::uint32_t target_pid ) const {
            auto list_head = g_pdb->get_symbol_address(
                oxorany( "PsActiveProcessHead" )
            );
            const auto links_offset = g_pdb->get_struct_member(
                oxorany( "_EPROCESS" ), oxorany( "ActiveProcessLinks" )
            );
            const auto pid_offset = g_pdb->get_struct_member(
                oxorany( "_EPROCESS" ), oxorany( "UniqueProcessId" )
            );

            std::uint64_t current = 0;
            if ( !memory::read_virtual( list_head, &current, sizeof( current ) ) )
                return 0;

            while ( current && current != list_head ) {
                const auto eprocess = current - links_offset;

                std::uint64_t cur_pid = 0;
                if ( !memory::read_virtual( eprocess + pid_offset, &cur_pid, sizeof( cur_pid ) ) )
                    break;

                if ( static_cast< std::uint32_t >( cur_pid ) == target_pid )
                    return eprocess;

                if ( !memory::read_virtual( current, &current, sizeof( current ) ) )
                    break;
            }

            return 0;
        }

        std::uint64_t find_peb( std::uint64_t eprocess ) const {
            const auto peb_offset = g_pdb->get_struct_member(
                oxorany( "_EPROCESS" ), oxorany( "Peb" )
            );
            std::uint64_t peb = 0;
            memory::read_virtual( eprocess + peb_offset, &peb, sizeof( peb ) );
            return peb;
        }

        std::uint64_t find_base_address( std::uint64_t eprocess ) const {
            const auto section_base_offset = g_pdb->get_struct_member(
                oxorany( "_EPROCESS" ), oxorany( "SectionBaseAddress" )
            );
            std::uint64_t base = 0;
            memory::read_virtual( eprocess + section_base_offset, &base, sizeof( base ) );
            return base;
        }

        std::uint64_t find_dtb( std::uint64_t eprocess ) const {
            const auto dtb_offset = g_pdb->get_struct_member(
                oxorany( "_KPROCESS" ), oxorany( "DirectoryTableBase" )
            );
            std::uint64_t dtb = 0;
            memory::read_virtual( eprocess + dtb_offset, &dtb, sizeof( dtb ) );
            return dtb & ~0xFull;
        }
	};
}