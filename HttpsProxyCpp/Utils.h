#pragma once
class Utils
{
public:
	template<typename T, typename ... Args>
	static bool Any(T x, Args&& ... args) noexcept
	{
		return ((x == args) || ...);
	}

	template<typename T, typename ... Args>
	static bool All(T x, Args&& ... args) noexcept
	{
		return ((x == args) && ...);
	}

	// return (begin <= val && val <= end)
	template<typename T>
	static bool Between(T begin, T val, T end) noexcept
	{
		return begin <= val && val <= end;
	}
public:
	static bool ReadFileBytes(IN const std::string& filepath, OUT std::string& data);
	static bool WriteFileBytes(IN const std::string& filepath, IN const std::string& data);
	static bool WriteFileBytes(IN const std::string& filepath, IN const char* data, IN size_t len);

	static auto Split(IN const std::string& in, IN const std::string& delim, IN bool removeEmptyItem) -> std::vector<std::string>;
	static auto Join(IN const std::vector<std::string>& list, IN const std::string joinstr) -> std::string;
	static auto Join(IN const std::list<std::string>& list, IN const std::string joinstr) -> std::string;

	static auto Trim(IN const std::string& str) -> std::string;				//移除首尾空格
	static auto Trim(IN const std::wstring& str) -> std::wstring;			//移除首尾空格
	static auto TrimAndToLower(IN const std::string& str) -> std::string;	//移除首尾空格，并转换到小写
	static auto TrimAndToLower(IN const std::wstring& str) -> std::wstring;	//移除首尾空格，并转换到小写

	static auto Bin2Hex(IN const void* buf, IN int len, IN const char* delimiter = "") -> std::string;

	static auto ReadHeadersPeek(tcp::socket& sock, bool peek) -> awaitable<std::string>
	{
		std::string ret;
		size_t nowPos = 0;		//当前pos
		size_t stepSize = 1024;	//每次扩张多少

		try
		{
			while (true)
			{
				if (nowPos >= ret.length()) {
					//不够了，扩充
					ret.resize(ret.length() + stepSize);
				}

				nowPos = co_await sock.async_receive(buffer(&ret[0] + nowPos, ret.length() - nowPos), socket_base::message_peek, use_awaitable);

				//扫一下有没有\r\n\r\n
				auto findPos = ret.find("\r\n\r\n");
				if (findPos == std::string::npos) {
					continue;
				}

				ret.resize(findPos + 4);

				if (!peek)
				{
					//消耗对应的数量
					co_await asio::async_read(sock, buffer(&ret[0], ret.length()), use_awaitable);
				}

				break;
			}
		}
		catch (const std::exception&)
		{
			ret.clear();
		}

		co_return ret;


		// 会多读数据，不能用
		//std::string ret;
		//asio::streambuf buf;
		//size_t n = co_await asio::async_read_until(sock, buf, "\r\n\r\n", use_awaitable);

		//std::istream is(&buf);

		//ret.resize(n);
		//is.read(&ret[0], n);

		//co_return ret;
	}

	template<typename StreamType>
	static auto ReadHeadersDontPeek(StreamType& stream, size_t max) -> awaitable<std::string>
	{
		//由于ssl::stream<tcp::socket>好像不支持peek，只能每次只读一个字节然后判断一下末尾
		const int step = 1024;
		size_t pos = 0;
		std::string ret;

		std::string pattern = "\r\n\r\n";

		while (true)
		{
			if (pos >= ret.size()) {
				if (pos >= max) {
					co_return "";
				}

				ret.resize(ret.size() + step);
			}

			co_await asio::async_read(stream, buffer(&ret[pos], 1), use_awaitable);

			pos += 1;
			//直接判断末尾
			if (pos < pattern.size()) {
				continue;
			}

			bool find = true;
			size_t spos = pos - pattern.size();
			for (size_t i = 0; i < pattern.size(); i++) {
				if (ret[spos + i] != pattern[i]) {
					find = false;
					break;
				}
			}

			if (!find) {
				continue;
			}

			ret.resize(pos);

			co_return ret;
		}
	}
};

