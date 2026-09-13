#include "victron/ir_decrypt.h"

#include <string.h>
#include "mbedtls/aes.h"

namespace {

void ctr_inc_le(uint8_t counter[16]) {
  for (int i = 0; i < 16; i++) {
    if (++counter[i] != 0) break;
  }
}

}  // namespace

bool victron_ir_decrypt(const uint8_t* mfr_with_company, size_t len,
                        const uint8_t key[16],
                        uint8_t* out, size_t out_cap, size_t* out_len) {
  if (!mfr_with_company || !key || !out || !out_len) return false;
  // company(2) + prefix(2)+model(2)+type(1)+nonce(2)+key_check(1)+ct(>=1)
  if (len < 11) return false;
  if (mfr_with_company[0] != 0xE1 || mfr_with_company[1] != 0x02) return false;

  const uint8_t* container = mfr_with_company + 2;
  const size_t clen = len - 2;
  if (clen < 9) return false;
  if (container[7] != key[0]) return false;  // key_check

  const uint8_t* ct = container + 8;
  const size_t ct_len = clen - 8;
  if (ct_len == 0 || ct_len > out_cap) return false;

  uint8_t counter[16] = {0};
  counter[0] = container[5];  // nonce LE
  counter[1] = container[6];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }

  uint8_t stream[16];
  size_t offset = 0;
  while (offset < ct_len) {
    if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, counter, stream) != 0) {
      mbedtls_aes_free(&aes);
      return false;
    }
    const size_t n = (ct_len - offset > 16) ? 16 : (ct_len - offset);
    for (size_t i = 0; i < n; i++) {
      out[offset + i] = ct[offset + i] ^ stream[i];
    }
    offset += n;
    ctr_inc_le(counter);
  }

  mbedtls_aes_free(&aes);
  *out_len = ct_len;
  return true;
}
