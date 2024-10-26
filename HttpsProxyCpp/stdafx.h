#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WIN32_WINDOWS 0x0602

//STL
#include <iostream>
#include <format>
#include <memory>
#include <fstream>
#include <map>
#include <tuple>
#include <ranges>
#include <filesystem>
#include <shared_mutex>
#include <functional>

//系统
#pragma comment(lib,"Crypt32.lib")

//三方
#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/ssl.hpp>
#include <openssl/x509.h>

using namespace asio;
using namespace asio::ip;
using namespace asio::experimental;
using namespace asio::experimental::awaitable_operators;

using my_channel = concurrent_channel<void(asio::error_code, std::shared_ptr<std::string>)>;
using my_channel_ptr = std::shared_ptr<my_channel>;

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

template<typename T>
class IObjCounter
{
public:
	IObjCounter()
	{
		ULONG id = InterlockedIncrement(&m_obj_counter);
		printf("%s", std::format("{:<15} new  {:x} | {}\n", typeid(T).name(), (u32)(this), id).c_str());
	}
	virtual ~IObjCounter()
	{
		ULONG id = InterlockedDecrement(&m_obj_counter);
		printf("%s", std::format("{:<15} free {:x} | {}\n", typeid(T).name(), (u32)(this), id).c_str());
	}
	static void ShowCounter()
	{
		printf("%s", std::format("{:<15} load |{}\n", typeid(T).name(), m_obj_counter).c_str());
	}
private:
	static ULONG m_obj_counter;
};

template<typename T>
_declspec(selectany) ULONG IObjCounter<T>::m_obj_counter = 0;

//自己
#include "Utils.h"
#include "transer/ITranser.h"
#include "transer/SimpleTranser.h"
#include "transer/WebSocketTranser.h"
