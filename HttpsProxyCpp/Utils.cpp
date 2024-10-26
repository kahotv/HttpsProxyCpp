#include "stdafx.h"
#include "Utils.h"


bool Utils::ReadFileBytes(IN const std::string& filepath, OUT std::string& data)
{
	std::ifstream fs;

	fs.open(filepath, std::ios_base::in | std::ios_base::binary);

	if (!fs.is_open())
	{
		return false;
	}

	int len = 0;

	fs.seekg(0, std::ios::end);
	len = (int)fs.tellg();
	fs.seekg(0, std::ios::beg);
	data.resize(len);
	fs.read(&data[0], len);
	fs.close();

	return true;
}
bool Utils::WriteFileBytes(IN const std::string& filepath, IN const std::string& data)
{
	std::ofstream fs;

	fs.open(filepath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);

	if (!fs.is_open())
	{
		return false;
	}

	fs.write(data.data(), data.length());
	fs.flush();
	fs.close();

	return true;
}
bool Utils::WriteFileBytes(IN const std::string& filepath, IN const char* data, IN size_t len)
{
	std::ofstream fs;

	fs.open(filepath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);

	if (!fs.is_open())
	{
		return false;
	}

	fs.write(data, len);
	fs.flush();
	fs.close();

	return true;
}

auto Utils::Split(IN const std::string& in, IN const std::string& delim, IN bool removeEmptyItem)
-> std::vector<std::string>
{
	std::vector<std::string> ret;

	if (!in.empty())
	{
		if (delim.empty())
		{
			ret.emplace_back(in);
			return ret;
		}

		size_t delime_size = delim.size();;
		int last = 0;

		size_t total = in.size();
		const char* begin = in.c_str();

		for (size_t pos = 0; pos < total;)
		{
			pos = in.find(delim, pos);
			if (pos == std::string::npos)
			{
				//没搜到
				ret.emplace_back(begin + last, total - last);
				break;
			}
			else
			{
				//搜到了
				if (pos - last > 0)
				{
					ret.emplace_back(begin + last, pos - last);
				}
				else if (pos == last && !removeEmptyItem)
				{
					ret.emplace_back();
				}
				pos = pos + delime_size;
				last = pos;
			}

		}

		if (in.ends_with(delim))
		{
			ret.emplace_back();
		}
	}

	return ret;
}


auto Utils::Join(IN const std::vector<std::string>& list, IN const std::string joinstr) -> std::string
{
	return list
		| std::views::join_with(joinstr)
		| std::ranges::to<std::string>();
}

auto Utils::Join(IN const std::list<std::string>& list, IN const std::string joinstr) -> std::string
{
	return list
		| std::views::join_with(joinstr)
		| std::ranges::to<std::string>();
}

auto Utils::Trim(IN const std::string& str) -> std::string
{
	auto iswhitespace = [](char c)
		{ return Utils::Any(c, ' ', '\r', '\n', '\t', '\0'); };
	auto ret = str
		| std::views::drop_while(iswhitespace)
		| std::views::reverse
		| std::views::drop_while(iswhitespace)
		| std::views::reverse
		| std::ranges::to<std::string>();

	return ret;
}

auto Utils::Trim(IN const std::wstring& str) -> std::wstring
{
	auto iswhitespace = [](wchar_t c)
		{ return Utils::Any(c, L' ', L'\r', L'\n', L'\t', L'\0'); };
	auto ret = str
		| std::views::drop_while(iswhitespace)
		| std::views::reverse
		| std::views::drop_while(iswhitespace)
		| std::views::reverse
		| std::ranges::to<std::wstring>();

	return ret;
}

//移除首尾空格，并转换到小写
auto Utils::TrimAndToLower(IN const std::string& str) -> std::string
{
	std::string tmp = Utils::Trim(str);

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);

	return tmp;
}

//移除首尾空格，并转换到小写
auto Utils::TrimAndToLower(IN const std::wstring& str) -> std::wstring
{
	std::wstring tmp = Utils::Trim(str);

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::towlower);

	return tmp;
}

auto Utils::Bin2Hex(IN const void* buf, IN int len, IN const char* delimiter) -> std::string
{
	if (buf == nullptr || len <= 0)
		return "";

	return std::span((const unsigned char*)buf, len)
		| std::views::transform([](const unsigned char c) -> std::string { return std::format("{:02X}", c); })
		| std::views::join_with(std::string(delimiter))
		| std::ranges::to<std::string>();
}