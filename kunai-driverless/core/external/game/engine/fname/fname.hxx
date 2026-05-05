#pragma once

namespace game {
    namespace helper {
        std::uint64_t get_xor_key( ) {
            constexpr const char* plaintext = "ByteProperty";
            constexpr uint16_t len = 12;
            constexpr int32_t index = 3;

            const uint32_t block = static_cast< uint32_t >( index >> 16 );
            const uint32_t offset = index & 0xFFFF;

            const uint64_t data_ptr = g_process->read<uint64_t>( g_process->base_address( ) + offsets::fname_pool + 0x10 + 8ll * block ) + 4ll * offset;
            const uint16_t header = g_process->read<uint16_t>( data_ptr + 4 );

            const uint16_t read_len = header >> 1;
            if ( ( header & 1 ) || read_len != len )
                return 0;

            char enc[ 12 ];
            g_process->read_memory( data_ptr + 6, enc, len );

            uint32_t key = 0;
            for ( int i = 0; i < 4; i++ ) {
                reinterpret_cast< char* >( &key )[ i ] = enc[ i ] ^ plaintext[ i ] ^ len;
            }

            return key;
        }
    }

    std::string get_fname( std::uint32_t key ) {
        const std::uint32_t chunk_offset = static_cast< std::uint32_t >( key ) >> 16;
        const auto name_offset = static_cast< std::uint16_t >( key );

        const std::uint64_t pool_base = g_process->base_address( ) + offsets::fname_pool;
        const std::uint64_t name_pool_chunk = g_process->read<std::uint64_t>( pool_base + 0x10 + 8ULL * chunk_offset );
        if ( !utility::is_valid_pointer( name_pool_chunk ) )
            return {};

        const std::uint64_t entry_offset = name_pool_chunk + 4ULL * name_offset;
        if ( !utility::is_valid_pointer( entry_offset ) )
            return {};

        std::uint16_t header = 0;
        g_process->read_memory( entry_offset + 4, &header, sizeof( header ) );

        const std::uint16_t name_len = header >> 1;
        if ( name_len == 0 || name_len > 256 )
            return {};

        std::string name( name_len, '\0' );
        g_process->read_memory( entry_offset + 6, name.data( ), name_len );

        for ( std::uint16_t i = 0; i < name_len; ++i )
            name[ i ] ^= name_len ^ ( fname_key >> ( 8 * ( i & 3 ) ) ) & 0xFF;

        return name;
    }

    std::string get_fname( u_object* object ) {
        if ( !object )
            return {};
        return get_fname( object->name_private( ) );
    }
}