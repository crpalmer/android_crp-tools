/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This structure starts 16,384 bytes before the end of a hardware
 * partition that is encrypted, or in a separate partition.  It's location
 * is specified by a property set in init.<device>.rc.
 * The structure allocates 48 bytes for a key, but the real key size is
 * specified in the struct.  Currently, the code is hardcoded to use 128
 * bit keys.
 * The fields after salt are only valid in rev 1.1 and later stuctures.
 * Obviously, the filesystem does not include the last 16 kbytes
 * of the partition if the crypt_mnt_ftr lives at the end of the
 * partition.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* The current cryptfs version */
#define CURRENT_MAJOR_VERSION 1
#define CURRENT_MINOR_VERSION 3

#define CRYPT_FOOTER_OFFSET 0x4000
#define CRYPT_FOOTER_TO_PERSIST_OFFSET 0x1000
#define CRYPT_PERSIST_DATA_SIZE 0x1000

#define MAX_CRYPTO_TYPE_NAME_LEN 64

#define MAX_KEY_LEN 48
#define SALT_LEN 16
#define SCRYPT_LEN 32

/* definitions of flags in the structure below */
#define CRYPT_MNT_KEY_UNENCRYPTED 0x1 /* The key for the partition is not encrypted. */
#define CRYPT_ENCRYPTION_IN_PROGRESS 0x2 /* Encryption partially completed,
                                            encrypted_upto valid*/
#define CRYPT_INCONSISTENT_STATE 0x4 /* Set when starting encryption, clear when
                                        exit cleanly, either through success or
                                        correctly marked partial encryption */
#define CRYPT_DATA_CORRUPT 0x8 /* Set when encryption is fine, but the
                                  underlying volume is corrupt */
#define CRYPT_FORCE_ENCRYPTION 0x10 /* Set when it is time to encrypt this
                                       volume on boot. Everything in this
                                       structure is set up correctly as
                                       though device is encrypted except
                                       that the master key is encrypted with the
                                       default password. */
#define CRYPT_FORCE_COMPLETE 0x20 /* Set when the above encryption cycle is
                                     complete. On next cryptkeeper entry, match
                                     the password. If it matches fix the master
                                     key and remove this flag. */
#define CRYPT_ASCII_PASSWORD_UPDATED 0x1000

/* Allowed values for type in the structure below */
#define CRYPT_TYPE_PASSWORD 0 /* master_key is encrypted with a password
                               * Must be zero to be compatible with pre-L
                               * devices where type is always password.*/
#define CRYPT_TYPE_DEFAULT  1 /* master_key is encrypted with default
                               * password */
#define CRYPT_TYPE_PATTERN  2 /* master_key is encrypted with a pattern */
#define CRYPT_TYPE_PIN      3 /* master_key is encrypted with a pin */
#define CRYPT_TYPE_MAX_TYPE 3 /* type cannot be larger than this value */

#define CRYPT_MNT_MAGIC 0xD0B5B1C4
#define PERSIST_DATA_MAGIC 0xE950CD44

/* Key Derivation Function algorithms */
#define KDF_PBKDF2 1
#define KDF_SCRYPT 2
/* Algorithms 3 & 4 deprecated before shipping outside of google, so removed */
#define KDF_SCRYPT_KEYMASTER 5

/* Maximum allowed keymaster blob size. */
#define KEYMASTER_BLOB_SIZE 2048

/* __le32 and __le16 defined in system/extras/ext4_utils/ext4_utils.h */
#define __le8  unsigned char
#define __le16  unsigned short
#define __le32  unsigned int
#define __le64  unsigned long

#if !defined(SHA256_DIGEST_LENGTH)
#define SHA256_DIGEST_LENGTH 32
#endif

struct crypt_mnt_ftr {
  __le32 magic;         /* See above */
  __le16 major_version;
  __le16 minor_version;
  __le32 ftr_size;      /* in bytes, not including key following */
  __le32 flags;         /* See above */
  __le32 keysize;       /* in bytes */
  __le32 crypt_type;    /* how master_key is encrypted. Must be a
                         * CRYPT_TYPE_XXX value */
  __le64 fs_size;       /* Size of the encrypted fs, in 512 byte sectors */
  __le32 failed_decrypt_count; /* count of # of failed attempts to decrypt and
                                  mount, set to 0 on successful mount */
  unsigned char crypto_type_name[MAX_CRYPTO_TYPE_NAME_LEN]; /* The type of encryption
                                                               needed to decrypt this
                                                               partition, null terminated */
  __le32 spare2;        /* ignored */
/* 108 */
  unsigned char master_key[MAX_KEY_LEN]; /* The encrypted key for decrypting the filesystem */
/* 156 */
  unsigned char salt[SALT_LEN];   /* The salt used for this encryption */
/* 172 */
  __le64 persist_data_offset[2];  /* Absolute offset to both copies of crypt_persist_data
                                   * on device with that info, either the footer of the
                                   * real_blkdevice or the metadata partition. */

  __le32 persist_data_size;       /* The number of bytes allocated to each copy of the
                                   * persistent data table*/
/* 192 */

  __le8  kdf_type; /* The key derivation function used. */

  /* scrypt parameters. See www.tarsnap.com/scrypt/scrypt.pdf */
  __le8  N_factor; /* (1 << N) */
  __le8  r_factor; /* (1 << r) */
  __le8  p_factor; /* (1 << p) */
/* 196 */
  __le64 encrypted_upto; /* If we are in state CRYPT_ENCRYPTION_IN_PROGRESS and
                            we have to stop (e.g. power low) this is the last
                            encrypted 512 byte sector.*/
/* 204 */
  __le8  hash_first_block[SHA256_DIGEST_LENGTH]; /* When CRYPT_ENCRYPTION_IN_PROGRESS
                                                    set, hash of first block, used
                                                    to validate before continuing*/

  /* key_master key, used to sign the derived key which is then used to generate
   * the intermediate key
   * This key should be used for no other purposes! We use this key to sign unpadded 
   * data, which is acceptable but only if the key is not reused elsewhere. */
  __le8 keymaster_blob[KEYMASTER_BLOB_SIZE];
  __le32 keymaster_blob_size;

  /* Store scrypt of salted intermediate key. When decryption fails, we can
     check if this matches, and if it does, we know that the problem is with the
     drive, and there is no point in asking the user for more passwords.

     Note that if any part of this structure is corrupt, this will not match and
     we will continue to believe the user entered the wrong password. In that
     case the only solution is for the user to enter a password enough times to
     force a wipe.

     Note also that there is no need to worry about migration. If this data is
     wrong, we simply won't recognise a right password, and will continue to
     prompt. On the first password change, this value will be populated and
     then we will be OK.
   */
  unsigned char scrypted_intermediate_key[SCRYPT_LEN];

  /* sha of this structure with this element set to zero
     Used when encrypting on reboot to validate structure before doing something
     fatal
   */
  unsigned char sha256[SHA256_DIGEST_LENGTH];
};

static void dump_hex(const char *str, unsigned char *d, size_t l)
{
    printf("%s", str);
    while (l--) printf(" %02x", *d++);
    printf("\n");
}

int main(int argc, char **argv)
{
    struct crypt_mnt_ftr f;
    int i;
    int any_extra = 0;
    int skipped = 0;

    if (fread(&f, 1, sizeof(f), stdin) <= 0) {
	perror("<stdin>");
	exit(1);
    }

    printf("magic:                0x%x\n", f.magic);
    printf("version:              %d.%d\n", f.major_version, f.minor_version);
    printf("ftr_size:             %d\n", f.ftr_size);
    printf("flags:                0x%x\n", f.flags);
    printf("keysize:              %d\n", f.keysize);
    printf("crypt_type:           %d\n", f.crypt_type);
    printf("fs_size:              %ld\n", f.fs_size);
    printf("failed_decrypt_count: %d\n", f.failed_decrypt_count);
    printf("crypto_type_name      %s\n", f.crypto_type_name);
    printf("spare2:               %d\n", f.spare2);
    dump_hex("master_key:          ", f.master_key, MAX_KEY_LEN);
    dump_hex("salt:                ", f.salt, SALT_LEN);
    printf("persist_data_offset:  %ld, %ld\n", f.persist_data_offset[0], f.persist_data_offset[1]);
    printf("persist_data_size:    %d\n", f.persist_data_size);
    printf("kdf_type:             %d\n", f.kdf_type);
    printf("scrypt parameters:    %d, %d, %d\n", f.N_factor, f.r_factor, f.p_factor);
    printf("encrypted_upto:       %ld\n", f.encrypted_upto);
    dump_hex("hash_first_block:    ", f.hash_first_block, SHA256_DIGEST_LENGTH);
    dump_hex("keymaster_block:     ", f.keymaster_blob, KEYMASTER_BLOB_SIZE);
    printf("keymaster_blob_size:  %d\n", f.keymaster_blob_size);
    dump_hex("scrypted_int_key:    ", f.scrypted_intermediate_key, SCRYPT_LEN);
    dump_hex("sha256:              ", f.sha256, SHA256_DIGEST_LENGTH);
    while ((i = fgetc(stdin)) != EOF) {
        if (!i && !any_extra) {
            skipped++;
        } else if (i && !any_extra) {
            printf("Any extra data:\n");
            if (skipped) printf(" [%d zeros]", skipped);
            any_extra = 1;
            skipped = 0;
        } else if (!i) {
            skipped++;
        } else {
            if (skipped > 0 && skipped < 10) do printf(" 00"); while (--skipped);
            else if (skipped) printf(" [%d zeros]", skipped);
            skipped = 0;
            printf(" %02x", i);
        }
    }
    if (any_extra) {
        if (skipped < 10) do printf(" 00"); while (--skipped);
        else if (skipped) printf(" [%d zeros]", skipped);
        printf("\n");
    }

    return 0;
};
