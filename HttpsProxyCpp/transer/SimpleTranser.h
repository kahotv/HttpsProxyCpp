#pragma once

class SimpleTranser
	: public IObjCounter<SimpleTranser>
	, public iTranser
{
public:
	// Í¨¹ý iTranser ¼Ì³Ð
	template<typename StreamType>
	auto Trans(const std::string host,
		StreamType& local, StreamType& remote) -> awaitable<void>
	{
		co_await(TransSimple(host, local, remote, true) || TransSimple(host, remote, local, false));
		co_return;
	}

	template<typename StreamType>
	auto TransSimple(const std::string host,
		StreamType& from, StreamType& to, bool up) -> awaitable<void>
	{
		char buf[1024];
		try
		{
			while (true) {

				size_t n = 0;

				//printf("[%s] read begin\n", host.c_str());
				n = co_await from.async_read_some(buffer(buf, sizeof(buf)), use_awaitable);
				//printf("[%s] read end, size: %d\n", host.c_str(), n);
				n = co_await asio::async_write(to, buffer(buf, n), use_awaitable);

				//printf("[%s][%s] [%s -> %s] len %d\n", host.c_str(), link.c_str(), strFrom.c_str(), strTo.c_str(), n);

			}
		}
		catch (const std::exception&)
		{
			//printf("[%s] local2remote error: %s\n", link.c_str(), e.what());
		}
		co_return;
	}
};
