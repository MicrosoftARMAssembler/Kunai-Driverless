#pragma once

namespace driver {

#pragma pack(push, 1)
    struct map_physical_input_t {
        std::uint64_t physical_address;
        std::uint64_t size;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct unmap_physical_input_t {
        std::uint64_t mapped_memory;
    };
#pragma pack(pop)


#pragma section(".text")
    __declspec( allocate( ".text" ) )
        const std::uint8_t syscall_stub[ ]{
            0x4C, 0x8B, 0x11,
            0x8B, 0x41, 0x08,
            0x0F, 0x05,
            0xC3
    };

    template <typename ret_type = void, typename first = void*, typename... args>
    ret_type syscall( DWORD index, first arg = first{}, args... NextArgs ) {
        struct data_struct {
            first arg1;
            DWORD syscall_id;
        } data{ arg, index };

        using call_stub = ret_type( __fastcall* )( data_struct*, args... );
        return ( ( call_stub )&syscall_stub[ 0 ] )( &data, NextArgs... );
    }

    class c_driver {

        NTSTATUS send_cmd(
            uint32_t ioctl_code,
            void* input_buffer,
            uint32_t input_size,
            void* output_buffer,
            uint32_t output_size
        ) const {
            if ( m_driver_handle == INVALID_HANDLE_VALUE ) {
                logging::print( "could not open driver handle." );
                return STATUS_INVALID_HANDLE;
            }

            IO_STATUS_BLOCK block{};
            return driver::syscall<NTSTATUS>( 7,
                this->m_driver_handle,
                nullptr,
                nullptr,
                nullptr,
                &block,
                ioctl_code,
                input_buffer,
                input_size,
                output_buffer,
                output_size
            );
        }

    public:

        bool initialize( ) {
            m_driver_handle = CreateFileW(
                oxorany( L"\\\\.\\EBIoDispatch" ),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if ( !m_driver_handle || m_driver_handle == INVALID_HANDLE_VALUE ) {
                logging::print( "could not open driver handle." );
                return false;
            }

            logging::print( "driver handle opened successfully.\n" );
            return true;
        }

        void unload( ) {
            if ( m_driver_handle != INVALID_HANDLE_VALUE ) {
                CloseHandle( m_driver_handle );
                m_driver_handle = INVALID_HANDLE_VALUE;
            }
        }

        HANDLE get_handle( ) const {
            return m_driver_handle;
        }

        void close( ) {
            m_driver_handle = INVALID_HANDLE_VALUE;
        }

        void* resolve_mapped_address( uint32_t TruncatedAddress ) const
        {
            for ( uint64_t High = 0; High < 0x800; High++ ) {
                uintptr_t TestAddr = ( High << 32 ) | static_cast< uint64_t >( TruncatedAddress );
                MEMORY_BASIC_INFORMATION Mbi = {};
                if ( VirtualQuery( reinterpret_cast< void* >( TestAddr ), &Mbi, sizeof( Mbi ) ) > 0 ) {
                    if ( Mbi.State == MEM_COMMIT &&
                        reinterpret_cast< uintptr_t >( Mbi.BaseAddress ) == TestAddr ) {
                        return reinterpret_cast< void* >( TestAddr );
                    }
                }
            }
            return nullptr;
        }

        void* map_physical_memory( uint64_t physical_address, uint32_t size ) {
            map_physical_input_t map_physical_input;
            map_physical_input.physical_address = physical_address;
            map_physical_input.size = size;

            std::uint32_t mapped_memory;
            auto status = send_cmd( 
                0x0022E008,
                &map_physical_input, sizeof( map_physical_input ),
                &mapped_memory, sizeof( mapped_memory ) );
            if ( status )
                return nullptr;

            return resolve_mapped_address( mapped_memory );
        }

        void unmap_physical_memory( void* mapped_memory ) {
            unmap_physical_input_t unmap_physical_input;
            unmap_physical_input.mapped_memory = ( std::uint64_t )mapped_memory;

            send_cmd( 0x0022E00C, &unmap_physical_input, sizeof( unmap_physical_input ), nullptr, 0 );
        }

        bool read_physical_memory( uint64_t pa, void* buffer, size_t size ) {
            auto* dst = static_cast< uint8_t* >( buffer );
            size_t offset = 0;
            while ( offset < size ) {
                uint64_t cur_pa = pa + offset;
                uint64_t page_base = cur_pa & ~0xFFFULL;
                uint32_t page_off = static_cast< uint32_t >( cur_pa & 0xFFF );
                size_t chunk = min( size - offset, 0x1000 - page_off );

                void* view = map_physical_memory( page_base, 0x1000 );
                if ( view ) {
                    memcpy( dst + offset, ( uint8_t* )view + page_off, chunk );
                    unmap_physical_memory( view );
                }

                offset += chunk;
            }
            return true;
        }

        bool write_physical_memory( uint64_t pa, const void* buffer, size_t size ) {
            const auto* src = static_cast< const uint8_t* >( buffer );
            size_t offset = 0;
            while ( offset < size ) {
                uint64_t cur_pa = pa + offset;
                uint64_t page_base = cur_pa & ~0xFFFULL;
                uint32_t page_off = static_cast< uint32_t >( cur_pa & 0xFFF );
                size_t   chunk = min( size - offset, 0x1000 - page_off );

                void* view = map_physical_memory( page_base, 0x1000 );
                if ( !view ) {
                    logging::print( "map failed — PA=0x%llx offset=0x%llx", pa, offset );
                    return false;
                }

                memcpy( static_cast< uint8_t* >( view ) + page_off, src + offset, chunk );
                unmap_physical_memory( view );
                offset += chunk;
            }
            return true;
        }

    private:
        HANDLE m_driver_handle = INVALID_HANDLE_VALUE;
    };

} // namespace driver