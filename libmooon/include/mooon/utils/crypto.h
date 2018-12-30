// Writed by yijian on 2018/12/23
#ifndef MOOON_UTILS_CRYPTO_H
#define MOOON_UTILS_CRYPTO_H
#include "mooon/utils/config.h"
UTILS_NAMESPACE_BEGIN
#if MOOON_HAVE_OPENSSL == 1

void base64_encode(const std::string& src, std::string* dest, bool no_newline=true);
void base64_decode(const std::string& src, std::string* dest, bool no_newline=true);

#endif // MOOON_HAVE_OPENSSL
UTILS_NAMESPACE_END
#endif // MOOON_UTILS_CRYPTO_H
