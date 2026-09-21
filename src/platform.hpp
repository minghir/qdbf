#ifndef QDBF_PLATFORM_HPP
#define QDBF_PLATFORM_HPP

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <algorithm>
#include <codecvt>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <locale>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using HINSTANCE = void*;
using SOCKET = int;
using WORD = unsigned short;
using DWORD = unsigned long;
using UINT = unsigned int;
using errno_t = int;

constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
constexpr int SD_BOTH = SHUT_RDWR;
constexpr int SW_SHOW = 1;
constexpr WORD FOREGROUND_BLUE = 1;
constexpr WORD FOREGROUND_GREEN = 2;
constexpr WORD FOREGROUND_RED = 4;
constexpr WORD FOREGROUND_INTENSITY = 8;
constexpr WORD BACKGROUND_RED = 64;
constexpr int MAX_PATH = 4096;
constexpr unsigned int CP_UTF8 = 65001;
constexpr int _TRUNCATE = -1;
constexpr int WSAEWOULDBLOCK = EWOULDBLOCK;

struct WSADATA {};
inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
inline int WSACleanup() { return 0; }
inline int WSAGetLastError() { return errno; }
inline int MAKEWORD(unsigned char, unsigned char) { return 0; }
inline int closesocket(SOCKET socket) { return close(socket); }

inline int localtime_s(std::tm* result, const std::time_t* time) {
	return localtime_r(time, result) == nullptr ? 1 : 0;
}

inline errno_t strncpy_s(char* destination, size_t destinationSize,
						 const char* source, int) {
	if (destinationSize == 0) return EINVAL;
	std::strncpy(destination, source, destinationSize - 1);
	destination[destinationSize - 1] = '\0';
	return 0;
}

inline int MultiByteToWideChar(unsigned int, unsigned long, const char* source,
							   int sourceLength, wchar_t* destination,
							   int destinationLength) {
	const std::string input(source, sourceLength < 0 ? std::strlen(source) :
														static_cast<size_t>(sourceLength));
	std::wstring converted = std::wstring_convert<
		std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(input);
	const int required = static_cast<int>(converted.size()) + (sourceLength < 0 ? 1 : 0);
	if (destination == nullptr || destinationLength == 0) return required;
	const int count = std::min(destinationLength, required);
	std::copy_n(converted.c_str(), std::min<int>(count, converted.size()), destination);
	if (sourceLength < 0 && count > 0) destination[count - 1] = L'\0';
	return required;
}

inline int WideCharToMultiByte(unsigned int, unsigned long, const wchar_t* source,
							   int sourceLength, char* destination,
							   int destinationLength, const char*, bool*) {
	const std::wstring input(source, sourceLength);
	const std::string converted = std::wstring_convert<
		std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(input);
	if (destination == nullptr || destinationLength == 0) return static_cast<int>(converted.size());
	const int count = std::min(destinationLength, static_cast<int>(converted.size()));
	std::copy_n(converted.data(), count, destination);
	return static_cast<int>(converted.size());
}
#endif

#endif
