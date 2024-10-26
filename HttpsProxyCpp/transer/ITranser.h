#pragma once
class iTranser
{
public:
	template<typename StreamType>
	auto Trans(const std::string host,
		StreamType& local, StreamType& remote) -> awaitable<void>
	{
		co_return;
	}
};
using iTranserPtr = std::shared_ptr<iTranser>;
