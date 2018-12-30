// Writed by yijian on 2018/12/23
#include "utils/crypto.h"
#if MOOON_HAVE_OPENSSL == 1
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
UTILS_NAMESPACE_BEGIN

// https://linux.die.net/man/3/bio
// https://linux.die.net/man/3/bio_push
// https://linux.die.net/man/3/bio_f_base64
// https://linux.die.net/man/3/bio_write
void base64_encode(const std::string& src, std::string* dest, bool no_newline)
{
    BUF_MEM* bptr;
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());

    if (no_newline)
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, src.data(), static_cast<int>(src.length()));
    if (BIO_flush(b64));
    BIO_get_mem_ptr(b64, &bptr);

    dest->resize(bptr->length);
    memcpy(const_cast<char*>(dest->data()), bptr->data, bptr->length);
    BIO_free_all(b64);
}

// https://linux.die.net/man/3/bio_read
// https://www.openssl.org/docs/man1.0.2/crypto/BIO_read.html
void base64_decode(const std::string& src, std::string* dest, bool no_newline)
{
    BIO* bmem = BIO_new_mem_buf(const_cast<char*>(src.data()), static_cast<int>(src.length()));
    BIO* b64 = BIO_new(BIO_f_base64());

    if (no_newline)
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_push(b64, bmem);

    dest->resize(src.length());
    const int bytes = BIO_read(bmem, const_cast<char*>(dest->data()), static_cast<int>(src.length()));
    dest->resize(bytes);
    BIO_free_all(bmem);

}

UTILS_NAMESPACE_END
#endif // MOOON_HAVE_OPENSSL
