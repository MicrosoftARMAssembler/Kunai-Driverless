#pragma once

namespace pdb {
    class ComInitializer {
    public:
        ComInitializer( ) {
            HRESULT hr = CoInitialize( NULL );
            initialized = SUCCEEDED( hr );
        }
        ~ComInitializer( ) {
            if ( initialized ) {
                CoUninitialize( );
            }
        }
    private:
        bool initialized;
    };

    static ComInitializer g_com_init;

    struct pdb_info {
        DWORD signature;
        GUID guid;
        DWORD age;
        char pdb_file_name[ 1 ];
    };

    class c_pdb {
    public:
        std::string m_module_bare;
        std::uint64_t m_module_base;
        std::vector<std::pair<std::string, std::uint64_t>> m_symbols;

        c_pdb( const char* module_name ) : m_module_name( module_name ) {
            std::string s = module_name;
            auto slash = s.find_last_of( "\\/" );
            if ( slash != std::string::npos ) {
                m_module_bare = s.substr( slash + 1 );
            }
            else {
                m_module_bare = s;
            }

            m_pe_path = get_pe_path( module_name );
        }

        ~c_pdb( ) {
            cleanup( );
        }

        bool load( ) {
            this->m_module_base = get_kernel_module( m_module_bare.c_str( ) );
            if ( !m_module_base ) {
                logging::print( oxorany( "failed to find module" ) );
                return false;
            }

            m_pdb_path = find_cached_pdb( );
            if ( m_pdb_path.empty( ) ) {
                m_pdb_path = download_pdb( m_pe_path );
                if ( m_pdb_path.empty( ) ) {
                    logging::print( oxorany( "failed to download PDB" ) );
                    return false;
                }
            }

            CComPtr<IDiaDataSource> pSource;
            HRESULT hr = CoCreateInstance( CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER,
                IID_IDiaDataSource, ( void** )&pSource );

            if ( FAILED( hr ) ) {
                const wchar_t* dia_versions[ ] = { L"msdia140.dll", L"msdia120.dll", L"msdia110.dll" };
                for ( const auto& dll : dia_versions ) {
                    hr = NoRegCoCreate( dll, CLSID_DiaSource, IID_IDiaDataSource, ( void** )&pSource );
                    if ( SUCCEEDED( hr ) ) break;
                }
                if ( FAILED( hr ) ) {
                    logging::print( oxorany( "failed to create DIA data source: 0x%08x" ), hr );
                    return false;
                }
            }

            std::wstring wide_path( m_pdb_path.begin( ), m_pdb_path.end( ) );
            hr = pSource->loadDataFromPdb( wide_path.c_str( ) );
            if ( FAILED( hr ) ) {
                logging::print( oxorany( "cached PDB load failed (0x%08x), re-downloading..." ), hr );
                DeleteFileA( m_pdb_path.c_str( ) );
                m_pdb_path = download_pdb( m_pe_path );
                if ( m_pdb_path.empty( ) ) return false;

                wide_path = std::wstring( m_pdb_path.begin( ), m_pdb_path.end( ) );
                hr = pSource->loadDataFromPdb( wide_path.c_str( ) );
                if ( FAILED( hr ) ) {
                    logging::print( oxorany( "failed to load re-downloaded PDB: 0x%08x" ), hr );
                    return false;
                }
            }

            CComPtr<IDiaSession> pSession;
            hr = pSource->openSession( &pSession );
            if ( FAILED( hr ) ) {
                logging::print( oxorany( "failed to open DIA session: 0x%08x" ), hr );
                return false;
            }

            CComPtr<IDiaSymbol> pGlobal;
            hr = pSession->get_globalScope( &pGlobal );
            if ( FAILED( hr ) ) {
                logging::print( oxorany( "failed to get global scope: 0x%08x" ), hr );
                return false;
            }

            m_pSource = pSource.Detach( );
            m_pSession = pSession.Detach( );
            m_pGlobal = pGlobal.Detach( );

            if ( !enumerate_symbols( ) ) {
                logging::print( oxorany( "failed to enumerate symbols" ) );
                cleanup( );
                return false;
            }

            return true;
        }

        std::uint64_t get_symbol_address( const char* symbol_name ) {
            for ( const auto& symbol : m_symbols ) {
                if ( symbol.first == symbol_name )
                    return this->m_module_base + symbol.second;
            }
            return 0;
        }

        std::uint64_t find_member_in_udt( IDiaSymbol* pUDT, const std::wstring& member_name, LONG base_offset ) {
            CComPtr<IDiaEnumSymbols> pEnumData;
            if ( SUCCEEDED( pUDT->findChildren( SymTagData, member_name.c_str( ), nsfCaseInsensitive, &pEnumData ) ) && pEnumData ) {
                CComPtr<IDiaSymbol> pMember;
                ULONG celt = 0;
                if ( SUCCEEDED( pEnumData->Next( 1, &pMember, &celt ) ) && celt == 1 && pMember ) {
                    LONG offset = 0;
                    if ( SUCCEEDED( pMember->get_offset( &offset ) ) )
                        return static_cast< std::uint64_t >( base_offset + offset );
                }
            }

            CComPtr<IDiaEnumSymbols> pEnumBases;
            if ( FAILED( pUDT->findChildren( SymTagBaseClass, nullptr, nsNone, &pEnumBases ) ) || !pEnumBases )
                return 0;

            CComPtr<IDiaSymbol> pBase;
            ULONG celt = 0;
            while ( SUCCEEDED( pEnumBases->Next( 1, &pBase, &celt ) ) && celt == 1 ) {
                LONG base_off = 0;
                pBase->get_offset( &base_off );

                CComPtr<IDiaSymbol> pBaseType;
                if ( SUCCEEDED( pBase->get_type( &pBaseType ) ) && pBaseType ) {
                    auto result = find_member_in_udt( pBaseType, member_name, base_offset + base_off );
                    if ( result ) return result;
                }
                pBase.Release( );
            }

            return 0;
        }

        std::uint64_t get_struct_size( const char* type_name ) {
            if ( !m_pGlobal ) return 0;

            std::wstring w_type( type_name, type_name + strlen( type_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return 0;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return 0;

            ULONGLONG size = 0;
            if ( FAILED( pUDT->get_length( &size ) ) )
                return 0;

            return static_cast< std::uint64_t >( size );
        }

        std::uint64_t get_struct_member( const char* type_name, const char* member_name ) {
            if ( !m_pGlobal ) return 0;

            std::wstring w_type( type_name, type_name + strlen( type_name ) );
            std::wstring w_member( member_name, member_name + strlen( member_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return 0;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return 0;

            return find_member_in_udt( pUDT, w_member, 0 );
        }

        void dump_struct_members( const char* type_name ) {
            if ( !m_pGlobal ) return;
            std::wstring w_type( type_name, type_name + strlen( type_name ) );

            CComPtr<IDiaEnumSymbols> pEnumTypes;
            if ( FAILED( m_pGlobal->findChildren( SymTagUDT, w_type.c_str( ), nsfCaseInsensitive, &pEnumTypes ) ) || !pEnumTypes )
                return;

            CComPtr<IDiaSymbol> pUDT;
            ULONG celt = 0;
            if ( FAILED( pEnumTypes->Next( 1, &pUDT, &celt ) ) || celt != 1 || !pUDT )
                return;

            CComPtr<IDiaEnumSymbols> pChildren;
            if ( FAILED( pUDT->findChildren( SymTagNull, nullptr, nsNone, &pChildren ) ) || !pChildren )
                return;

            CComPtr<IDiaSymbol> pChild;
            while ( SUCCEEDED( pChildren->Next( 1, &pChild, &celt ) ) && celt == 1 ) {
                BSTR name = nullptr;
                LONG offset = 0;
                pChild->get_name( &name );
                pChild->get_offset( &offset );
                if ( name ) {
                    char buf[ 256 ]{};
                    size_t n = 0;
                    wcstombs_s( &n, buf, name, sizeof( buf ) - 1 );
                    logging::print( oxorany( "  [0x%x] %s" ), offset, buf );
                    SysFreeString( name );
                }
                pChild.Release( );
            }
        }

        std::uint64_t get_symbol_rva( const char* symbol_name ) {
            return get_symbol_address( symbol_name );
        }

        static std::uint64_t get_kernel_module( const char* module_name ) {
            void* buffer = nullptr;
            unsigned long buffer_size = 0;
            NTSTATUS status = NtQuerySystemInformation(
                static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
                buffer, buffer_size, &buffer_size
            );

            while ( status == 0xC0000004L ) {
                if ( buffer ) {
                    VirtualFree( buffer, 0, MEM_RELEASE );
                    buffer = nullptr;
                }
                buffer = VirtualAlloc( nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
                if ( !buffer ) {
                    return 0;
                }
                status = NtQuerySystemInformation(
                    static_cast< SYSTEM_INFORMATION_CLASS >( 11 ),
                    buffer, buffer_size, &buffer_size
                );
            }

            if ( !NT_SUCCESS( status ) || !buffer ) {
                if ( buffer ) VirtualFree( buffer, 0, MEM_RELEASE );
                return 0;
            }

            const auto modules = reinterpret_cast< rtl_process_modules_t* >( buffer );
            for ( auto idx = 0u; idx < modules->m_count; ++idx ) {
                const auto current_module_name = std::string(
                    reinterpret_cast< char* >( modules->m_modules[ idx ].m_full_path ) +
                    modules->m_modules[ idx ].m_offset_to_file_name
                );
                if ( !_stricmp( current_module_name.c_str( ), module_name ) ) {
                    const auto module_base = reinterpret_cast< std::uint64_t >( modules->m_modules[ idx ].m_image_base );
                    VirtualFree( buffer, 0, MEM_RELEASE );
                    return module_base;
                }
            }

            VirtualFree( buffer, 0, MEM_RELEASE );
            return 0;
        }

    private:
        std::string m_module_name;
        std::string m_pe_path;
        std::string m_pdb_path;

        IDiaDataSource* m_pSource = nullptr;
        IDiaSession* m_pSession = nullptr;
        IDiaSymbol* m_pGlobal = nullptr;

        std::string get_pe_path( const char* module_name ) {
            return std::string( std::getenv( "systemroot" ) ) + "\\System32\\" + module_name;
        }

        std::string get_module_prefix( ) {
            std::string prefix = m_module_bare;
            auto dot = prefix.rfind( '.' );
            if ( dot != std::string::npos )
                prefix = prefix.substr( 0, dot );
            for ( auto& c : prefix )
                c = static_cast< char >( tolower( static_cast< unsigned char >( c ) ) );
            return prefix;
        }

        std::string find_cached_pdb( ) {
            char sz_dir[ MAX_PATH ] = { };
            if ( !GetCurrentDirectoryA( sizeof( sz_dir ), sz_dir ) )
                return "";

            std::string pattern = std::string( sz_dir ) + "\\" + get_module_prefix( ) + "_*.pdb";

            WIN32_FIND_DATAA fd = { };
            HANDLE h = FindFirstFileA( pattern.c_str( ), &fd );
            if ( h == INVALID_HANDLE_VALUE )
                return "";

            std::string cached = std::string( sz_dir ) + "\\" + fd.cFileName;
            FindClose( h );

            std::ifstream v( cached, std::ios::binary | std::ios::ate );
            if ( !v.is_open( ) || v.tellg( ) < 1024 )
                return "";

            return cached;
        }

        std::string download_pdb( const std::string& pe_path ) {
            if ( pe_path.empty( ) ) {
                logging::print( oxorany( "pe path is empty" ) );
                return "";
            }

            char sz_download_dir[ MAX_PATH ] = { };
            if ( !GetCurrentDirectoryA( sizeof( sz_download_dir ), sz_download_dir ) ) {
                logging::print( oxorany( "failed to get current directory" ) );
                return "";
            }

            std::string download_path = sz_download_dir;
            if ( download_path.back( ) != '\\' )
                download_path += "\\";

            std::ifstream file( pe_path, std::ios::binary | std::ios::ate );
            if ( !file.is_open( ) ) {
                logging::print( oxorany( "failed to open PE file: %s" ), pe_path.c_str( ) );
                return "";
            }

            auto size = file.tellg( );
            if ( size <= 0 ) {
                logging::print( oxorany( "PE file is empty" ) );
                return "";
            }

            file.seekg( 0, std::ios::beg );
            std::vector<char> buffer( size );
            if ( !file.read( buffer.data( ), size ) ) {
                logging::print( oxorany( "failed to read PE file" ) );
                return "";
            }

            if ( buffer.size( ) < sizeof( IMAGE_DOS_HEADER ) ) {
                logging::print( oxorany( "file too small" ) );
                return "";
            }

            auto p_dos = reinterpret_cast< IMAGE_DOS_HEADER* >( buffer.data( ) );
            if ( p_dos->e_magic != IMAGE_DOS_SIGNATURE ) {
                logging::print( oxorany( "invalid DOS signature" ) );
                return "";
            }

            if ( buffer.size( ) < static_cast< size_t >( p_dos->e_lfanew ) + sizeof( IMAGE_NT_HEADERS ) ) {
                logging::print( oxorany( "invalid PE structure" ) );
                return "";
            }

            auto p_nt = reinterpret_cast< IMAGE_NT_HEADERS* >( buffer.data( ) + p_dos->e_lfanew );
            if ( p_nt->Signature != IMAGE_NT_SIGNATURE ) {
                logging::print( oxorany( "invalid NT signature" ) );
                return "";
            }

            bool is_x86 = ( p_nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 );
            auto p_opt32 = reinterpret_cast< IMAGE_OPTIONAL_HEADER32* >( &p_nt->OptionalHeader );
            auto p_opt64 = reinterpret_cast< IMAGE_OPTIONAL_HEADER64* >( &p_nt->OptionalHeader );
            auto image_size = is_x86 ? p_opt32->SizeOfImage : p_opt64->SizeOfImage;

            auto image_buffer = std::make_unique<BYTE[ ]>( image_size );
            if ( !image_buffer ) {
                logging::print( oxorany( "failed to allocate memory" ) );
                return "";
            }

            auto headers_size = is_x86 ? p_opt32->SizeOfHeaders : p_opt64->SizeOfHeaders;
            std::memcpy( image_buffer.get( ), buffer.data( ), headers_size );

            auto p_section = IMAGE_FIRST_SECTION( p_nt );
            for ( UINT i = 0; i < p_nt->FileHeader.NumberOfSections; ++i, ++p_section ) {
                if ( p_section->SizeOfRawData ) {
                    std::memcpy( image_buffer.get( ) + p_section->VirtualAddress,
                        buffer.data( ) + p_section->PointerToRawData,
                        p_section->SizeOfRawData );
                }
            }

            IMAGE_DATA_DIRECTORY* p_data_dir = is_x86
                ? &p_opt32->DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ]
                : &p_opt64->DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ];

            if ( !p_data_dir->Size ) {
                logging::print( oxorany( "no debug directory found" ) );
                return "";
            }

            auto p_debug_dir = reinterpret_cast< IMAGE_DEBUG_DIRECTORY* >(
                image_buffer.get( ) + p_data_dir->VirtualAddress );

            if ( p_debug_dir->Type != IMAGE_DEBUG_TYPE_CODEVIEW ) {
                logging::print( oxorany( "not CodeView debug type" ) );
                return "";
            }

            auto pdb_info_ptr = reinterpret_cast< pdb_info* >(
                image_buffer.get( ) + p_debug_dir->AddressOfRawData );

            if ( pdb_info_ptr->signature != 0x53445352 ) {
                logging::print( oxorany( "invalid PDB signature" ) );
                return "";
            }

            wchar_t w_guid[ 100 ] = { };
            if ( !StringFromGUID2( pdb_info_ptr->guid, w_guid, 100 ) ) {
                logging::print( oxorany( "failed to convert GUID" ) );
                return "";
            }

            char a_guid[ 100 ] = { };
            size_t l_guid = 0;
            wcstombs_s( &l_guid, a_guid, w_guid, sizeof( a_guid ) );

            char guid_filtered[ 256 ] = { };
            for ( size_t i = 0; i < l_guid; ++i ) {
                char c = a_guid[ i ];
                if ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' ) )
                    guid_filtered[ strlen( guid_filtered ) ] = c;
            }

            char age[ 16 ] = { };
            sprintf_s( age, "%X", pdb_info_ptr->age );

            std::string url = "https://msdl.microsoft.com/download/symbols/";
            url += pdb_info_ptr->pdb_file_name;
            url += "/";
            url += guid_filtered;
            url += age;
            url += "/";
            url += pdb_info_ptr->pdb_file_name;

            std::string pdb_path = download_path + get_module_prefix( ) + "_" + guid_filtered + age + ".pdb";
            HRESULT hr = URLDownloadToFileA( NULL, url.c_str( ), pdb_path.c_str( ), NULL, NULL );
            if ( FAILED( hr ) ) {
                logging::print( oxorany( "download failed: 0x%08x" ), hr );
                return "";
            }

            std::ifstream verify( pdb_path, std::ios::binary | std::ios::ate );
            if ( !verify.is_open( ) || verify.tellg( ) < 1024 ) {
                logging::print( oxorany( "downloaded PDB is invalid or too small" ) );
                DeleteFileA( pdb_path.c_str( ) );
                return "";
            }
            verify.close( );

            return pdb_path;
        }

        bool enumerate_symbols( ) {
            if ( !m_pGlobal ) return false;

            CComPtr<IDiaEnumSymbols> pEnumSymbols;
            HRESULT hr = m_pGlobal->findChildren( SymTagNull, NULL, nsNone, &pEnumSymbols );
            if ( FAILED( hr ) ) {
                logging::print( oxorany( "failed to enumerate: 0x%08x" ), hr );
                return false;
            }

            CComPtr<IDiaSymbol> pSymbol;
            ULONG celt = 0;
            while ( SUCCEEDED( pEnumSymbols->Next( 1, &pSymbol, &celt ) ) && celt == 1 ) {
                BSTR name;
                if ( SUCCEEDED( pSymbol->get_name( &name ) ) ) {
                    DWORD rva = 0;
                    if ( SUCCEEDED( pSymbol->get_relativeVirtualAddress( &rva ) ) ) {
                        char symbol_name[ 1024 ] = { };
                        size_t converted = 0;
                        wcstombs_s( &converted, symbol_name, name, sizeof( symbol_name ) - 1 );
                        if ( converted > 0 && symbol_name[ 0 ] != '\0' )
                            m_symbols.emplace_back( symbol_name, rva );
                    }
                    SysFreeString( name );
                }
                pSymbol.Release( );
            }

            return !m_symbols.empty( );
        }

        void cleanup( ) {
            if ( m_pGlobal ) { m_pGlobal->Release( );  m_pGlobal = nullptr; }
            if ( m_pSession ) { m_pSession->Release( ); m_pSession = nullptr; }
            if ( m_pSource ) { m_pSource->Release( );  m_pSource = nullptr; }
        }
    };
}