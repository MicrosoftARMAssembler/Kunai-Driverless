#pragma once 

namespace memory {
    bool read_physical( std::uint64_t addr, void* buffer, std::size_t size );
    bool write_physical( std::uint64_t addr, void* buffer, std::size_t size );
}

namespace paging {
    constexpr auto page_4kb_size = 0x1000ull;
    constexpr auto page_2mb_size = 0x200000ull;
    constexpr auto page_1gb_size = 0x40000000ull;

    constexpr auto page_shift = 12ull;
    constexpr auto page_2mb_shift = 21ull;
    constexpr auto page_1gb_shift = 30ull;

    constexpr auto page_4kb_mask = 0xFFFull;
    constexpr auto page_2mb_mask = 0x1FFFFFull;
    constexpr auto page_1gb_mask = 0x3FFFFFFFull;

    struct pt_entries_t {
        std::optional<pml4e> m_pml4; std::uint64_t m_pml4_pa{};
        std::optional<pdpte> m_pdpt; std::uint64_t m_pdpt_pa{};
        std::optional<pde>   m_pd; std::uint64_t m_pd_pa{};
        std::optional<pte>   m_pt; std::uint64_t m_pt_pa{};
        std::uint64_t m_phys_addr{};
        std::uint64_t m_page_size{};
    };

	class c_paging {
        std::uint64_t m_directory_table_base{};

	public:
        c_paging( ) { }
        ~c_paging( ) { }

        bool setup( ) {
            auto page_buffer = std::make_unique<uint8_t[ ]>( 0x1000 );
            for ( uint64_t page = 0; page < 0x20000000; page += 0x1000 ) {
                if ( !memory::read_physical( page, page_buffer.get( ), 0x1000 ) )
                    continue;

                if ( page_buffer[ 0 ] == 0xE9 && page_buffer[ 1 ] == 0x4D &&
                    page_buffer[ 2 ] == 0x06 && page_buffer[ 3 ] == 0x00 ) {
                    auto dtb = *reinterpret_cast< uint64_t* >( page_buffer.get( ) + 0xA0 );
                    if ( dtb && ( dtb & 0xFFF ) == 0 ) {
                        logging::print( oxorany( "found directory table base: 0x%llx" ), dtb );
                        this->m_directory_table_base = dtb;
                        return true;
                    }
                }
            }

            return false;
        }

        std::uint64_t swap_dtb( std::uint64_t new_dtb ) {
            const auto saved_dtb = this->m_directory_table_base;
            this->m_directory_table_base = new_dtb;
            return saved_dtb;
        }

        std::uint64_t find_dtb( std::uint64_t module_base, std::uint64_t second_va = 0 ) {
            MEMORYSTATUSEX mem_status{};
            mem_status.dwLength = sizeof( mem_status );
            GlobalMemoryStatusEx( &mem_status );

            const auto total_pages = mem_status.ullTotalPhys >> 12;
            const auto max_threads = max( 1u, std::thread::hardware_concurrency( ) );
            const auto pages_per_thread = total_pages / max_threads;

            virt_addr_t virt_base{ .value = module_base };

            std::atomic<std::uint64_t> target_cr3{ 0 };
            std::atomic<bool> found{ false };
            std::vector<std::thread> threads;
            threads.reserve( max_threads );

            const auto t_start = std::chrono::high_resolution_clock::now( );
            for ( auto idx = 0u; idx < max_threads; ++idx ) {
                const auto start = idx * pages_per_thread;
                const auto end = ( idx == max_threads - 1 )
                    ? total_pages : ( idx + 1 ) * pages_per_thread;

                threads.emplace_back( [ =, &target_cr3, &found, &virt_base ] {
                    this->scan_cr3_worker( start, end, virt_base, second_va, total_pages, target_cr3, found );
                    } );
            }

            for ( auto& t : threads )
                t.join( );

            return target_cr3.load( );
        }

        template<typename addr_t, typename callback_t>
        bool scan_va(
            addr_t va, std::size_t size,
            callback_t callback ) {
            std::uint64_t va64;
            const auto aligned_size = ( size + page_4kb_mask ) & ~page_4kb_mask;
            const auto page_count = aligned_size >> page_shift;
            for ( auto idx = 0; idx < page_count; idx++ ) {
                const auto current_va = va + ( idx << page_shift );
                auto ctx = this->hyperspace_entries( current_va );
                if ( !ctx ) continue;

                if ( !callback( current_va, *ctx ) )
                    return false;
            }

            return true;
        }

        std::uint64_t translate_linear(
            std::uint64_t virt_addr,
            std::uint32_t* page_size = nullptr ) {
            auto ctx = hyperspace_entries( virt_addr );
            if ( !ctx )
                return 0;

            if ( page_size )
                *page_size = ctx->m_page_size;

            return ctx->m_phys_addr;
        }

        std::uint64_t get_pte_address(
            std::uint64_t virt_addr,
            std::uint32_t* page_size = nullptr ) {
            auto ctx = hyperspace_entries( virt_addr );
            if ( !ctx )
                return 0;

            if ( page_size )
                *page_size = ctx->m_page_size;

            return ctx->m_pt_pa;
        }

        std::optional<pt_entries_t> hyperspace_entries(
            std::uint64_t virt_addr,
            std::function<bool( pt_entries_t& )> on_pml4 = nullptr,
            std::function<bool( pt_entries_t& )> on_pdpt = nullptr,
            std::function<bool( pt_entries_t& )> on_pd = nullptr,
            std::function<bool( pt_entries_t& )> on_pt = nullptr ) {

            virt_addr_t va{ virt_addr };
            pt_entries_t ctx{};

            ctx.m_pml4_pa = m_directory_table_base + ( va.pml4e_index * sizeof( pml4e ) );
            auto pml4 = read_pt_entry<pml4e>( ctx.m_pml4_pa );
            if ( !pml4 || !pml4->hard.present ) return std::nullopt;
            ctx.m_pml4 = pml4;
            if ( on_pml4 && !on_pml4( ctx ) ) return std::nullopt;

            ctx.m_pdpt_pa = ( ctx.m_pml4->hard.pfn << 12 ) + ( va.pdpte_index * sizeof( pdpte ) );
            auto pdpt = read_pt_entry<pdpte>( ctx.m_pdpt_pa );
            if ( !pdpt || !pdpt->hard.present ) return std::nullopt;
            ctx.m_pdpt = pdpt;

            if ( on_pdpt && !on_pdpt( ctx ) ) return std::nullopt;
            if ( ctx.m_pdpt->hard.page_size ) {
                ctx.m_page_size = page_1gb_size;
                ctx.m_phys_addr = ( ctx.m_pdpt->hard.pfn << 12 ) + ( virt_addr & page_1gb_mask );
                return ctx;
            }

            ctx.m_pd_pa = ( ctx.m_pdpt->hard.pfn << 12 ) + ( va.pde_index * sizeof( pde ) );
            auto pd = read_pt_entry<pde>( ctx.m_pd_pa );
            if ( !pd || !pd->hard.present ) return std::nullopt;
            ctx.m_pd = pd;

            if ( on_pd && !on_pd( ctx ) ) return std::nullopt;
            if ( ctx.m_pd->hard.page_size ) {
                ctx.m_page_size = page_2mb_size;
                ctx.m_phys_addr = ( ctx.m_pd->hard.pfn << 12 ) + ( virt_addr & page_2mb_mask );
                return ctx;
            }

            ctx.m_pt_pa = ( ctx.m_pd->hard.pfn << page_shift ) + ( va.pte_index * sizeof( pte ) );
            auto pt = read_pt_entry<pte>( ctx.m_pt_pa );
            if ( !pt || !pt->hard.present ) return std::nullopt;
            ctx.m_pt = pt;

            if ( on_pt && !on_pt( ctx ) ) return std::nullopt;
            ctx.m_page_size = page_4kb_size;
            ctx.m_phys_addr = ( ctx.m_pt->hard.pfn << 12 ) + ( virt_addr & page_4kb_mask );
            return ctx;
        }

        template<typename type>
        std::optional<type> read_pt_entry( std::uint64_t phys_addr ) {
            type entry{};
            if ( !memory::read_physical( phys_addr, &entry, sizeof( type ) ) )
                return std::nullopt;
            return entry;
        }
        
  private:
        void scan_cr3_worker(
            const std::uint64_t start,
            const std::uint64_t end,
            const virt_addr_t& va,
            const std::uint64_t second_va,
            const std::uint64_t total_pages,
            std::atomic<std::uint64_t>& found_cr3,
            std::atomic<bool>& found ) {

            if ( found.load( std::memory_order_acquire ) )
                return;

            const auto count = end - start;
            for ( auto idx = 0ull; idx < count; ++idx ) {
                if ( found.load( std::memory_order_acquire ) )
                    return;

                const auto cur_pa = ( start + idx ) << 12;
                if ( !cur_pa )
                    continue;

                auto pml4 = read_pt_entry<pml4e>(
                    cur_pa + ( va.pml4e_index * sizeof( ::pml4e ) ) );
                if ( !pml4 || !pml4->hard.present )
                    continue;
                if ( !pml4->hard.pfn || pml4->hard.pfn >= total_pages )
                    continue;

                auto pdpte_pa = pml4->hard.pfn << 12;
                if ( !utility::is_physical_address_valid( pdpte_pa, sizeof( ::pdpte ) ) )
                    continue;

                auto pdpt = read_pt_entry<pdpte>(
                    pdpte_pa + ( va.pdpte_index * sizeof( ::pdpte ) ) );
                if ( !pdpt || !pdpt->hard.present )
                    continue;
                if ( !pdpt->hard.pfn || pdpt->hard.pfn >= total_pages )
                    continue;

                auto pde_pa = pdpt->hard.pfn << 12;
                if ( !utility::is_physical_address_valid( pde_pa, sizeof( ::pde ) ) )
                    continue;

                auto pd = read_pt_entry<pde>(
                    pde_pa + ( va.pde_index * sizeof( ::pde ) ) );
                if ( !pd || !pd->hard.present )
                    continue;
                if ( !pd->hard.pfn || pd->hard.pfn >= total_pages )
                    continue;

                auto pte_pa = pd->hard.pfn << 12;
                if ( !utility::is_physical_address_valid( pte_pa, sizeof( ::pte ) ) )
                    continue;

                auto pt = read_pt_entry<pte>(
                    pte_pa + ( va.pte_index * sizeof( ::pte ) ) );
                if ( !pt || !pt->hard.present )
                    continue;
                if ( !pt->hard.pfn || pt->hard.pfn >= total_pages )
                    continue;

                if ( second_va ) {
                    virt_addr_t va2{ .value = second_va };

                    auto pml4_2 = read_pt_entry<pml4e>(
                        cur_pa + ( va2.pml4e_index * sizeof( ::pml4e ) ) );
                    if ( !pml4_2 || !pml4_2->hard.present )
                        continue;

                    auto pdpt_2 = read_pt_entry<pdpte>(
                        ( pml4_2->hard.pfn << 12 ) + ( va2.pdpte_index * sizeof( ::pdpte ) ) );
                    if ( !pdpt_2 || !pdpt_2->hard.present )
                        continue;

                    if ( !pdpt_2->hard.page_size ) {
                        auto pd_2 = read_pt_entry<pde>(
                            ( pdpt_2->hard.pfn << 12 ) + ( va2.pde_index * sizeof( ::pde ) ) );
                        if ( !pd_2 || !pd_2->hard.present )
                            continue;

                        if ( !pd_2->hard.page_size ) {
                            auto pt_2 = read_pt_entry<pte>(
                                ( pd_2->hard.pfn << 12 ) + ( va2.pte_index * sizeof( ::pte ) ) );
                            if ( !pt_2 || !pt_2->hard.present )
                                continue;
                        }
                    }
                }

                std::uint64_t expected = 0;
                if ( found_cr3.compare_exchange_strong( expected, cur_pa,
                    std::memory_order_release, std::memory_order_relaxed ) ) {
                    found.store( true, std::memory_order_release );
                }

                return;
            }
        }
	};
}