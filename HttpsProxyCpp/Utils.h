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

	static auto Trim(IN const std::string& str) -> std::string;				//�Ƴ���β�ո�
	static auto Trim(IN const std::wstring& str) -> std::wstring;			//�Ƴ���β�ո�
	static auto TrimAndToLower(IN const std::string& str) -> std::string;	//�Ƴ���β�ո񣬲�ת����Сд
	static auto TrimAndToLower(IN const std::wstring& str) -> std::wstring;	//�Ƴ���β�ո񣬲�ת����Сд

	static auto Bin2Hex(IN const void* buf, IN int len, IN const char* delimiter = "") -> std::string;

	static auto ReadHeadersPeek(tcp::socket& sock, bool peek) -> awaitable<std::string>
	{
		std::string ret;
		size_t nowPos = 0;		//��ǰpos
		size_t stepSize = 1024;	//ÿ�����Ŷ���

		try
		{
			while (true)
			{
				if (nowPos >= ret.length()) {
					//�����ˣ�����
					ret.resize(ret.length() + stepSize);
				}

				nowPos = co_await sock.async_receive(buffer(&ret[0] + nowPos, ret.length() - nowPos), socket_base::message_peek, use_awaitable);

				//ɨһ����û��\r\n\r\n
				auto findPos = ret.find("\r\n\r\n");
				if (findPos == std::string::npos) {
					continue;
				}

				ret.resize(findPos + 4);

				if (!peek)
				{
					//���Ķ�Ӧ������
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


		// �������ݣ�������
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
		//����ssl::stream<tcp::socket>����֧��peek��ֻ��ÿ��ֻ��һ���ֽ�Ȼ���ж�һ��ĩβ
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
			//ֱ���ж�ĩβ
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

