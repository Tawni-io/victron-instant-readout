#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Manufacturer payload WITH Victron company ID (0xE1 0x02) at bytes 0..1.
// Decrypts ciphertext after key_check (AES-128-CTR, LE counter).
bool victron_ir_decrypt(const uint8_t* mfr_with_company, size_t len,
                        const uint8_t key[16],
                        uint8_t* out, size_t out_cap, size_t* out_len);
