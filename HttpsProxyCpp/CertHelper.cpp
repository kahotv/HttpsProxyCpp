#include "stdafx.h"
#include "CertHelper.h"


bool CertHelper::init(
	const char* caCertFile,
	const char* caKeyFile,
	const char* serverKeyFile)
{
	m_caCert = PEM_ReadX509(caCertFile);
	m_caKey = PEM_ReadPKEY(caKeyFile);
	m_serverKey = PEM_ReadPKEY(serverKeyFile);

	if (m_caCert == NULL) {
		printf("m_caCert: NULL\n");
	}

	if (m_caKey == NULL) {
		printf("m_caKey: NULL\n");
	}

	if (m_serverKey == NULL) {
		printf("m_serverKey: NULL\n");
	}

	//printf("m_caCert: %x, m_caKey: %x, m_serverKey: %x\n", m_caCert, m_caKey, m_serverKey);

	return m_caCert != NULL && m_caKey != NULL && m_serverKey != NULL;
}
void CertHelper::free()
{
	X509_free(m_caCert);
	EVP_PKEY_free(m_caKey);
	EVP_PKEY_free(m_serverKey);

	m_caCert = NULL;
	m_caKey = NULL;
	m_serverKey = NULL;
}
X509* CertHelper::createServerCert(const std::string& host)
{
	return CreateCertByCA(host, m_caCert, m_caKey, m_serverKey);
}
auto CertHelper::PEM_ReadX509(const char* filepath) -> X509*
{
	X509* ret = NULL;

	std::string strPEM;
	if (Utils::ReadFileBytes(filepath, strPEM))
	{
		BIO* in = BIO_new_mem_buf(strPEM.data(), strPEM.length());
		if (in != NULL) {
			PEM_read_bio_X509(in, &ret, NULL, NULL);
			BIO_free(in);
		}
	}

	//printf("PEM_ReadX509 %s => %x\n", filepath, ret);

	return ret;
}
auto CertHelper::PEM_ReadPKEY(const char* filepath) -> EVP_PKEY*
{
	EVP_PKEY* ret = NULL;

	std::string strPEM;
	if (Utils::ReadFileBytes(filepath, strPEM))
	{
		BIO* in = BIO_new_mem_buf(strPEM.data(), strPEM.length());
		if (in != NULL) {
			PEM_read_bio_PrivateKey(in, &ret, NULL, NULL);
			BIO_free(in);
		}
	}
	//printf("PEM_ReadPKEY %s => %x\n", filepath, ret);

	return ret;
}
void CertHelper::PEM_WriteX509(const char* filepath, const X509* x509)
{
	BIO* out = BIO_new_file(filepath, "wb");
	if (out != NULL)
	{
		PEM_write_bio_X509(out, x509);
		BIO_flush(out);
		BIO_free(out);
	}
}
auto CertHelper::PEM_FormX509(const X509* x509) -> std::string
{
	BIO* out = BIO_new(BIO_s_mem());
	if (out != NULL)
	{
		PEM_write_bio_X509(out, x509);
		BIO_flush(out);

		BUF_MEM* mem = NULL;
		BIO_get_mem_ptr(out, &mem);

		std::string ret;
		ret.resize(mem->length);
		memcpy(&ret[0], mem->data, ret.size());
		BIO_free(out);
		return ret;
	}
	return "";
}
auto CertHelper::CreateSNO() -> ASN1_INTEGER*
{
	static long counter = 0x1;

	uint64_t now = _InterlockedIncrement(&counter);
	ASN1_INTEGER* sno = ASN1_INTEGER_new();
	ASN1_INTEGER_set_uint64(sno, now);
	return sno;
}
auto CertHelper::CreateExtDNS(X509* cert, const std::string& host) -> X509_EXTENSION*
{
	auto list_opt = CreateDnsList(host);
	if (!list_opt) {
		return NULL;
	}

	//abc,def => DNS.1:abc,DNS.2:def
	int i = 1;
	const std::string joinstr = ",";
	std::string dns_ext = list_opt.value()
		| std::views::transform([&i](const std::string& name) { return std::format("DNS.{}:{}", i++, name); })
		| std::views::join_with(joinstr)
		| std::ranges::to<std::string>();

	X509V3_CTX ctx;
	X509V3_set_ctx_nodb(&ctx);
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
	X509_EXTENSION* ext = X509V3_EXT_conf_nid(NULL, &ctx, NID_subject_alt_name, dns_ext.c_str());
	return ext;
}
auto CertHelper::CreateCertByCA(const std::string& host, const X509* ca, EVP_PKEY* ca_key, EVP_PKEY* server_key) -> X509*
{
	static std::map<std::string, X509*> certlist;
	static std::shared_mutex certlocker;

	//printf("CreateCertByCA [%s] begin\n", host.c_str());
	X509* ret = NULL;
	{
		std::lock_guard<std::shared_mutex> lock(certlocker);

		auto& tmp = certlist[host];
		if (tmp == NULL)
		{
			//printf("CreateCertByCA [%s] new begin | cert count: %d\n", host.c_str(), certlist.size() - 1);

			if (1)
			{
				EVP_PKEY* pkey = server_key;

				X509* cert = X509_new();

				//修改CN为指定的host
				X509_NAME* name = X509_NAME_dup(X509_get_subject_name(ca));
				X509_NAME_ENTRY* tmpname = X509_NAME_delete_entry(name, X509_NAME_get_index_by_NID(name, NID_commonName, -1));
				X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)host.c_str(), host.length(), -1, 0);
				X509_NAME_ENTRY_free(tmpname);

				X509_set_subject_name(cert, name);
				X509_set_pubkey(cert, pkey);
				X509_set_serialNumber(cert, CreateSNO());
				X509_set_notBefore(cert, X509_get_notBefore(ca));
				X509_set_notAfter(cert, X509_get_notAfter(ca));
				X509_set_issuer_name(cert, X509_get_subject_name(ca));
				X509_set_version(cert, X509_VERSION_3);
				X509_add_ext(cert, CreateExtDNS(cert, host), -1);
				X509_sign(cert, ca_key, EVP_sha256());
				//X509_verify(cert, ca_key);

				tmp = cert;	//缓存
				ret = cert;
			}
			//printf("CreateCertByCA [%s] new end | ret=%p\n", host.c_str(), ret);
		}
		else
		{
			ret = tmp;
			//printf("CreateCertByCA [%s] use cache | ret=%p\n", host.c_str(), ret);
		}
	}

	//printf("CreateCertByCA [%s] end | ret=%p | verify: %d | verify2: %d\n", host.c_str(), ret, X509_verify(ret, ca_key), X509_verify(ret, server_key));
	return ret;
}

auto CertHelper::CreateDnsList(const std::string& host) -> std::optional<std::list<std::string>>
{
	/*

	正常情况：

	input:
			"baidu.com"
	output:
			"baidu.com"
		  "*.baidu.com"

	input:
			"www.baidu.com"
	output:
			"www.baidu.com"
			  "*.baidu.com"

	input:
			"123.api.baidu.com"
	output:
			"123.api.baidu.com"
			  "*.api.baidu.com"
				  "*.baidu.com"
	...

	异常情况：
	input:
			""
	output:
			nullopt

	input:
			"com"
	output:
			nullopt
	*/

	auto ret = std::list<std::string>();
	auto tmp = Utils::Split(host, ".", true);
	if (tmp.size() < 2) {
		return std::nullopt;
	}
	if (tmp.size() == 2) {
		ret.push_back("*." + host);
	}
	else {
		auto tmp2 = std::list<std::string>();
		for (auto it = tmp.rbegin(); it != tmp.rend(); it++) {
			tmp2.push_front(*it);
			std::string dns = "*." + Utils::Join(tmp2, ".");
			ret.push_front(dns);
		}

		ret.pop_front();
		ret.pop_back();
	}

	ret.push_front(host);

	return ret;
}