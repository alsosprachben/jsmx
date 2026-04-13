#include "jscrypto.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "utf8.h"

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
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

static const EVP_CIPHER *
jscrypto_aes_ctr_evp(size_t key_len)
{
	switch (key_len) {
	case 16:
		return EVP_aes_128_ctr();
	case 24:
		return EVP_aes_192_ctr();
	case 32:
		return EVP_aes_256_ctr();
	default:
		return NULL;
	}
}

static const EVP_CIPHER *
jscrypto_aes_ecb_evp(size_t key_len)
{
	switch (key_len) {
	case 16:
		return EVP_aes_128_ecb();
	case 24:
		return EVP_aes_192_ecb();
	case 32:
		return EVP_aes_256_ecb();
	default:
		return NULL;
	}
}

static const EVP_CIPHER *
jscrypto_aes_cbc_evp(size_t key_len)
{
	switch (key_len) {
	case 16:
		return EVP_aes_128_cbc();
	case 24:
		return EVP_aes_192_cbc();
	case 32:
		return EVP_aes_256_cbc();
	default:
		return NULL;
	}
}

static const EVP_CIPHER *
jscrypto_aes_kw_evp(size_t key_len)
{
	switch (key_len) {
	case 16:
		return EVP_aes_128_wrap();
	case 24:
		return EVP_aes_192_wrap();
	case 32:
		return EVP_aes_256_wrap();
	default:
		return NULL;
	}
}
#endif

static void
jscrypto_aes_ctr_increment(uint8_t *block, uint32_t length_bits)
{
	/*
	 * WebCrypto AES-CTR treats the rightmost length_bits of the 16-byte
	 * counter block as the counter and the leftmost (128 - length_bits)
	 * bits as a fixed nonce. Increment by 1 modulo 2^length_bits, with
	 * carries confined to the counter portion.
	 */
	uint32_t i;
	uint32_t carry = 1u;

	for (i = 0; i < length_bits && carry != 0u; i++) {
		uint32_t byte_index = 15u - (i / 8u);
		uint32_t bit_mask = 1u << (i % 8u);

		if ((block[byte_index] & bit_mask) == 0u) {
			block[byte_index] = (uint8_t)(block[byte_index] | bit_mask);
			carry = 0u;
		} else {
			block[byte_index] = (uint8_t)(block[byte_index] & ~bit_mask);
		}
	}
	/* If carry is still 1 the counter wrapped to zero, which is correct
	 * mod 2^length_bits — every bit in the counter region is already 0. */
}

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
	if (output_len == 0) {
		return 0;
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
	if (output_len == 0) {
		return 0;
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
	if ((key_len != 16 && key_len != 24 && key_len != 32) || iv_len == 0) {
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
	if ((key_len != 16 && key_len != 24 && key_len != 32) || iv_len == 0) {
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
jscrypto_aes_ctr_crypt(const uint8_t *key, size_t key_len,
		const uint8_t *counter, size_t counter_len, uint32_t length_bits,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr)
{
	if ((key == NULL && key_len > 0) || (counter == NULL && counter_len > 0)
			|| (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (counter_len != 16) {
		errno = EINVAL;
		return -1;
	}
	if (length_bits == 0 || length_bits > 128) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * WebCrypto AES-CTR step 1: input must not be longer than
	 * 2^length_bits counter blocks of 16 bytes. The check is only
	 * meaningful when 2^length_bits * 16 fits in size_t; otherwise
	 * size_t already constrains the input.
	 */
	if (length_bits < 60) {
		uint64_t max_blocks = (uint64_t)1 << length_bits;
		uint64_t input_blocks = (uint64_t)((input_len + 15u) / 16u);

		if (input_blocks > max_blocks) {
			errno = EINVAL;
			return -1;
		}
	}
	if (len_ptr != NULL) {
		*len_ptr = input_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < input_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)key;
	(void)counter;
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	if (input_len == 0) {
		return 0;
	}
	if (length_bits == 128) {
		/*
		 * Fast path: when the entire 16-byte block is the counter,
		 * WebCrypto's "increment mod 2^128" matches OpenSSL CTR exactly.
		 */
		const EVP_CIPHER *cipher = jscrypto_aes_ctr_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || input_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, counter) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptUpdate(ctx, output, &written, input,
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
		EVP_CIPHER_CTX_free(ctx);
		if ((size_t)total != input_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
	{
		/*
		 * length_bits < 128: OpenSSL CTR would carry across the nonce
		 * boundary on overflow, but WebCrypto requires the carry to wrap
		 * inside the rightmost length_bits only. Drive AES-ECB block by
		 * block and increment the counter ourselves.
		 */
		const EVP_CIPHER *cipher = jscrypto_aes_ecb_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		uint8_t counter_block[16];
		uint8_t keystream[16];
		size_t pos = 0;

		if (cipher == NULL) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		EVP_CIPHER_CTX_set_padding(ctx, 0);
		memcpy(counter_block, counter, 16);
		while (pos < input_len) {
			int written = 0;
			size_t chunk = input_len - pos;
			size_t i;

			if (chunk > 16u) {
				chunk = 16u;
			}
			if (EVP_EncryptUpdate(ctx, keystream, &written, counter_block,
					16) != 1 || written != 16) {
				EVP_CIPHER_CTX_free(ctx);
				errno = EIO;
				return -1;
			}
			for (i = 0; i < chunk; i++) {
				output[pos + i] = (uint8_t)(input[pos + i] ^ keystream[i]);
			}
			pos += chunk;
			if (pos < input_len) {
				jscrypto_aes_ctr_increment(counter_block, length_bits);
			}
		}
		EVP_CIPHER_CTX_free(ctx);
		return 0;
	}
#endif
}

int
jscrypto_aes_cbc_encrypt(const uint8_t *key, size_t key_len, const uint8_t *iv,
		size_t iv_len, const uint8_t *input, size_t input_len, uint8_t *output,
		size_t cap, size_t *len_ptr)
{
	size_t output_len;

	if ((key == NULL && key_len > 0) || (iv == NULL && iv_len > 0)
			|| (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (iv_len != 16) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * PKCS#7 padding always appends at least one byte and rounds the
	 * ciphertext up to a full block multiple, so an N-byte input becomes
	 * ((N / 16) + 1) * 16 bytes of ciphertext.
	 */
	if (input_len > SIZE_MAX - 16u) {
		errno = EOVERFLOW;
		return -1;
	}
	output_len = ((input_len / 16u) + 1u) * 16u;
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
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_cbc_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || input_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) != 1) {
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
		EVP_CIPHER_CTX_free(ctx);
		if ((size_t)total != output_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_aes_cbc_decrypt(const uint8_t *key, size_t key_len, const uint8_t *iv,
		size_t iv_len, const uint8_t *input, size_t input_len, uint8_t *output,
		size_t cap, size_t *len_ptr)
{
	if ((key == NULL && key_len > 0) || (iv == NULL && iv_len > 0)
			|| (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (iv_len != 16) {
		errno = EINVAL;
		return -1;
	}
	if (input_len == 0 || (input_len % 16u) != 0) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * Plaintext length is unknown until PKCS#7 padding is stripped, but
	 * the OpenSSL intermediate write may temporarily hold up to input_len
	 * bytes in the output buffer before Final strips the trailing block.
	 * The sizing call therefore reports input_len as the required cap,
	 * and the real call writes the post-strip length to len_ptr.
	 */
	if (output == NULL) {
		if (len_ptr != NULL) {
			*len_ptr = input_len;
		}
		return 0;
	}
	if (cap < input_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)key;
	(void)iv;
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_cbc_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || input_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		if (EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (EVP_DecryptUpdate(ctx, output, &written, input,
				(int)input_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		total = written;
		if (EVP_DecryptFinal_ex(ctx, output + total, &final_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EINVAL;
			return -1;
		}
		total += final_len;
		EVP_CIPHER_CTX_free(ctx);
		if (len_ptr != NULL) {
			*len_ptr = (size_t)total;
		}
		return 0;
	}
#endif
}

int
jscrypto_aes_kw_wrap(const uint8_t *key, size_t key_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t output_len;

	if ((key == NULL && key_len > 0) || (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * RFC 3394 requires the input to be at least two 64-bit blocks and a
	 * multiple of 8 bytes. Output is 8 bytes longer than input.
	 */
	if (input_len < 16u || (input_len % 8u) != 0u
			|| input_len > SIZE_MAX - 8u) {
		errno = EINVAL;
		return -1;
	}
	output_len = input_len + 8u;
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
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_kw_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || input_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		/* OpenSSL disables the AES-KW ciphers by default; opt in. */
		EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
		if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		if (EVP_EncryptUpdate(ctx, output, &written, input,
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
		EVP_CIPHER_CTX_free(ctx);
		if ((size_t)total != output_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_aes_kw_unwrap(const uint8_t *key, size_t key_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t output_len;

	if ((key == NULL && key_len > 0) || (input == NULL && input_len > 0)
			|| (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (key_len != 16 && key_len != 24 && key_len != 32) {
		errno = EINVAL;
		return -1;
	}
	if (input_len < 24u || (input_len % 8u) != 0u) {
		errno = EINVAL;
		return -1;
	}
	output_len = input_len - 8u;
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
	(void)input;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_CIPHER *cipher = jscrypto_aes_kw_evp(key_len);
		EVP_CIPHER_CTX *ctx = NULL;
		int written = 0;
		int total = 0;
		int final_len = 0;

		if (cipher == NULL || input_len > (size_t)INT_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		ctx = EVP_CIPHER_CTX_new();
		if (ctx == NULL) {
			errno = EIO;
			return -1;
		}
		EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
		if (EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EIO;
			return -1;
		}
		/*
		 * For the AES-KW cipher OpenSSL processes the entire wrap input
		 * in EVP_DecryptUpdate and detects the AIV mismatch there
		 * (EVP_DecryptFinal_ex is effectively a no-op). Map any Update
		 * or Final failure to EINVAL so the runtime surfaces it as
		 * OperationError.
		 */
		if (EVP_DecryptUpdate(ctx, output, &written, input,
				(int)input_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EINVAL;
			return -1;
		}
		total = written;
		if (EVP_DecryptFinal_ex(ctx, output + total, &final_len) != 1) {
			EVP_CIPHER_CTX_free(ctx);
			errno = EINVAL;
			return -1;
		}
		total += final_len;
		EVP_CIPHER_CTX_free(ctx);
		if ((size_t)total != output_len) {
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

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL

/*
 * ECDSA P-256. WebCrypto requires IEEE P1363 fixed-width `r || s` signature
 * format (64 bytes for P-256), while OpenSSL emits DER. These helpers convert
 * between the two; der_to_p1363 parses the DER SEQUENCE and zero-pads each
 * coordinate to the fixed width, p1363_to_der does the reverse.
 *
 * The EC_KEY / EVP_PKEY_assign_EC_KEY API is marked deprecated in
 * OpenSSL 3.0 but still functions correctly. The replacement EVP_PKEY
 * / OSSL_PARAM path would require a substantial rewrite; suppress the
 * deprecation warnings here until there's a cross-cutting migration.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static int
jscrypto_ecdsa_der_to_p1363(const uint8_t *der, size_t der_len,
		size_t coord_len, uint8_t *out)
{
	const uint8_t *cursor = der;
	ECDSA_SIG *sig = d2i_ECDSA_SIG(NULL, &cursor, (long)der_len);
	const BIGNUM *r = NULL;
	const BIGNUM *s = NULL;

	if (sig == NULL) {
		errno = EIO;
		return -1;
	}
	ECDSA_SIG_get0(sig, &r, &s);
	if (BN_bn2binpad(r, out, (int)coord_len) != (int)coord_len
			|| BN_bn2binpad(s, out + coord_len, (int)coord_len)
				!= (int)coord_len) {
		ECDSA_SIG_free(sig);
		errno = EIO;
		return -1;
	}
	ECDSA_SIG_free(sig);
	return 0;
}

static int
jscrypto_ecdsa_p1363_to_der(const uint8_t *p1363, size_t coord_len,
		uint8_t **der_out, int *der_len_out)
{
	ECDSA_SIG *sig = ECDSA_SIG_new();
	BIGNUM *r = NULL;
	BIGNUM *s = NULL;
	int len;
	uint8_t *out = NULL;

	if (sig == NULL) {
		errno = ENOMEM;
		return -1;
	}
	r = BN_bin2bn(p1363, (int)coord_len, NULL);
	s = BN_bin2bn(p1363 + coord_len, (int)coord_len, NULL);
	if (r == NULL || s == NULL || ECDSA_SIG_set0(sig, r, s) != 1) {
		BN_free(r);
		BN_free(s);
		ECDSA_SIG_free(sig);
		errno = EIO;
		return -1;
	}
	/* sig now owns r and s. */
	len = i2d_ECDSA_SIG(sig, &out);
	ECDSA_SIG_free(sig);
	if (len <= 0 || out == NULL) {
		errno = EIO;
		return -1;
	}
	*der_out = out;
	*der_len_out = len;
	return 0;
}

static EVP_PKEY *
jscrypto_ecdsa_pkey_from_xy(const uint8_t public_xy[64])
{
	EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	EVP_PKEY *pkey = NULL;
	uint8_t uncompressed[65];

	if (ec == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	uncompressed[0] = 0x04;
	memcpy(uncompressed + 1, public_xy, 64);
	if (EC_KEY_oct2key(ec, uncompressed, sizeof(uncompressed), NULL) != 1) {
		EC_KEY_free(ec);
		errno = EIO;
		return NULL;
	}
	pkey = EVP_PKEY_new();
	if (pkey == NULL || EVP_PKEY_assign_EC_KEY(pkey, ec) != 1) {
		EVP_PKEY_free(pkey);
		EC_KEY_free(ec);
		errno = ENOMEM;
		return NULL;
	}
	/* pkey now owns ec. */
	return pkey;
}

static EVP_PKEY *
jscrypto_ecdsa_pkey_from_scalar(const uint8_t private_in[32])
{
	EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	BIGNUM *priv_bn = NULL;
	const EC_GROUP *group = NULL;
	EC_POINT *pub = NULL;
	EVP_PKEY *pkey = NULL;

	if (ec == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	priv_bn = BN_bin2bn(private_in, 32, NULL);
	if (priv_bn == NULL || EC_KEY_set_private_key(ec, priv_bn) != 1) {
		BN_free(priv_bn);
		EC_KEY_free(ec);
		errno = EIO;
		return NULL;
	}
	group = EC_KEY_get0_group(ec);
	pub = EC_POINT_new(group);
	if (pub == NULL
			|| EC_POINT_mul(group, pub, priv_bn, NULL, NULL, NULL) != 1
			|| EC_KEY_set_public_key(ec, pub) != 1) {
		EC_POINT_free(pub);
		BN_free(priv_bn);
		EC_KEY_free(ec);
		errno = EIO;
		return NULL;
	}
	EC_POINT_free(pub);
	BN_free(priv_bn);
	pkey = EVP_PKEY_new();
	if (pkey == NULL || EVP_PKEY_assign_EC_KEY(pkey, ec) != 1) {
		EVP_PKEY_free(pkey);
		EC_KEY_free(ec);
		errno = ENOMEM;
		return NULL;
	}
	return pkey;
}

static int
jscrypto_ecdsa_extract_public_xy(const EC_KEY *ec, uint8_t out_xy[64])
{
	const EC_GROUP *group = EC_KEY_get0_group(ec);
	const EC_POINT *point = EC_KEY_get0_public_key(ec);
	BIGNUM *x = BN_new();
	BIGNUM *y = BN_new();
	int rc = -1;

	if (group == NULL || point == NULL || x == NULL || y == NULL) {
		goto out;
	}
	if (EC_POINT_get_affine_coordinates(group, point, x, y, NULL) != 1) {
		goto out;
	}
	if (BN_bn2binpad(x, out_xy, 32) != 32
			|| BN_bn2binpad(y, out_xy + 32, 32) != 32) {
		goto out;
	}
	rc = 0;
out:
	BN_free(x);
	BN_free(y);
	if (rc < 0) {
		errno = EIO;
	}
	return rc;
}

#pragma GCC diagnostic pop

#endif /* JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL */

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

int
jscrypto_ecdsa_p256_generate(uint8_t private_out[32], uint8_t public_out_xy[64])
{
	if (private_out == NULL || public_out_xy == NULL) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	errno = ENOTSUP;
	return -1;
#else
	{
		EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		const BIGNUM *priv_bn = NULL;

		if (ec == NULL) {
			errno = ENOMEM;
			return -1;
		}
		if (EC_KEY_generate_key(ec) != 1) {
			EC_KEY_free(ec);
			errno = EIO;
			return -1;
		}
		priv_bn = EC_KEY_get0_private_key(ec);
		if (priv_bn == NULL
				|| BN_bn2binpad(priv_bn, private_out, 32) != 32) {
			EC_KEY_free(ec);
			errno = EIO;
			return -1;
		}
		if (jscrypto_ecdsa_extract_public_xy(ec, public_out_xy) < 0) {
			EC_KEY_free(ec);
			return -1;
		}
		EC_KEY_free(ec);
		return 0;
	}
#endif
}

int
jscrypto_ecdsa_p256_public_from_private(const uint8_t private_in[32],
		uint8_t public_out_xy[64])
{
	if (private_in == NULL || public_out_xy == NULL) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	errno = ENOTSUP;
	return -1;
#else
	{
		EVP_PKEY *pkey = jscrypto_ecdsa_pkey_from_scalar(private_in);
		const EC_KEY *ec = NULL;
		int rc;

		if (pkey == NULL) {
			return -1;
		}
		ec = EVP_PKEY_get0_EC_KEY(pkey);
		if (ec == NULL) {
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		rc = jscrypto_ecdsa_extract_public_xy(ec, public_out_xy);
		EVP_PKEY_free(pkey);
		return rc;
	}
#endif
}

int
jscrypto_ecdsa_p256_sign(const uint8_t private_in[32],
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		uint8_t signature_out[64])
{
	if (private_in == NULL || signature_out == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)hash;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		uint8_t der[96];
		size_t der_len = sizeof(der);
		int rc = -1;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		pkey = jscrypto_ecdsa_pkey_from_scalar(private_in);
		if (pkey == NULL) {
			return -1;
		}
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestSignInit(ctx, NULL, md, NULL, pkey) != 1
				|| EVP_DigestSignUpdate(ctx, data, data_len) != 1
				|| EVP_DigestSignFinal(ctx, der, &der_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		rc = jscrypto_ecdsa_der_to_p1363(der, der_len, 32, signature_out);
		return rc;
	}
#endif
}

int
jscrypto_ecdsa_p256_verify(const uint8_t public_in_xy[64],
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		const uint8_t signature_in[64], int *matches_out)
{
	if (public_in_xy == NULL || signature_in == NULL || matches_out == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
	*matches_out = 0;
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)hash;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		uint8_t *der = NULL;
		int der_len = 0;
		int verify_rc;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (jscrypto_ecdsa_p1363_to_der(signature_in, 32, &der,
				&der_len) < 0) {
			return -1;
		}
		pkey = jscrypto_ecdsa_pkey_from_xy(public_in_xy);
		if (pkey == NULL) {
			OPENSSL_free(der);
			return -1;
		}
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			OPENSSL_free(der);
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestVerifyInit(ctx, NULL, md, NULL, pkey) != 1
				|| EVP_DigestVerifyUpdate(ctx, data, data_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			OPENSSL_free(der);
			errno = EIO;
			return -1;
		}
		verify_rc = EVP_DigestVerifyFinal(ctx, der, (size_t)der_len);
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		OPENSSL_free(der);
		if (verify_rc == 1) {
			*matches_out = 1;
			return 0;
		}
		if (verify_rc == 0) {
			*matches_out = 0;
			return 0;
		}
		errno = EIO;
		return -1;
	}
#endif
}

/*
 * RSASSA-PKCS1-v1_5 support. Keys are stored as PKCS#1 DER blobs
 * (RSAPublicKey / RSAPrivateKey SEQUENCEs) and re-parsed on each
 * operation. This leverages OpenSSL's existing ASN.1 machinery and
 * keeps the runtime arena allocator happy. Like the ECDSA block above,
 * RSA_new / RSA_generate_key_ex / d2i_RSA*Key / EVP_PKEY_assign_RSA are
 * all deprecated in OpenSSL 3.0 but still functional — the pragma block
 * suppresses the warnings.
 */

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL

static RSA *
jscrypto_rsa_from_private_der(const uint8_t *der, size_t der_len)
{
	const uint8_t *cursor = der;

	if (der == NULL || der_len == 0 || der_len > LONG_MAX) {
		errno = EINVAL;
		return NULL;
	}
	{
		RSA *rsa = d2i_RSAPrivateKey(NULL, &cursor, (long)der_len);
		if (rsa == NULL) {
			errno = EIO;
		}
		return rsa;
	}
}

static RSA *
jscrypto_rsa_from_public_der(const uint8_t *der, size_t der_len)
{
	const uint8_t *cursor = der;

	if (der == NULL || der_len == 0 || der_len > LONG_MAX) {
		errno = EINVAL;
		return NULL;
	}
	{
		RSA *rsa = d2i_RSAPublicKey(NULL, &cursor, (long)der_len);
		if (rsa == NULL) {
			errno = EIO;
		}
		return rsa;
	}
}

static int
jscrypto_rsa_bn_to_bytes(const BIGNUM *bn, uint8_t *out, size_t cap,
		size_t *len_out)
{
	int needed;

	if (bn == NULL || len_out == NULL) {
		errno = EINVAL;
		return -1;
	}
	needed = BN_num_bytes(bn);
	if (needed < 0) {
		errno = EIO;
		return -1;
	}
	*len_out = (size_t)needed;
	if (out == NULL) {
		return 0;
	}
	if ((size_t)needed > cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (BN_bn2bin(bn, out) != needed) {
		errno = EIO;
		return -1;
	}
	return 0;
}

#endif /* JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL */

int
jscrypto_rsa_pkcs1_v1_5_generate(uint32_t modulus_bits,
		uint8_t *private_der_out, size_t private_cap,
		size_t *private_len_out)
{
	if (private_len_out == NULL
			|| (private_der_out == NULL && private_cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (modulus_bits != 2048 && modulus_bits != 3072
			&& modulus_bits != 4096) {
		errno = ENOTSUP;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)private_der_out;
	(void)private_cap;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = RSA_new();
		BIGNUM *e = BN_new();
		uint8_t *der = NULL;
		int der_len;

		if (rsa == NULL || e == NULL || BN_set_word(e, RSA_F4) != 1
				|| RSA_generate_key_ex(rsa, (int)modulus_bits, e, NULL)
					!= 1) {
			BN_free(e);
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		BN_free(e);
		der_len = i2d_RSAPrivateKey(rsa, &der);
		RSA_free(rsa);
		if (der_len <= 0 || der == NULL) {
			errno = EIO;
			return -1;
		}
		*private_len_out = (size_t)der_len;
		if (private_der_out == NULL) {
			OPENSSL_free(der);
			return 0;
		}
		if ((size_t)der_len > private_cap) {
			OPENSSL_free(der);
			errno = ENOBUFS;
			return -1;
		}
		memcpy(private_der_out, der, (size_t)der_len);
		OPENSSL_free(der);
		return 0;
	}
#endif
}

int
jscrypto_rsa_public_from_private(const uint8_t *private_der,
		size_t private_len, uint8_t *public_der_out, size_t public_cap,
		size_t *public_len_out)
{
	if (public_len_out == NULL
			|| (public_der_out == NULL && public_cap > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)private_der;
	(void)private_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = jscrypto_rsa_from_private_der(private_der, private_len);
		uint8_t *der = NULL;
		int der_len;

		if (rsa == NULL) {
			return -1;
		}
		der_len = i2d_RSAPublicKey(rsa, &der);
		RSA_free(rsa);
		if (der_len <= 0 || der == NULL) {
			errno = EIO;
			return -1;
		}
		*public_len_out = (size_t)der_len;
		if (public_der_out == NULL) {
			OPENSSL_free(der);
			return 0;
		}
		if ((size_t)der_len > public_cap) {
			OPENSSL_free(der);
			errno = ENOBUFS;
			return -1;
		}
		memcpy(public_der_out, der, (size_t)der_len);
		OPENSSL_free(der);
		return 0;
	}
#endif
}

int
jscrypto_rsa_modulus_bits(const uint8_t *der, size_t der_len, int is_private,
		uint32_t *bits_out)
{
	if (bits_out == NULL) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)der;
	(void)der_len;
	(void)is_private;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = is_private
				? jscrypto_rsa_from_private_der(der, der_len)
				: jscrypto_rsa_from_public_der(der, der_len);
		const BIGNUM *n = NULL;
		int bits;

		if (rsa == NULL) {
			return -1;
		}
		RSA_get0_key(rsa, &n, NULL, NULL);
		if (n == NULL) {
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		bits = BN_num_bits(n);
		RSA_free(rsa);
		if (bits <= 0) {
			errno = EIO;
			return -1;
		}
		*bits_out = (uint32_t)bits;
		return 0;
	}
#endif
}

int
jscrypto_rsa_pkcs1_v1_5_sign(const uint8_t *private_der, size_t private_len,
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		uint8_t *signature_out, size_t sig_cap, size_t *sig_len_out)
{
	if (signature_out == NULL || sig_len_out == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)private_der;
	(void)private_len;
	(void)hash;
	(void)sig_cap;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		RSA *rsa = NULL;
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		size_t out_len = sig_cap;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		rsa = jscrypto_rsa_from_private_der(private_der, private_len);
		if (rsa == NULL) {
			return -1;
		}
		pkey = EVP_PKEY_new();
		if (pkey == NULL || EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
			EVP_PKEY_free(pkey);
			RSA_free(rsa);
			errno = ENOMEM;
			return -1;
		}
		/* pkey now owns rsa. */
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestSignInit(ctx, NULL, md, NULL, pkey) != 1
				|| EVP_DigestSignUpdate(ctx, data, data_len) != 1
				|| EVP_DigestSignFinal(ctx, signature_out, &out_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		*sig_len_out = out_len;
		return 0;
	}
#endif
}

int
jscrypto_rsa_pkcs1_v1_5_verify(const uint8_t *public_der, size_t public_len,
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		const uint8_t *signature, size_t signature_len, int *matches_out)
{
	if (matches_out == NULL || signature == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
	*matches_out = 0;
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)public_der;
	(void)public_len;
	(void)hash;
	(void)signature_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		RSA *rsa = NULL;
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		int verify_rc;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		rsa = jscrypto_rsa_from_public_der(public_der, public_len);
		if (rsa == NULL) {
			return -1;
		}
		pkey = EVP_PKEY_new();
		if (pkey == NULL || EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
			EVP_PKEY_free(pkey);
			RSA_free(rsa);
			errno = ENOMEM;
			return -1;
		}
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestVerifyInit(ctx, NULL, md, NULL, pkey) != 1
				|| EVP_DigestVerifyUpdate(ctx, data, data_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		verify_rc = EVP_DigestVerifyFinal(ctx, signature, signature_len);
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		if (verify_rc == 1) {
			*matches_out = 1;
			return 0;
		}
		if (verify_rc == 0) {
			*matches_out = 0;
			return 0;
		}
		errno = EIO;
		return -1;
	}
#endif
}

int
jscrypto_rsa_public_jwk_components(const uint8_t *public_der, size_t public_len,
		uint8_t *n_out, size_t n_cap, size_t *n_len,
		uint8_t *e_out, size_t e_cap, size_t *e_len)
{
	if (n_len == NULL || e_len == NULL) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)public_der;
	(void)public_len;
	(void)n_out;
	(void)n_cap;
	(void)e_out;
	(void)e_cap;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = jscrypto_rsa_from_public_der(public_der, public_len);
		const BIGNUM *n = NULL;
		const BIGNUM *e = NULL;

		if (rsa == NULL) {
			return -1;
		}
		RSA_get0_key(rsa, &n, &e, NULL);
		if (jscrypto_rsa_bn_to_bytes(n, n_out, n_cap, n_len) < 0
				|| jscrypto_rsa_bn_to_bytes(e, e_out, e_cap, e_len) < 0) {
			RSA_free(rsa);
			return -1;
		}
		RSA_free(rsa);
		return 0;
	}
#endif
}

int
jscrypto_rsa_private_jwk_components(const uint8_t *private_der,
		size_t private_len,
		uint8_t *n_out, size_t n_cap, size_t *n_len,
		uint8_t *e_out, size_t e_cap, size_t *e_len,
		uint8_t *d_out, size_t d_cap, size_t *d_len,
		uint8_t *p_out, size_t p_cap, size_t *p_len,
		uint8_t *q_out, size_t q_cap, size_t *q_len,
		uint8_t *dp_out, size_t dp_cap, size_t *dp_len,
		uint8_t *dq_out, size_t dq_cap, size_t *dq_len,
		uint8_t *qi_out, size_t qi_cap, size_t *qi_len)
{
	if (n_len == NULL || e_len == NULL || d_len == NULL || p_len == NULL
			|| q_len == NULL || dp_len == NULL || dq_len == NULL
			|| qi_len == NULL) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)private_der;
	(void)private_len;
	(void)n_out;
	(void)n_cap;
	(void)e_out;
	(void)e_cap;
	(void)d_out;
	(void)d_cap;
	(void)p_out;
	(void)p_cap;
	(void)q_out;
	(void)q_cap;
	(void)dp_out;
	(void)dp_cap;
	(void)dq_out;
	(void)dq_cap;
	(void)qi_out;
	(void)qi_cap;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = jscrypto_rsa_from_private_der(private_der, private_len);
		const BIGNUM *n = NULL;
		const BIGNUM *e = NULL;
		const BIGNUM *d = NULL;
		const BIGNUM *p = NULL;
		const BIGNUM *q = NULL;
		const BIGNUM *dp = NULL;
		const BIGNUM *dq = NULL;
		const BIGNUM *qi = NULL;

		if (rsa == NULL) {
			return -1;
		}
		RSA_get0_key(rsa, &n, &e, &d);
		RSA_get0_factors(rsa, &p, &q);
		RSA_get0_crt_params(rsa, &dp, &dq, &qi);
		if (jscrypto_rsa_bn_to_bytes(n, n_out, n_cap, n_len) < 0
				|| jscrypto_rsa_bn_to_bytes(e, e_out, e_cap, e_len) < 0
				|| jscrypto_rsa_bn_to_bytes(d, d_out, d_cap, d_len) < 0
				|| jscrypto_rsa_bn_to_bytes(p, p_out, p_cap, p_len) < 0
				|| jscrypto_rsa_bn_to_bytes(q, q_out, q_cap, q_len) < 0
				|| jscrypto_rsa_bn_to_bytes(dp, dp_out, dp_cap, dp_len) < 0
				|| jscrypto_rsa_bn_to_bytes(dq, dq_out, dq_cap, dq_len) < 0
				|| jscrypto_rsa_bn_to_bytes(qi, qi_out, qi_cap, qi_len) < 0) {
			RSA_free(rsa);
			return -1;
		}
		RSA_free(rsa);
		return 0;
	}
#endif
}

int
jscrypto_rsa_public_from_jwk_components(const uint8_t *n, size_t n_len,
		const uint8_t *e, size_t e_len, uint8_t *public_der_out, size_t cap,
		size_t *len_out)
{
	if (n == NULL || e == NULL || len_out == NULL
			|| (public_der_out == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)n_len;
	(void)e_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = RSA_new();
		BIGNUM *n_bn = BN_bin2bn(n, (int)n_len, NULL);
		BIGNUM *e_bn = BN_bin2bn(e, (int)e_len, NULL);
		uint8_t *der = NULL;
		int der_len;

		if (rsa == NULL || n_bn == NULL || e_bn == NULL
				|| RSA_set0_key(rsa, n_bn, e_bn, NULL) != 1) {
			BN_free(n_bn);
			BN_free(e_bn);
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		/* rsa now owns n_bn/e_bn. */
		der_len = i2d_RSAPublicKey(rsa, &der);
		RSA_free(rsa);
		if (der_len <= 0 || der == NULL) {
			errno = EIO;
			return -1;
		}
		*len_out = (size_t)der_len;
		if (public_der_out == NULL) {
			OPENSSL_free(der);
			return 0;
		}
		if ((size_t)der_len > cap) {
			OPENSSL_free(der);
			errno = ENOBUFS;
			return -1;
		}
		memcpy(public_der_out, der, (size_t)der_len);
		OPENSSL_free(der);
		return 0;
	}
#endif
}

int
jscrypto_rsa_private_from_jwk_components(
		const uint8_t *n, size_t n_len, const uint8_t *e, size_t e_len,
		const uint8_t *d, size_t d_len,
		const uint8_t *p, size_t p_len, const uint8_t *q, size_t q_len,
		const uint8_t *dp, size_t dp_len, const uint8_t *dq, size_t dq_len,
		const uint8_t *qi, size_t qi_len,
		uint8_t *private_der_out, size_t cap, size_t *len_out)
{
	if (n == NULL || e == NULL || d == NULL || p == NULL || q == NULL
			|| dp == NULL || dq == NULL || qi == NULL || len_out == NULL
			|| (private_der_out == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)n_len;
	(void)e_len;
	(void)d_len;
	(void)p_len;
	(void)q_len;
	(void)dp_len;
	(void)dq_len;
	(void)qi_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		RSA *rsa = RSA_new();
		BIGNUM *n_bn = BN_bin2bn(n, (int)n_len, NULL);
		BIGNUM *e_bn = BN_bin2bn(e, (int)e_len, NULL);
		BIGNUM *d_bn = BN_bin2bn(d, (int)d_len, NULL);
		BIGNUM *p_bn = BN_bin2bn(p, (int)p_len, NULL);
		BIGNUM *q_bn = BN_bin2bn(q, (int)q_len, NULL);
		BIGNUM *dp_bn = BN_bin2bn(dp, (int)dp_len, NULL);
		BIGNUM *dq_bn = BN_bin2bn(dq, (int)dq_len, NULL);
		BIGNUM *qi_bn = BN_bin2bn(qi, (int)qi_len, NULL);
		uint8_t *der = NULL;
		int der_len;

		if (rsa == NULL || n_bn == NULL || e_bn == NULL || d_bn == NULL
				|| p_bn == NULL || q_bn == NULL || dp_bn == NULL
				|| dq_bn == NULL || qi_bn == NULL
				|| RSA_set0_key(rsa, n_bn, e_bn, d_bn) != 1) {
			BN_free(n_bn);
			BN_free(e_bn);
			BN_free(d_bn);
			BN_free(p_bn);
			BN_free(q_bn);
			BN_free(dp_bn);
			BN_free(dq_bn);
			BN_free(qi_bn);
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		/* rsa now owns n_bn/e_bn/d_bn. */
		if (RSA_set0_factors(rsa, p_bn, q_bn) != 1) {
			BN_free(p_bn);
			BN_free(q_bn);
			BN_free(dp_bn);
			BN_free(dq_bn);
			BN_free(qi_bn);
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		/* rsa now owns p_bn/q_bn. */
		if (RSA_set0_crt_params(rsa, dp_bn, dq_bn, qi_bn) != 1) {
			BN_free(dp_bn);
			BN_free(dq_bn);
			BN_free(qi_bn);
			RSA_free(rsa);
			errno = EIO;
			return -1;
		}
		/* rsa now owns dp_bn/dq_bn/qi_bn. */
		der_len = i2d_RSAPrivateKey(rsa, &der);
		RSA_free(rsa);
		if (der_len <= 0 || der == NULL) {
			errno = EIO;
			return -1;
		}
		*len_out = (size_t)der_len;
		if (private_der_out == NULL) {
			OPENSSL_free(der);
			return 0;
		}
		if ((size_t)der_len > cap) {
			OPENSSL_free(der);
			errno = ENOBUFS;
			return -1;
		}
		memcpy(private_der_out, der, (size_t)der_len);
		OPENSSL_free(der);
		return 0;
	}
#endif
}

/*
 * RSA-PSS sign / verify. Structurally identical to
 * rsa_pkcs1_v1_5_sign / _verify except that the EVP_PKEY_CTX is
 * captured from EVP_Digest{Sign,Verify}Init and configured with
 * PSS padding, saltLength, and MGF1 tied to the same hash as the
 * main digest — WebCrypto §21.2 says MGF1 must match the hash.
 */
int
jscrypto_rsa_pss_sign(const uint8_t *private_der, size_t private_len,
		jscrypto_digest_algorithm_t hash, uint32_t salt_length_bytes,
		const uint8_t *data, size_t data_len,
		uint8_t *signature_out, size_t sig_cap, size_t *sig_len_out)
{
	if (signature_out == NULL || sig_len_out == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)private_der;
	(void)private_len;
	(void)hash;
	(void)salt_length_bytes;
	(void)sig_cap;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		RSA *rsa = NULL;
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		EVP_PKEY_CTX *pctx = NULL;
		size_t out_len = sig_cap;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		rsa = jscrypto_rsa_from_private_der(private_der, private_len);
		if (rsa == NULL) {
			return -1;
		}
		pkey = EVP_PKEY_new();
		if (pkey == NULL || EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
			EVP_PKEY_free(pkey);
			RSA_free(rsa);
			errno = ENOMEM;
			return -1;
		}
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestSignInit(ctx, &pctx, md, NULL, pkey) != 1
				|| EVP_PKEY_CTX_set_rsa_padding(pctx,
					RSA_PKCS1_PSS_PADDING) <= 0
				|| EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx,
					(int)salt_length_bytes) <= 0
				|| EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) <= 0
				|| EVP_DigestSignUpdate(ctx, data, data_len) != 1
				|| EVP_DigestSignFinal(ctx, signature_out, &out_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		*sig_len_out = out_len;
		return 0;
	}
#endif
}

int
jscrypto_rsa_pss_verify(const uint8_t *public_der, size_t public_len,
		jscrypto_digest_algorithm_t hash, uint32_t salt_length_bytes,
		const uint8_t *data, size_t data_len,
		const uint8_t *signature, size_t signature_len, int *matches_out)
{
	if (matches_out == NULL || signature == NULL
			|| (data == NULL && data_len > 0)) {
		errno = EINVAL;
		return -1;
	}
	*matches_out = 0;
#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)public_der;
	(void)public_len;
	(void)hash;
	(void)salt_length_bytes;
	(void)signature_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(hash);
		RSA *rsa = NULL;
		EVP_PKEY *pkey = NULL;
		EVP_MD_CTX *ctx = NULL;
		EVP_PKEY_CTX *pctx = NULL;
		int verify_rc;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		rsa = jscrypto_rsa_from_public_der(public_der, public_len);
		if (rsa == NULL) {
			return -1;
		}
		pkey = EVP_PKEY_new();
		if (pkey == NULL || EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
			EVP_PKEY_free(pkey);
			RSA_free(rsa);
			errno = ENOMEM;
			return -1;
		}
		ctx = EVP_MD_CTX_new();
		if (ctx == NULL) {
			EVP_PKEY_free(pkey);
			errno = ENOMEM;
			return -1;
		}
		if (EVP_DigestVerifyInit(ctx, &pctx, md, NULL, pkey) != 1
				|| EVP_PKEY_CTX_set_rsa_padding(pctx,
					RSA_PKCS1_PSS_PADDING) <= 0
				|| EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx,
					(int)salt_length_bytes) <= 0
				|| EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md) <= 0
				|| EVP_DigestVerifyUpdate(ctx, data, data_len) != 1) {
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			errno = EIO;
			return -1;
		}
		verify_rc = EVP_DigestVerifyFinal(ctx, signature, signature_len);
		EVP_MD_CTX_free(ctx);
		EVP_PKEY_free(pkey);
		if (verify_rc == 1) {
			*matches_out = 1;
			return 0;
		}
		if (verify_rc == 0) {
			*matches_out = 0;
			return 0;
		}
		errno = EIO;
		return -1;
	}
#endif
}

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#pragma GCC diagnostic pop
#endif
