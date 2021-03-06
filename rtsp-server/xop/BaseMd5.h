#ifndef _XOP_BASEMD5_H
#define _XOP_BASEMD5_H

#include "Md5.h"

namespace xop {
class BaseMd5 : public Md5 {
public:
	BaseMd5();
	~BaseMd5() override;

	void GetMd5Hash(const unsigned char *data, size_t dataSize,
			unsigned char *outHash) override;
};
}

#endif
