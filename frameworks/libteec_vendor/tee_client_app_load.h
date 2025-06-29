/*
 * Copyright (C) 2022 Huawei Technologies Co., Ltd.
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef LIBTEEC_TEE_CLIENT_APP_LOAD_H
#define LIBTEEC_TEE_CLIENT_APP_LOAD_H

#include <stdint.h>
#include <stdio.h>
#include "tc_ns_client.h"
#include "tee_client_inner.h"
#include "tee_client_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_PATH_LEN 40
#define MAX_FILE_NAME_LEN 40
#define MAX_FILE_EXT_LEN  6

#define MAX_IMAGE_LEN          0x800000 /* max image len */
#define MAX_SHARE_BUF_LEN      0x100000 /* max share buf len */
#define LOAD_IMAGE_FLAG_OFFSET 0x4
#define SEND_IMAGE_LEN         (MAX_SHARE_BUF_LEN - LOAD_IMAGE_FLAG_OFFSET)

#define TA_HEAD_MAGIC1         0xA5A55A5A
#define TA_HEAD_MAGIC2         0x55AA
#define TA_OH_HEAD_MAGIC2      0xAAAA
#define NUM_OF_RESERVED_BITMAP 16

enum TA_VERSION {
    TA_SIGN_VERSION        = 1, /* first version */
    TA_RSA2048_VERSION     = 2, /* use rsa 2048, and use right crypt mode */
    CIPHER_LAYER_VERSION   = 3,
    CONFIG_SEGMENT_VERSION = 4,
    TA_SIGN_VERSION_MAX
};

/* start: keep same with tee */
typedef struct {
    uint32_t contextLen;         /* manifest_crypto_len + cipher_bin_len */
    uint32_t manifestCryptoLen;  /* manifest crypto len */
    uint32_t manifestPlainLen;   /* manfiest str + manifest binary */
    uint32_t manifestStrLen;     /* manifest str len */
    uint32_t cipherBinLen;       /* cipher elf len */
    uint32_t signLen;            /* sign file len, now rsa 2048 this len is 256 */
} TeecImageHead;

typedef struct {
    uint32_t magicNum1;
    uint16_t magicNum2;
    uint16_t versionNum;
} TeecImageIdentity;

typedef struct {
    TeecImageIdentity imgIdentity;
    uint32_t contextLen;
    uint32_t taKeyVersion;
} TaImageHdrV3;

typedef struct {
    TeecImageHead imgHd;
    TeecImageIdentity imgIdentity;
    uint8_t reserved[NUM_OF_RESERVED_BITMAP];
} TeecTaHead;
/* end */

int32_t TEEC_GetApp(const TaFileInfo *taFile, const TEEC_UUID *srvUuid, TC_NS_ClientContext *cliContext);
int32_t TEEC_LoadSecfile(const char *filePath, int tzFd, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
