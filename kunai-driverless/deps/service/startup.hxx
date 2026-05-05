#pragma once

namespace cache {
	bool set_driver_load( std::uint32_t pid );
}

namespace service {
	class c_service {
	public:
		bool create( ) {
			if ( !load_driver_privilage( true ) ) {
				logging::print( oxorany( "Could not enable driver loading privilege.\n" ) );
				return false;
			}

			utility::gen_rnd_str( m_driver_name );
			if ( !install_service( m_driver_name ) ) {
				load_driver_privilage( false );
				return false;
			}

			return true;
		}

		bool cleanup( ) {
			auto services_pid = utility::find_pid( oxorany( L"services.exe" ) );
			if ( !cache::set_driver_load( services_pid ) )
				return false;

			memset( driver_bytes, 0, sizeof( driver_bytes ) );
			return true;
		}

		void unload( ) {
			uninstall_services( );
			load_driver_privilage( false );
		}

	private:
		wchar_t m_driver_name[ 20 ]{ };

		std::uint32_t get_timestamp( ) {
			auto dos_header = reinterpret_cast< PIMAGE_DOS_HEADER >( driver_bytes );
			if ( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
				return 0;

			auto nt_headers = reinterpret_cast< PIMAGE_NT_HEADERS64 >( reinterpret_cast< std::uint64_t >( driver_bytes ) + dos_header->e_lfanew );
			if ( nt_headers->Signature != IMAGE_NT_SIGNATURE )
				return 0;

			return nt_headers->FileHeader.TimeDateStamp;
		}
	};
}