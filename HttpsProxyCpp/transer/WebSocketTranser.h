#pragma once

class WebSocketTranser
	: public IObjCounter<WebSocketTranser>
	, public iTranser
{
public:
	template<typename StreamType>
	auto Trans(const std::string host,
		StreamType& local, StreamType& remote) -> awaitable<void>
	{
		//printf("[%s] WebSocket Trans begin\n", host.c_str());
		
		//读取HTTP Response
		auto headers = co_await Utils::ReadHeadersDontPeek(remote, 0x1000);
		//printf("[%s] WebSocket Trans response header:\n%s\n", host.c_str(), headers.c_str());
		//写入HTTP Response
		co_await asio::async_write(local, buffer(headers), use_awaitable);

		//接下来都是WebSocket数据
		co_await(TransWebsocket(host, local, remote, true) || TransWebsocket(host, remote, local, false));
		//printf("[%s] WebSocket Trans end\n", host.c_str());
		co_return;
	}
private:

	void TransWebsocketData(const std::string& host, IN OUT std::string& data, bool up)
	{
		const char* dir = up ? "WSRequest :" : "WSResponse:";
		printf("%s %s\n", dir, Utils::Bin2Hex(data.data(), data.length()," ").c_str());
	}


	template<typename StreamType>
	auto TransWebsocket(const std::string host,
		StreamType& from, StreamType& to, bool up) -> awaitable<void>
	{
        /*
             0               1               2               3
             0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            +-+-+-+-+-------+-+-------------+-------------------------------+
            |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
            |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
            |N|V|V|V|       |S|             |   (if payload len==126/127)   |
            | |1|2|3|       |K|             |                               |
            +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
            |     Extended payload length continued, if payload len == 127  |
            + - - - - - - - - - - - - - - - +-------------------------------+
            |                               |Masking-key, if MASK set to 1  |
            +-------------------------------+-------------------------------+
            | Masking-key (continued)       |          Payload Data         |
            +-------------------------------- - - - - - - - - - - - - - - - +
            :                     Payload Data continued ...                :
            + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
            |                     Payload Data continued ...                |
            +---------------------------------------------------------------+
        */

		std::string buf;

        while (true)
        {
			/* 1. request */
			u8 wsFlagAndOpcode = 0;
			u8 wsMaskAndLen = 0;
			u32 len = 0;
			co_await asio::async_read(from, buffer(&wsFlagAndOpcode, 1), use_awaitable);
			co_await asio::async_read(from, buffer(&wsMaskAndLen, 1), use_awaitable);

			if ((wsMaskAndLen & 0x7F) == 126) {
				u16 len16 = 0;
				co_await asio::async_read(from, buffer(&len16, sizeof(len16)), use_awaitable);
				len = ntohs(len16);
			}
			else if ((wsMaskAndLen & 0x7F) == 127) {
				u64 len64 = 0;
				co_await asio::async_read(from, buffer(&len64, sizeof(len64)), use_awaitable);
				len64 = ntohll(len64);
				if (len64 >  0xFFFFFFFFul) {
					printf("暂时不支持超大包\n");
					break;
				}
				len = (u32)len64;
			}
			else {
				len = wsMaskAndLen & 0x7F;
			}

			bool havePassword = (wsMaskAndLen & 0x80) == 0x80;
			u32 pwd = 0;
			if (havePassword) {
				co_await asio::async_read(from, buffer(&pwd, sizeof(pwd)), use_awaitable);
			}

			//扩展到4的倍数方便xor
			u32 tmplen = len + (4 - (len % 4));
			buf.resize(tmplen);
			co_await asio::async_read(from, buffer(&buf[0], len), use_awaitable);

			if (havePassword) {
				u32* buf2 = (u32*)&buf[0];

				for (u32 i = 0; i < tmplen / 4; i++) {
					buf2[i] ^= pwd;
				}
			}

			/* 2. process */
			buf.resize(len);
			TransWebsocketData(host, buf, up);

			/* 3. response */
			wsMaskAndLen &= 0x7F;	//set pwd=0
			co_await asio::async_write(to, buffer(&wsFlagAndOpcode, 1), use_awaitable);
			co_await asio::async_write(to, buffer(&wsMaskAndLen, 1), use_awaitable);
			if ((wsMaskAndLen & 0x7F) == 126) {
				u16 len16 = (u16)len;
				len16 = htons(len16);
				co_await asio::async_write(to, buffer(&len16, 16 / 8), use_awaitable);
			}
			else if ((wsMaskAndLen & 0x7F) == 127) {
				u64 len64 = len;
				len64 = htonll(len64);
				co_await asio::async_write(to, buffer(&len64, 64 / 8), use_awaitable);
			}

			co_await asio::async_write(to, buffer(buf), use_awaitable);
		}


		co_return;
	}
};
