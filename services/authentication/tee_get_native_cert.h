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

#ifndef TEE_GET_NATIVE_CERT_H
#define TEE_GET_NATIVE_CERT_H

#include "tee_auth_common.h"
#ifdef __cplusplus
extern "C" {
#endif

int TeeGetNativeCert(int caPid, unsigned int caUid, uint32_t *len, uint8_t *buffer);

#ifdef __cplusplus
}
#endif
#endif /* TEE_GET_NATIVE_CERT */
