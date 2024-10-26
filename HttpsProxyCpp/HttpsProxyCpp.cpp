#include "stdafx.h"
#include "CertHelper.h"

CertHelper g_certHepler;

std::string g_pathServerPrivateKey	= "";

/*
	HTTP 其实就是http连接 但是要处理一下"Proxy-Connection"，一般是替换成"Connection: close"

	Request
		GET http://c.pki.goog/r/r1.crl HTTP/1.1\r\n
		Cache-Control: max-age = 3000\r\n
		Proxy-Connection: Keep-Alive\r\n
		Accept: * / *\r\n
		If-Modified-Since: Thu, 25 Jul 2024 14:48:00 GMT\r\n
		User-Agent: Microsoft-CryptoAPI/10.0\r\n
		Host: c.pki.goog\r\n
		\r\n
	Response
		...
*/

/*
	HTTPS 开头明文的HTTP请求，并且是CONNECT。然后才是建立SSL，成功后基于SSL进行真正的HTTP通信。

	Request
		CONNECT accounts.google.com:443 HTTP/1.1\r\n
		Host: accounts.google.com:443\r\n
		Proxy-Connection: keep-alive\r\n
		User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36\r\n
		\r\n
	Response
		HTTP/1.1 200 Connection established\r\n
		\r\n

*/

auto GetHostByHeaders(std::string headers) -> std::tuple<std::string, WORD>
{
	//优先取host字段，如果没有再说

	std::string host = "";
	u16 port = 0;

	headers = Utils::TrimAndToLower(headers);
	auto v = Utils::Split(headers, "\n", true);
	for (auto& kv : v)
	{
		kv = Utils::Trim(kv);
		
		auto vv = Utils::Split(kv, ":", true);
		if (vv[0] != "host") {
			continue;
		}

		if (vv.size() == 3) {
			host = Utils::Trim(vv[1]);
			port = std::stoul(vv[2]);
			break;
		}
		else if (vv.size() == 2) {
			//没有端口
			host = Utils::Trim(vv[1]);
			break;
		}
	}

	//分析第一行拿到端口
	if (port == 0 && v.size() > 0) {
		//GET http://c.pki.goog:80/r/r1.crl HTTP/1.1
		auto vv = Utils::Split(v[0], " ", true);
		if (vv.size() == 3) {
			auto vvv = Utils::Split(vv[1], ":", true);
			if (vvv.size() >= 2) {
				if (vvv[0].starts_with("http")) {
					//GET http(s)://www.baidu.com:8888/asdaxzczx HTTP/1.1
					if (vvv.size() >= 3) {
						auto vvvv = Utils::Split(vvv[2], "/", true);
						port = std::stoul(vvvv[0]);
					}
				}
				else {
					//GET www.baidu.com:8888/asdaxzczx HTTP/1.1
					auto vvvv = Utils::Split(vvv[1], "/", true);
					port = std::stoul(vvvv[0]);
				}
			}

			if(port == 0)
			{
				//没有端口，由协议决定
				if (vv[1].starts_with("http://")) {
					port = 80;
				}
				else if (vv[1].starts_with("https://")) {
					port = 443;
				}
			}
		}
	}

	return std::make_tuple(host, port);
}


auto ConnectAsync(const std::string& addr, WORD port) -> awaitable<std::tuple<error_code, std::optional<tcp::socket>>>
{
	auto ctx = co_await this_coro::executor;
	tcp::resolver rsv(ctx);

	auto [ec, epList] = co_await rsv.async_resolve(addr, std::format("{}", port), as_tuple(use_awaitable));
	if (ec) {
		printf("解析域名[%s]出错: %s\n", addr.c_str(), ec.message().c_str());
		co_return std::make_tuple(ec, std::nullopt);
	}
	if (epList.size() == 0) {
		printf("解析域名[%s]出错: 没有IP\n", addr.c_str());
		co_return std::make_tuple(asio::error::make_error_code(asio::error::basic_errors::host_unreachable), std::nullopt);
	}
	//printf("解析域名[%s]成功: 有%d个IP\n", addr.c_str(), epList.size());

	tcp::socket sockRemote(ctx);

	auto [ec2, ep] = co_await asio::async_connect(sockRemote, epList, as_tuple(use_awaitable));
	if (ec2) {
		printf("连接[%s]失败: %s\n", addr.c_str(), ec2.message().c_str());
		co_return std::make_tuple(ec, std::nullopt);
	}
	//printf("连接[%s]成功: 链路 %s:%d -> %s:%d\n", addr.c_str(),
	//	sockRemote.local_endpoint().address().to_string().c_str(),
	//	sockRemote.local_endpoint().port(),
	//	ep.address().to_string().c_str(), ep.port());
	co_return std::make_tuple(error_code{}, std::move(sockRemote));
}

bool IsUpgradeWebSocket(const std::string& headers)
{
	std::string header_connection = "";
	std::string header_upgrade = "";
	auto list = Utils::Split(Utils::TrimAndToLower(headers), "\n", true);
	for(auto& item : list)
	{
		item = Utils::Trim(item);
		auto kv = Utils::Split(item, ":", true);
		if (kv.size() == 2)
		{
			auto key = Utils::Trim(kv[0]);
			if (key == "connection")
			{
				header_connection = Utils::Trim(kv[1]);
			}
			else if (key == "upgrade")
			{
				header_upgrade = Utils::Trim(kv[1]);
			}
		}
	}

	if (header_connection == "upgrade"
		&& header_upgrade == "websocket")
	{
		return true;
	}
	return false;
}

auto ReplaceHttpHeader(const std::string& headers, std::string findHeader, std::string newHeaderValue) -> std::string
{
	findHeader = Utils::TrimAndToLower(findHeader);

	auto v = Utils::Split(headers, "\n", false);
	for (auto& item : v) {

		item = Utils::Trim(item);

		auto tmp = Utils::TrimAndToLower(item);
		if (tmp.starts_with(findHeader)) {
			item = newHeaderValue;
		}
	}

	auto newHeaders = Utils::Join(v, "\r\n");

	return newHeaders;
}


template<typename StreamType>
auto ConsumeHeaderAndProcessProxyConnection(const std::string& host, StreamType& local, StreamType& remote) -> awaitable<std::string>
{
	auto headers = co_await Utils::ReadHeadersDontPeek(local, 0x1000);

	//printf("[%s] header: \n%s\n", host.c_str(), headers.c_str());

	//处理http的Proxy-Connection
	if (std::is_same<StreamType, tcp::socket>::value) {
		headers = ReplaceHttpHeader(headers,"proxy-connection","Connection: close");
	}

	co_await asio::async_write(remote, buffer(headers), use_awaitable);

	co_return headers;
}


/// <summary>
/// 预处理，消耗掉header
/// </summary>
/// <typeparam name="StreamType"></typeparam>
/// <param name="host"></param>
/// <param name="local"></param>
/// <param name="remote"></param>
/// <returns></returns>
template<typename StreamType>
auto PreTransactAsync(const std::string host,
	StreamType& streamLocal, StreamType& streamRemote) -> awaitable<void>
{
	//printf("[%s] PreTransactAsync begin\n", host.c_str());

	std::string headers = co_await ConsumeHeaderAndProcessProxyConnection(host, streamLocal, streamRemote);
	//printf("[%s] PreTransactAsync end | \n%s\n", host.c_str(),headers.c_str());

	if (IsUpgradeWebSocket(headers)) {
		auto transer = std::make_shared<WebSocketTranser>();
		co_await transer->Trans(host, streamLocal, streamRemote);
	}
	else {
		auto transer = std::make_shared<SimpleTranser>();
		co_await transer->Trans(host, streamLocal, streamRemote);
	}
	
	co_return;
}

auto HandleClientAsync(tcp::socket sockLocal) -> awaitable<void>
{
	std::string strLink = std::format("{}:{} -> {}:{}",
		sockLocal.remote_endpoint().address().to_string(),
		sockLocal.remote_endpoint().port(),
		sockLocal.local_endpoint().address().to_string(),
		sockLocal.local_endpoint().port());

	bool first = true;
	do
	{
		std::string headers = co_await Utils::ReadHeadersPeek(sockLocal, true);
		if (headers.empty()) {
			printf("未知的协议: %s\n", headers.c_str());
			break;
		}

		bool isHttps = headers.starts_with("CONNECT");

		//1. 解析出host
		auto [host, port] = GetHostByHeaders(headers);
		if (host.empty()) {
			break;
		}
		if (port == 0) {
			port = isHttps ? 443 : 80;
		}

		//2. 连接真正的远端
		auto [ec, opt] = co_await ConnectAsync(host, port);
		if (!opt) {
			printf("connect %s %s:%d error: %s\n", 
				isHttps ? "https" : "http",
				host.c_str(), port, ec.message().c_str());
			break;
		}
		printf("connect %s %s:%d succ\n",
			isHttps ? "https" : "http", host.c_str(), port);

		tcp::socket sockRemote = std::move(opt.value());

		//3. https额外处理
		if (isHttps) {
			
			//3.1 消耗代理rquest header，没有data
			co_await Utils::ReadHeadersPeek(sockLocal, false);

			//3.2 返回代理成功
			std::string respFirst = "HTTP/1.1 200 Connection established\r\n\r\n";
			co_await asio::async_write(sockLocal, buffer(respFirst),use_awaitable);

			//3.3 建立ssl

			//3.3.1 与remote建立ssl
			ssl::context sslContextRemote(ssl::context::tlsv12);
			ssl::stream<tcp::socket> sslStreamRemote(std::move(sockRemote), sslContextRemote);
			sslStreamRemote.set_verify_mode(ssl::verify_none);
			co_await sslStreamRemote.async_handshake(ssl::stream_base::handshake_type::client, use_awaitable);

			//3.3.2 与local建立ssl (创建的serverCert不用释放，g_certHepler内部有生命周期管理)
			auto serverCert = g_certHepler.createServerCert(host);
			if (serverCert == NULL) {
				printf("createServerCert faild: %s\n", host.c_str());
				break;
			}
			std::string certServerPEM = CertHelper::PEM_FormX509(serverCert);
			ssl::context sslContextLocal(ssl::context::tlsv12);
			asio::error_code ecc;
			sslContextLocal.use_private_key_file(g_pathServerPrivateKey, ssl::context_base::file_format::pem, ecc);
			if (ecc) {
				printf("use_private_key_file error: %s\n", ecc.message().c_str());
				break;
			}
			sslContextLocal.use_certificate(asio::buffer(certServerPEM), ssl::context_base::file_format::pem, ecc);
			ssl::stream<tcp::socket> sslStreamLocal(std::move(sockLocal), sslContextLocal);
			sslStreamLocal.set_verify_mode(ssl::verify_none);
			auto [ec2] = co_await sslStreamLocal.async_handshake(ssl::stream_base::handshake_type::server, as_tuple(use_awaitable));

			//转发数据
			co_await PreTransactAsync(host, sslStreamLocal, sslStreamRemote);
		}
		else {
			//转发数据
			co_await PreTransactAsync(host, sockLocal, sockRemote);
		}

	} while (false);
}


auto listen(WORD port) -> awaitable<void>
{
	try
	{
		auto ctx = co_await this_coro::executor;
		tcp::acceptor sock(ctx, tcp::v4());
		sock.bind({ address_v4::any(),port });
		sock.listen();
		printf("listen %s:%d\n",
			sock.local_endpoint().address().to_string().c_str(),
			sock.local_endpoint().port());

		while (true)
		{
			auto cliSock = co_await	sock.async_accept(use_awaitable);
			//printf("accept %s:%d\n",
			//	cliSock.remote_endpoint().address().to_string().c_str(),
			//	cliSock.remote_endpoint().port());

			co_spawn(ctx, HandleClientAsync(std::move(cliSock)), detached);
		}
	}
	catch (const std::exception& e)
	{
		printf("listen error: %s\n", e.what());
	}

}



void StartServer(u16 port)
{
	//监听8000端口
	io_context ctx;
	co_spawn(ctx, listen(port), detached);
	ctx.run();
}

std::string FixPath(const std::string& path)
{
	namespace fs = std::filesystem;

	if (fs::exists(path)) {
		return path;
	}
	else
	{
		auto name = fs::path(path).filename();
		if (fs::exists(name)) {
			return name.generic_string();
		}
	}

	return "";
}

int main()
{
	//读取预先准备好的CA、CAKey、ServerKey

	auto pathCaCert = FixPath("../ca.crt");
	auto pathCaPrivateKey = FixPath("../ca.key");
	g_pathServerPrivateKey = FixPath("../server.key");

	bool b = g_certHepler.init(pathCaCert.c_str(), pathCaPrivateKey.c_str(), g_pathServerPrivateKey.c_str());
	printf("加载证书: %s\n", b ? "成功" : "失败");
	if (!b) {
		return -1;
	}

	StartServer(8000);

	return 0;
}

