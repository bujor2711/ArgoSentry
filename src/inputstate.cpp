#include "include/ArgoSentry/inputstate.hh"
#include "ArgoSentry/dma_internal.hh"  // Complete DMA definitions

#include <string>
#include <iostream>
#include <sstream>

// Inline logger definition since external include is having issues
namespace Volk {
    namespace Log {
        class Logger {
        public:
            explicit Logger(const char* name) : logger_name(name) {}

            void info(const std::string& message) const {
                log("INFO", message);
            }

            template<typename T>
            void info(const std::string& format, T&& arg) const {
                log("INFO", format_message(format, std::forward<T>(arg)));
            }

            template<typename T, typename... Args>
            void info(const std::string& format, T&& arg, Args&&... args) const {
                log("INFO", format_message(format, std::forward<T>(arg), std::forward<Args>(args)...));
            }

            void warn(const std::string& message) const {
                log("WARN", message);
            }

            template<typename T>
            void warn(const std::string& format, T&& arg) const {
                log("WARN", format_message(format, std::forward<T>(arg)));
            }

            template<typename T, typename... Args>
            void warn(const std::string& format, T&& arg, Args&&... args) const {
                log("WARN", format_message(format, std::forward<T>(arg), std::forward<Args>(args)...));
            }

            void error(const std::string& message) const {
                log("ERROR", message);
            }

            template<typename T>
            void error(const std::string& format, T&& arg) const {
                log("ERROR", format_message(format, std::forward<T>(arg)));
            }

            template<typename T, typename... Args>
            void error(const std::string& format, T&& arg, Args&&... args) const {
                log("ERROR", format_message(format, std::forward<T>(arg), std::forward<Args>(args)...));
            }

            void debug(const std::string& message) const {
                log("DEBUG", message);
            }

            template<typename T>
            void debug(const std::string& format, T&& arg) const {
                log("DEBUG", format_message(format, std::forward<T>(arg)));
            }

            template<typename T, typename... Args>
            void debug(const std::string& format, T&& arg, Args&&... args) const {
                log("DEBUG", format_message(format, std::forward<T>(arg), std::forward<Args>(args)...));
            }

        private:
            const char* logger_name;

            void log(const char* level, const std::string& message) const {
                std::cout << "[" << level << "] " << logger_name << ": " << message << std::endl;
            }

            template<typename T>
            std::string format_message(const std::string& format, T&& arg) const {
                std::ostringstream oss;
                size_t pos = 0;
                format_single_arg(oss, format, pos, std::forward<T>(arg));
                return oss.str();
            }

            template<typename T, typename... Args>
            std::string format_message(const std::string& format, T&& arg, Args&&... args) const {
                std::ostringstream oss;
                size_t pos = 0;
                format_single_arg(oss, format, pos, std::forward<T>(arg));
                format_remaining_args(oss, format, pos, std::forward<Args>(args)...);
                return oss.str();
            }

            template<typename T>
            void format_single_arg(std::ostringstream& oss, const std::string& format, size_t& pos, T&& arg) const {
                size_t start = pos;
                size_t found = format.find("{}", start);
                if (found != std::string::npos) {
                    oss << format.substr(start, found - start);
                    oss << arg;
                    pos = found + 2;
                } else {
                    oss << format.substr(start);
                    pos = format.length();
                }
            }

            template<typename T, typename... Args>
            void format_remaining_args(std::ostringstream& oss, const std::string& format, size_t& pos, T&& arg, Args&&... args) const {
                format_single_arg(oss, format, pos, std::forward<T>(arg));
                if constexpr (sizeof...(args) > 0) {
                    format_remaining_args(oss, format, pos, std::forward<Args>(args)...);
                } else {
                    if (pos < format.length()) {
                        oss << format.substr(pos);
                    }
                }
            }
        };
    }
}

#include "external/vmm/vmmdll.h"

#include "include/ArgoSentry/dma.hh"
#include "include/ArgoSentry/internal/volkresource.hh"

static const Volk::Log::Logger logger{ "INPUTSTATE" };

InputState::InputState(const DMA& dma) : dma(dma) {
    const std::vector<DWORD> csrss_process_ids = dma.get_process_id_list("csrss.exe");

    if (retrieve_gptCursorAsync(csrss_process_ids)) {
        logger.info("Successfully retrieved gptCursorAsync.");
    }
    else {
        logger.error("Failed to retrieve gptCursorAsync.");
    }

    if (!VMMDLL_ConfigGet(dma.handle.get(), VMMDLL_OPT_WIN_VERSION_BUILD, &windows_version_build)) {
        logger.error("Failed to retrieve Windows build.");
        return;
    }

    if (retrieve_gafAsyncKeyState(csrss_process_ids)) {
        logger.info("Successfully retrieved gafAsyncKeyState.");
    }
    else {
        logger.error("Failed to retrieve gafAsyncKeyState.");
    }
}

bool InputState::retrieve_gafAsyncKeyState(const std::vector<DWORD>& csrss_process_ids) {
    winlogon_process_id = dma.get_process_id("winlogon.exe");
    if (!winlogon_process_id) {
        logger.error("Failed to get process ID for winlogon.exe.");
        return false;
    }

    if (windows_version_build > 22000) {
        if (csrss_process_ids.empty()) {
            logger.error("No csrss.exe processes found.");
            return false;
        }

        for (const DWORD& process_id : csrss_process_ids) {
            VolkResource<VMMDLL_MAP_MODULEENTRY> win32k_module_info{};
            std::string_view win32k_module_name;
            if (VMMDLL_Map_GetModuleFromNameW(dma.handle.get(), process_id, const_cast<LPWSTR>(L"win32ksgd.sys"), win32k_module_info.out(), VMMDLL_MODULE_FLAG_NORMAL)) {
                win32k_module_name = "win32ksgd.sys";
            }
            else if (VMMDLL_Map_GetModuleFromNameW(dma.handle.get(), process_id, const_cast<LPWSTR>(L"win32k.sys"), win32k_module_info.out(), VMMDLL_MODULE_FLAG_NORMAL)) {
                win32k_module_name = "win32k.sys";
            }
            else {
                logger.error("Failed to find win32ksgd.sys or win32k.sys for csrss.exe (PID: {}).", process_id);
                continue;
            }

            uint64_t g_session_address = dma.find_signature("48 8B 05 ? ? ? ? 48 8B 04 C8", win32k_module_info->vaBase, win32k_module_info->vaBase + win32k_module_info->cbImageSize, process_id);
            if (!g_session_address)
                g_session_address = dma.find_signature("48 8B 05 ? ? ? ? FF C9", win32k_module_info->vaBase, win32k_module_info->vaBase + win32k_module_info->cbImageSize, process_id);

            if (!g_session_address) {
                logger.error("Failed to find signature in {} for csrss.exe (PID: {}).", win32k_module_name, process_id);
                continue;
            }

            uint64_t user_session_state = 0;
            for (int i = 0; i < 4; i++) {
                user_session_state = dma.read<uint64_t>(dma.read<uint64_t>(dma.read<uint64_t>(g_session_address + 7 + dma.read<int>(g_session_address + 3, process_id), process_id) + 8 * i, process_id), process_id);
                if (user_session_state > 0x7FFFFFFFFFFF)
                    break;
            }

            VolkResource<VMMDLL_MAP_MODULEENTRY> win32kbase_info{};
            if (!VMMDLL_Map_GetModuleFromNameW(dma.handle.get(), process_id, const_cast<LPWSTR>(L"win32kbase.sys"), win32kbase_info.out(), VMMDLL_MODULE_FLAG_NORMAL)) {
                logger.error("Failed to find win32kbase.sys for csrss.exe (PID: {}).", process_id);
                continue;
            }

            uint64_t sig_ptr = dma.find_signature("48 8D 90 ? ? ? ? E8 ? ? ? ? 0F 57 C0", win32kbase_info->vaBase, win32kbase_info->vaBase + win32kbase_info->cbImageSize, process_id);
            if (!sig_ptr) {
                logger.error("Failed to find signature in win32kbase.sys for csrss.exe (PID: {}).", process_id);
                continue;
            }

            gafAsyncKeyState_address = user_session_state + dma.read<uint32_t>(sig_ptr + 3, process_id);

            if (gafAsyncKeyState_address > 0x7FFFFFFFFFFF) {
                return true;
            }
        }

        return false;
    }

    // windows_version_build <= 22000
    VolkResource<VMMDLL_MAP_EAT> eat_map{};
    if (!VMMDLL_Map_GetEATU(dma.handle.get(), winlogon_process_id | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, const_cast<LPSTR>("win32kbase.sys"), eat_map.out()) || eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION) {
        logger.error("Failed to retrieve EAT map in win32kbase.sys for winlogon.exe (PID: {}).", winlogon_process_id);
        return false;
    }

    for (DWORD i = 0; i < eat_map->cMap; ++i) {
        PVMMDLL_MAP_EATENTRY entry = eat_map->pMap + i;
        if (strcmp(entry->uszFunction, "gafAsyncKeyState") == 0) {
            gafAsyncKeyState_address = entry->vaFunction;
            break;
        }
    }

    return gafAsyncKeyState_address > 0x7FFFFFFFFFFF;
}

bool InputState::retrieve_gptCursorAsync(const std::vector<DWORD>& csrss_process_ids) {
    if (csrss_process_ids.empty()) {
        logger.error("No csrss.exe processes found.");
        return false;
    }

    for (const DWORD& process_id : csrss_process_ids) {
        VolkResource<VMMDLL_MAP_EAT> eat_map;
        if (!VMMDLL_Map_GetEATU(dma.handle.get(), process_id | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, const_cast<LPSTR>("win32kbase.sys"), eat_map.out())) {
            logger.error("Failed to retrieve EAT map in win32kbase.sys for csrss.exe (PID: {}).", process_id);
            continue;
        }

        if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION) {
            logger.error("EAT version mismatch for csrss.exe (PID: {}): got {}.", process_id, eat_map->dwVersion);
            continue;
        }

        for (DWORD i = 0; i < eat_map->cMap; ++i) {
            auto& entry = eat_map->pMap[i];

            if (!entry.uszFunction) continue;

            std::string_view export_function_name(entry.uszFunction);
            if (export_function_name.find("gptCursorAsync") == std::string::npos) continue;

            Point position = dma.read<Point>(entry.vaFunction, process_id);

            if (((position.x == 0 && position.y == 0) || (position.x == 512 && position.y == 384))) continue;

            gptCursorAsync_process_id = process_id;
            gptCursorAsync_address = entry.vaFunction;
            break;
        }
    }

    return (gptCursorAsync_address != 0 && gptCursorAsync_process_id != 0);
}

InputState::Point InputState::get_cursor_position() const {
    return dma.read<Point>(gptCursorAsync_address, gptCursorAsync_process_id);
}

bool InputState::read_bitmap() {
    prev_bitmap = state_bitmap;
    return VMMDLL_MemReadEx(dma.handle.get(), winlogon_process_id | VMMDLL_PID_PROCESS_WITH_KERNELMEMORY, gafAsyncKeyState_address, reinterpret_cast<PBYTE>(&state_bitmap), sizeof(state_bitmap), nullptr, VMMDLL_FLAG_NOCACHE);
}

bool InputState::get_bit(const std::array<uint8_t, 64>& bitmap, uint8_t virtual_key_code) const {
    const int bit_index = virtual_key_code * 2;
    return (bitmap[bit_index / 8] & (1 << (bit_index % 8))) != 0;
}

bool InputState::is_key_held(uint8_t virtual_key_code) const {
    return get_bit(state_bitmap, virtual_key_code);
}

bool InputState::is_key_pressed(uint8_t virtual_key_code) const {
    return get_bit(state_bitmap, virtual_key_code) && !get_bit(prev_bitmap, virtual_key_code);
}

void InputState::print_down_keys() const {
    for (const auto& [code, name] : virtual_keys) {
        if (is_key_held(code)) {
            logger.debug("Key down: {}.", name);
        }
    }
}
