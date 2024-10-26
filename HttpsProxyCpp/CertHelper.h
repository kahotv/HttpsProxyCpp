#pragma once
class CertHelper
{
public:

	bool init(
		const char* caCertFile,
		const char* caKeyFile,
		const char* serverKeyFile);
	void free();
	X509* createServerCert(const std::string& host);
	static auto PEM_ReadX509(const char* filepath) -> X509*;
	static auto PEM_ReadPKEY(const char* filepath) -> EVP_PKEY*;
	static void PEM_WriteX509(const char* filepath, const X509* x509);
	static auto PEM_FormX509(const X509* x509) -> std::string;
	static auto CreateSNO() -> ASN1_INTEGER*;
	static auto CreateExtDNS(X509* cert, const std::string& host) -> X509_EXTENSION*;
	static auto CreateCertByCA(const std::string& host, const X509* ca, EVP_PKEY* ca_key, EVP_PKEY* server_key) -> X509*;
private:
	static auto CreateDnsList(const std::string& host) -> std::optional<std::list<std::string>>;
private:
	X509* m_caCert;
	EVP_PKEY* m_caKey;
	EVP_PKEY* m_serverKey;
};

