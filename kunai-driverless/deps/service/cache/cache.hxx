#pragma once

namespace cache {
	bool set_driver_load( std::uint32_t pid ) {
		void* buffer = nullptr;
		ULONG buffer_size = 0;

		auto result = NtQuerySystemInformation(
			static_cast< SYSTEM_INFORMATION_CLASS >( 64 ),
			buffer, buffer_size, &buffer_size );

		while ( result == 0xC0000004 ) {
			if ( buffer ) VirtualFree( buffer, 0, MEM_RELEASE );
			buffer = VirtualAlloc( nullptr, buffer_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
			result = NtQuerySystemInformation(
				static_cast< SYSTEM_INFORMATION_CLASS >( 64 ),
				buffer, buffer_size, &buffer_size );
		}

		if ( result || !buffer ) {
			if ( buffer ) VirtualFree( buffer, 0, MEM_RELEASE );
			logging::print( oxorany( "could not query system information" ) );
			return false;
		}

		auto handle_info = static_cast< system_handle_information_ex_t* >( buffer );

		for ( auto idx = 0u; idx < handle_info->m_number_of_handles; ++idx ) {
			auto& handle = handle_info->m_handles[ idx ];
			if ( handle.m_unique_process_id != GetCurrentProcessId( ) )
				continue;
			if ( handle.m_handle_value == reinterpret_cast< std::uint64_t >( g_driver->get_handle( ) ) ) {
				handle.m_unique_process_id = pid;
				VirtualFree( buffer, 0, MEM_RELEASE );
				return true;
			}
		}

		VirtualFree( buffer, 0, MEM_RELEASE );
		return false;
	}
}