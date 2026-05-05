#pragma once
static NTSTATUS( __stdcall* NtDelayExecution )( BOOL Alertable, PLARGE_INTEGER DelayInterval ) = ( NTSTATUS( __stdcall* )( BOOL, PLARGE_INTEGER ) ) GetProcAddress( GetModuleHandleA( ( "ntdll.dll" ) ), ( "NtDelayExecution" ) );
static NTSTATUS( __stdcall* ZwSetTimerResolution )( IN ULONG RequestedResolution, IN BOOLEAN Set, OUT PULONG ActualResolution ) = ( NTSTATUS( __stdcall* )( ULONG, BOOLEAN, PULONG ) ) GetProcAddress( GetModuleHandleA( ( "ntdll.dll" ) ), ( "ZwSetTimerResolution" ) );

extern "C" NTSTATUS NTAPI NtOpenSection(
    PHANDLE section_handle,
    ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes
);

namespace logging {
    template<typename... Args>
    inline void print( const char* format, Args... args ) {
        auto now = std::chrono::system_clock::now( );
        std::time_t time = std::chrono::system_clock::to_time_t( now );
        tm local_tm;
        localtime_s( &local_tm, &time );
        printf( oxorany( "\x1b[38;2;162;210;255m[%02d/%02d/%04d %02d:%02d:%02d]\x1b[0m " ),
            local_tm.tm_mon + 1,
            local_tm.tm_mday,
            local_tm.tm_year + 1900,
            local_tm.tm_hour,
            local_tm.tm_min,
            local_tm.tm_sec );
        printf( oxorany( "\x1b[38;2;0;119;182m[kunai]\x1b[0m " ) );
        printf( oxorany( "\x1b[37m" ) );
        printf( format, args... );
        printf( oxorany( "\x1b[0m\n" ) );
    }

    template<typename... Args>
    inline void print_line( const char* format, Args... args ) {
        auto now = std::chrono::system_clock::now( );
        std::time_t time = std::chrono::system_clock::to_time_t( now );
        tm local_tm;
        localtime_s( &local_tm, &time );
        char msg[ 512 ]{};
        snprintf( msg, sizeof( msg ), format, args... );
        printf( "\r"
            "\x1b[38;2;162;210;255m[%02d/%02d/%04d %02d:%02d:%02d]\x1b[0m "
            "\x1b[38;2;0;119;182m[kunai]\x1b[0m "
            "\x1b[37m%s\x1b[0m",
            local_tm.tm_mon + 1,
            local_tm.tm_mday,
            local_tm.tm_year + 1900,
            local_tm.tm_hour,
            local_tm.tm_min,
            local_tm.tm_sec,
            msg );
    }
}

namespace utility {
    bool is_admin( ) {
        BOOL is_member = FALSE;
        PSID admin_sid = nullptr;
        SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
        if ( AllocateAndInitializeSid( &nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &admin_sid ) ) {
            CheckTokenMembership( nullptr, admin_sid, &is_member );
            FreeSid( admin_sid );
        }

        return is_member == TRUE;
    }

    bool enable_privilege( const wchar_t* priv_name ) {
        HANDLE token = nullptr;
        if ( !OpenProcessToken( GetCurrentProcess( ),
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token ) )
            return false;

        LUID luid{};
        if ( !LookupPrivilegeValueW( nullptr, priv_name, &luid ) ) {
            CloseHandle( token );
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[ 0 ].Luid = luid;
        tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges( token, FALSE, &tp, sizeof( tp ),
            nullptr, nullptr ) && GetLastError( ) == ERROR_SUCCESS;
        CloseHandle( token );
        return result;
    }

    std::uint32_t find_pid( std::wstring target_process ) {
        auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        if ( snapshot == INVALID_HANDLE_VALUE )
            return 0;

        PROCESSENTRY32W entry{ .dwSize = sizeof( entry ) };
        std::uint32_t result = 0;

        if ( Process32FirstW( snapshot, &entry ) ) {
            do {
                if ( target_process == entry.szExeFile ) {
                    result = entry.th32ProcessID;
                    break;
                }
            } while ( Process32NextW( snapshot, &entry ) );
        }

        CloseHandle( snapshot );
        return result;
    }

    bool has_system_token( ) {
        HANDLE token = nullptr;
        if ( !OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY, &token ) )
            return false;

        DWORD size = 0;
        GetTokenInformation( token, TokenUser, nullptr, 0, &size );
        if ( !size ) {
            CloseHandle( token );
            return false;
        }

        auto buf = std::make_unique<BYTE[ ]>( size );
        auto* user = reinterpret_cast< TOKEN_USER* >( buf.get( ) );
        if ( !GetTokenInformation( token, TokenUser, user, size, &size ) ) {
            CloseHandle( token );
            return false;
        }

        CloseHandle( token );

        BYTE system_sid[ SECURITY_MAX_SID_SIZE ]{};
        DWORD sid_size = sizeof( system_sid );
        if ( !CreateWellKnownSid( WinLocalSystemSid, nullptr, system_sid, &sid_size ) )
            return false;

        if ( !IsValidSid( user->User.Sid ) || !IsValidSid( system_sid ) )
            return false;

        return EqualSid( user->User.Sid, system_sid ) != FALSE;
    }

    bool is_dacl_locked( HANDLE h_process ) {
        PACL p_dacl = nullptr;
        PSECURITY_DESCRIPTOR p_sd = nullptr;

        DWORD result = GetSecurityInfo(
            h_process,
            SE_KERNEL_OBJECT,
            DACL_SECURITY_INFORMATION,
            nullptr, nullptr,
            &p_dacl,
            nullptr,
            &p_sd
        );

        if ( result != ERROR_SUCCESS ) {
            logging::print( oxorany( "GetSecurityInfo failed: 0x%08X" ), result );
            return false;
        }

        if ( !p_dacl ) {
            LocalFree( p_sd );
            return false;
        }

        ACL_SIZE_INFORMATION acl_info{};
        GetAclInformation( p_dacl, &acl_info, sizeof( acl_info ), AclSizeInformation );
        LocalFree( p_sd );

        return acl_info.AceCount > 0;
    }

    bool is_physical_address_valid( std::uint64_t pa, std::size_t size ) {
        static const std::uint64_t physical_mask = [ ]( ) -> std::uint64_t {
            int regs[ 4 ]{};
            __cpuid( regs, 0x80000000 );
            unsigned max_ext = static_cast< unsigned >( regs[ 0 ] );
            unsigned phys_bits = 36;
            if ( max_ext >= 0x80000008 ) {
                __cpuid( regs, 0x80000008 );
                unsigned b = regs[ 0 ] & 0xFF;
                if ( b >= 32 && b <= 52 )
                    phys_bits = b;
            }
            return ( phys_bits >= 63 ) ? ~0ULL : ( ( 1ULL << phys_bits ) - 1ULL );
            }( );

        if ( ( pa & ~physical_mask ) != 0 )
            return false;

        const auto end_pa = pa + ( size - 1 );
        if ( ( end_pa & ~physical_mask ) != 0 )
            return false;

        return true;
    }

    std::string add_commas( std::uint64_t n ) {
        auto s = std::to_string( n );
        auto pos = ( int )s.size( ) - 3;
        while ( pos > 0 ) {
            s.insert( pos, "," );
            pos -= 3;
        }
        return s;
    }

    void gen_rnd_str( wchar_t* random_str ) {
        static std::mt19937 rng( std::random_device{}( ) );
        std::uniform_int_distribution<size_t> len_dist( 8, 19 );
        size_t length = len_dist( rng );

        std::uniform_int_distribution<int> char_type( 0, 2 );
        std::uniform_int_distribution<int> alpha( 0, 25 );
        std::uniform_int_distribution<int> digit( 0, 9 );

        for ( size_t i = 0; i < length; ++i ) {
            switch ( char_type( rng ) ) {
                case 0:
                    random_str[ i ] = L'A' + alpha( rng );
                    break;
                case 1:
                    random_str[ i ] = L'a' + alpha( rng );
                    break;
                case 2:
                    random_str[ i ] = L'0' + digit( rng );
                    break;
            }
        }
        random_str[ length ] = L'\0';
    }

    nt_headers_t* get_nt_headers( void* image_base ) {
        const auto dos_header = reinterpret_cast< dos_header_t* >( image_base );
        if ( !dos_header->is_valid( ) )
            return 0;

        const auto nt_headers = reinterpret_cast< nt_headers_t* >( reinterpret_cast< std::uint64_t >( image_base ) + dos_header->m_lfanew );
        if ( !nt_headers->is_valid( ) )
            return 0;

        return nt_headers;
    }

    void sleep_short( float milliseconds ) {
        static bool once = true;
        if ( once ) {
            ULONG actualResolution;
            ZwSetTimerResolution( 1, true, &actualResolution );
            once = false;
        }

        LARGE_INTEGER interval;
        interval.QuadPart = -1 * ( int )( milliseconds * 10000.0f );
        NtDelayExecution( false, &interval );
    }

    void update_function( ch::milliseconds time, ch::high_resolution_clock::time_point& timestamp, const std::function<void( )>& task ) {
        ch::high_resolution_clock::time_point current_time = std::chrono::high_resolution_clock::now( );

        if ( ( current_time - timestamp ) >= time ) {
            task( );
            timestamp = current_time;
        }
    }

    constexpr kunai::color from_hex( uint32_t rgb, float a = 1.0f ) {
        return {
            ( ( rgb >> 16 ) & 0xff ) / 255.0f,
            ( ( rgb >> 8 ) & 0xff ) / 255.0f,
            ( ( rgb >> 0 ) & 0xff ) / 255.0f,
            a
        };
    }

    __forceinline uintptr_t to_addr( const void* pointer ) {
        return reinterpret_cast< uintptr_t >( pointer );
    }

    __forceinline void* to_ptr( uintptr_t address ) {
        return reinterpret_cast< void* >( address );
    }

    __forceinline bool is_valid_pointer( uintptr_t address ) {
        return ( address >= 0x0000000000010000 && address < 0x00007FFFFFFEFFFF );
    }

    __forceinline bool is_valid_pointer( const void* pointer ) {
        return is_valid_pointer( to_addr( pointer ) );
    }
}