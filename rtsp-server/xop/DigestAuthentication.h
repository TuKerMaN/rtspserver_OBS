//PHZ
//2019-10-6

#ifndef RTSP_DIGEST_AUTHENTICATION_H
#define RTSP_DIGEST_AUTHENTICATION_H

#include <string>
#include "Md5.h"

namespace xop {

class DigestAuthentication {
public:
	DigestAuthentication(std::string realm, std::string
	                     username,
	                     std::string password);
	virtual ~DigestAuthentication();

	std::string GetRealm() const { return realm_; }

	std::string GetUsername() const { return username_; }

	std::string GetPassword() const { return password_; }

	std::string GetNonce() const;
	std::string GetResponse(const std::string &nonce, const std::string &cmd,
	                        const std::string &url) const;

private:
	std::string realm_;
	std::string username_;
	std::string password_;
	Md5 *md5_;
};

}

#endif
