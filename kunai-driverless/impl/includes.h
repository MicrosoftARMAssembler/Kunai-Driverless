#pragma once
#include <chrono>
#include <ctime>
#include <vector>
#include <Windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <winternl.h>
#define _NTDEF_
#include <ntsecapi.h>
#undef _NTDEF_
#include <cstdint>
#include <DbgHelp.h>
#include <thread>
#include <functional>
#include <map>
#include <atlbase.h>
#include <string>
#include <memory>
#include <random>
#include <ntstatus.h>
#include <dia2.h>
#include <diacreate.h>
#include <array>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <numbers>
#include <unordered_set>
#include <aclapi.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "diaguids.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")
namespace ch = std::chrono;

#include <impl/ia32/ia32.h>
#include <impl/gui/window.h>
#include <impl/gui/device.h>
#include <impl/gui/font.h>
#include <impl/gui/image.h>
#include <impl/gui/text_renderer.h>
#include <impl/gui/ui.h>
#include <impl/gui/embedded_font.h>
#include <impl/gui/embedded_font_awesome.h>
#include <impl/gui/embedded_logo.h>

#include <deps/oxorany/include.h>
#include <core/utility/utility.hxx>

#include <impl/pdb/pdb.hxx>
auto g_pdb = std::make_shared<pdb::c_pdb>( oxorany( "ntoskrnl.exe" ) );

#include <deps/service/driver/driver.hxx>
auto g_driver = std::make_shared<driver::c_driver>( );

#include <deps/service/bytes.h>
#include <deps/service/service.hxx>
#include <deps/service/cache/cache.hxx>
#include <deps/service/startup.hxx>
auto g_service = std::make_shared<service::c_service>( );

#include <core/loader/authority/authority.hxx>
auto g_authority = std::make_unique< authority::c_authority >( );

#include <core/loader/memory/paging/paging.hxx>
auto g_paging = std::make_unique< paging::c_paging >( );

#include <core/loader/memory/memory.hxx>
#include <core/loader/memory/syscall/syscall.hxx>
auto g_syscall = std::make_unique< syscall::c_syscall >( );

#include <core/loader/memory/physical/physical.hxx>
auto g_physical = std::make_unique< physical::c_physical >( );

#include <core/loader/process/process.hxx>
auto g_process = std::make_unique< process::c_process >( L"VALORANT-Win64-Shipping.exe" );

#include <core/external/game/engine/engine.h>
#include <core/external/game/cache/cache.h>
#include <core/external/game/cache/mutex/mutex.h>
#include <core/external/game/cache/cache.hxx>
auto g_cache = std::make_unique< cache::c_cache >( );

#include <core/external/game/engine/math/math.hxx>
#include <core/external/render/menu/menu.hxx>
#include <core/external/render/render.hxx>

#include <core/utility/crash.hxx>