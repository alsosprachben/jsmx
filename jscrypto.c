#include "jscrypto.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "utf8.h"

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#endif

static uint8_t
jscrypto_hex_lower(uint8_t value)
{
	return (uint8_t)(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

static int
jscrypto_digest_algorithm_eq(const uint8_t *name, size_t len,
		const char *expected)
{
	size_t expected_len = strlen(expected);

	return len == expected_len && memcmp(name, expected, expected_len) == 0;
}

int
jscrypto_digest_algorithm_parse(const uint8_t *name, size_t len,
		jscrypto_digest_algorithm_t *algorithm_ptr)
{
	if (name == NULL || algorithm_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-1")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA1;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-256")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA256;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-384")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA384;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-512")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA512;
		return 0;
	}
	errno = ENOTSUP;
	return -1;
}

int
jscrypto_digest_length(jscrypto_digest_algorithm_t algorithm, size_t *len_ptr)
{
	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	switch (algorithm) {
	case JSCRYPTO_DIGEST_SHA1:
		*len_ptr = 20;
		return 0;
	case JSCRYPTO_DIGEST_SHA256:
		*len_ptr = 32;
		return 0;
	case JSCRYPTO_DIGEST_SHA384:
		*len_ptr = 48;
		return 0;
	case JSCRYPTO_DIGEST_SHA512:
		*len_ptr = 64;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int
jscrypto_digest_block_size_bits(jscrypto_digest_algorithm_t algorithm,
		size_t *len_ptr)
{
	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	switch (algorithm) {
	case JSCRYPTO_DIGEST_SHA1:
	case JSCRYPTO_DIGEST_SHA256:
		*len_ptr = 512;
		return 0;
	case JSCRYPTO_DIGEST_SHA384:
	case JSCRYPTO_DIGEST_SHA512:
		*len_ptr = 1024;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

static int
jscrypto_aes_gcm_tag_bits_valid(uint32_t tag_bits)
{
	switch (tag_bits) {
	case 32:
	case 64:
	case 96:
	case 104:
	case 112:
	case 120:
	case 128:
		return 1;
	default:
		return 0;
	}
}

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
static const EVP_MD *
jscrypto_digest_evp(jscrypto_digest_algorithm_t algorithm)
{
	switch (algorithm) {
	case JSCRYPTO_DIGEST_SHA1:
		return EVP_sha1();
	case JSCRYPTO_DIGEST_SHA256:
		return EVP_sha256();
	case JSCRYPTO_DIGEST_SHA384:
		return EVP_sha384();
	case JSCRYPTO_DIGEST_SHA512:
		return EVP_sha512();
	default:
		return NULL;
	}
}

static const EVP_CIPHER *
jscrypto_aes_gcm_evp(size_t key_len)
{
	switch (key_len) {
	case 16:
		return EVP_aes_128_gcm();
	case 24:
		return EVP_aes_192_gcm();
	case 32:
		return EVP_aes_256_gcm();
	default:
		return NULL;
	}
}
#endif

int
jscrypto_random_bytes(uint8_t *buf, size_t len)
{
	size_t offset = 0;

	if (buf == NULL) {
		errno = EINVAL;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)offset;
	(void)len;
	errno = ENOTSUP;
	return -1;
#else
	while (offset < len) {
		size_t chunk = len - offset;
		int rc;

		if (chunk > (size_t)INT_MAX) {
			chunk = (size_t)INT_MAX;
		}
		rc = RAND_bytes(buf + offset, (int)chunk);
		if (rc != 1) {
			errno = EIO;
			return -1;
		}
		offset += chunk;
	}
	return 0;
#endif
}

int
jscrypto_digest(jscrypto_digest_algorithm_t algorithm, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t digest_len = 0;

	if ((input == NULL && input_len > 0) || (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_digest_length(algorithm, &digest_len) < 0) {
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = digest_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < digest_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)input;
	(void)input_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(algorithm);
		unsigned int written = 0;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (EVP_Digest(input, input_len, output, &written, md, NULL) != 1) {
			errno = EIO;
			return -1;
		}
		if ((size_t)written != digest_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_hmac(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t digest_len = 0;

	if ((key == NULL && key_len > 0) || (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_digest_length(algorithm, &digest_len) < 0) {
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = digest_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < digest_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)key;
	(void)key_len;
	(void)input;
	(void)input_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(algorithm);
		unsigned int written = 0;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (key_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		if (HMAC(md, key, (int)key_len, input, input_len, output, &written)
				== NULL) {
			errno = EIO;
			return -1;
		}
		if ((size_t)written != digest_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_hmac_verify(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *input, size_t input_len,
		const uint8_t *signature, size_t signature_len, int *matches_ptr)
{
	size_t mac_len = 0;
	uint8_t mac[64];
	size_t i;
	unsigned diff = 0;
	size_t compare_len;

	if ((key == NULL && key_len > 0) || (input == NULL && input_len > 0)
			|| (signature == NULL && signature_len > 0)
			|| matches_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_hmac(algorithm, key, key_len, input, input_len, mac,
			sizeof(mac), &mac_len) < 0) {
		return -1;
	}
#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
	if (signature_len == mac_len) {
		*matches_ptr = CRYPTO_memcmp(signature, mac, mac_len) == 0;
		return 0;
	}
#endif
	diff = signature_len == mac_len ? 0u : 1u;
	compare_len = signature_len > mac_len ? signature_len : mac_len;
	for (i = 0; i < compare_len; i++) {
		uint8_t lhs = i < signature_len ? signature[i] : 0;
		uint8_t rhs = i < mac_len ? mac[i] : 0;

		diff |= (unsigned)(lhs ^ rhs);
	}
	*matches_ptr = diff == 0u;
	return 0;
}

int
jscrypto_pbkdf2(jscrypto_digest_algorithm_t algorithm, const uint8_t *password,
		size_t password_len, const uint8_t *salt, size_t salt_len,
		uint32_t iterations, uint8_t *output, size_t output_len)
{
	static const uint8_t empty_salt[1] = { 0 };
	static const char empty_password[1] = { '\0' };

	if ((password == NULL && password_len > 0)
			|| (salt == NULL && salt_len > 0)
			|| (output == NULL && output_len > 0)
			|| iterations == 0) {
		errno = EINVAL;
		return -1;
	}
	if (output_len > (size_t)INT_MAX || iterations > (uint32_t)INT_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)algorithm;
	(void)password;
	(void)password_len;
	(void)salt;
	(void)salt_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(algorithm);

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (PKCS5_PBKDF2_HMAC(
					password_len > 0 ? (const char *)password : empty_password,
					(int)password_len,
					salt_len > 0 ? salt : empty_salt, (int)salt_len,
					(int)iterations, md, (int)output_len, output) != 1) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_hkdf(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len, uint8_t *output,
		size_t output_len)
{
	static const uint8_t empty_input[1] = { 0 };

	if ((key == NULL && key_len > 0) || (salt == NULL && salt_len > 0)
			|| (info == NULL && info_len > 0) || (output == NULL && output_len > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len > (size_t)INT_MAX || salt_len > (size_t)INT_MAX
			|| info_len > (size_t)INT_MAX) {
		errno = EOVERFLOW;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)algorithm;
	(void)key;
	(void)key_len;
	(void)salt;
	(void)salt_len;
	(void)info;
	(void)info_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(algorithm);
		EVP_PKEY_CTX *ctx = NULL;
		size_t derived_len = output_len;
		int rc = 0;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		rc = EVP_PKEY_derive_init(ctx);
		if (rc == 1) {
			rc = EVP_PKEY_CTX_set_hkdf_mode(ctx,
					EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND);
		}
		if (rc == 1) {
			rc = EVP_PKEY_CTX_set_hkdf_md(ctx, md);
		}
		if (rc == 1) {
			rc = EVP_PKEY_CTX_set1_hkdf_salt(ctx,
					salt_len > 0 ? salt : empty_input, (int)salt_len);
		}
		if (rc == 1) {
			rc = EVP_PKEY_CTX_set1_hkdf_key(ctx,
					key_len > 0 ? key : empty_input, (int)key_len);
		}
		if (rc == 1) {
			rc = EVP_PKEY_CTX_add1_hkdf_info(ctx,
					info_len > 0 ? info : empty_input, (int)info_len);
		}
		if (rc == 1) {
			rc = EVP_PKEY_derive(ctx, output, &derived_len);
		}
		EVP_PKEY_CTX_free(ctx);
		if (rc != 1 || derived_len != output_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t tag_len;
	size_t output_len;

	if ((key == NULL && key_len > 0) || (iv == NULL && iv_len > 0)
			|| (aad == NULL && aad_len > 0) || (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (!jscrypto_aes_gcm_tag_bits_valid(tag_bits)) {
		errno = EINVAL;
		return -1;
	}
	tag_len = tag_bits / 8u;
	if (input_len > SIZE_MAX - tag_len) {
		errno = EOVERFLOW;
		return -1;
	}
	output_len = input_len + tag_len;
	if (len_ptr != NULL) {
		*len_ptr = output_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < output_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)key;
	(void)iv;
	(void)aad;
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_gcm_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || iv_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX
				|| input_len > (size_t)INT_MAX || tag_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1
				|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len,
					NULL) != 1
				|| EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (aad_len > 0 && EVP_EncryptUpdate(ctx, NULL, &written, aad,
				(int)aad_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (input_len > 0 && EVP_EncryptUpdate(ctx, output, &written, input,
				(int)input_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		total = written;
		if (EVP_EncryptFinal_ex(ctx, output + total, &final_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		total += final_len;
		if ((size_t)total != input_len
				|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)tag_len,
					output + input_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		EVP_CIPHER_CTX_free(ctx);
		return 0;
	}
#endif
}

int
jscrypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t tag_len;
	size_t output_len;
	size_t ciphertext_len;

	if ((key == NULL && key_len > 0) || (iv == NULL && iv_len > 0)
			|| (aad == NULL && aad_len > 0) || (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (!jscrypto_aes_gcm_tag_bits_valid(tag_bits)) {
		errno = EINVAL;
		return -1;
	}
	tag_len = tag_bits / 8u;
	if (input_len < tag_len) {
		errno = EINVAL;
		return -1;
	}
	ciphertext_len = input_len - tag_len;
	output_len = ciphertext_len;
	if (len_ptr != NULL) {
		*len_ptr = output_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < output_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)key;
	(void)iv;
	(void)aad;
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_gcm_evp(key_len);
		const uint8_t *tag = input + ciphertext_len;
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || iv_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX
				|| ciphertext_len > (size_t)INT_MAX || tag_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1
				|| EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len,
					NULL) != 1
				|| EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (aad_len > 0 && EVP_DecryptUpdate(ctx, NULL, &written, aad,
				(int)aad_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (ciphertext_len > 0
				&& EVP_DecryptUpdate(ctx, output, &written, input,
					(int)ciphertext_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		total = written;
		if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)tag_len,
				(void *)tag) != 1
				|| EVP_DecryptFinal_ex(ctx, output + total, &final_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		total += final_len;
		EVP_CIPHER_CTX_free(ctx);
		if ((size_t)total != ciphertext_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_random_uuid(uint8_t *buf, size_t cap, size_t *len_ptr)
{
	static const size_t uuid_len = 36;
	uint8_t bytes[16];
	size_t i;
	size_t out = 0;

	if (len_ptr != NULL) {
		*len_ptr = uuid_len;
	}
	if (buf == NULL && cap > 0) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_random_bytes(bytes, sizeof(bytes)) < 0) {
		return -1;
	}
	bytes[6] = (uint8_t)((bytes[6] & 0x0f) | 0x40);
	bytes[8] = (uint8_t)((bytes[8] & 0x3f) | 0x80);
	if (buf == NULL) {
		return 0;
	}
	if (cap < uuid_len) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < sizeof(bytes); i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10) {
			buf[out++] = '-';
		}
		buf[out++] = jscrypto_hex_lower((uint8_t)((bytes[i] >> 4) & 0x0f));
		buf[out++] = jscrypto_hex_lower((uint8_t)(bytes[i] & 0x0f));
	}
	return 0;
}
