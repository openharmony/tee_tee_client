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

#ifndef TEE_CLIENT_TYPE_H
#define TEE_CLIENT_TYPE_H
/**
 * @addtogroup TeeClient
 * @{
 *
 * @brief Provides APIs for the client applications (CAs) in the Rich Execution Environment (normal mode) to
 * access the trusted applications (TAs) in a Trusted Execution Environment (TEE).
 *
 * @syscap SystemCapability.Tee.TeeClient
 * @since 9
 * @version 1.0
 */

/**
 * @file tee_client_type.h
 *
 * @brief Defines basic data types and data structures.
 *
 * @since 9
 * @version 1.0
 */

#include <semaphore.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "tee_client_constants.h"

/**
 * @brief Defines the linked list type.
 *
 * @since 9
 * @version 1.0
 */
struct ListNode {
    struct ListNode *next;
    struct ListNode *prev;
};

/**
 * @brief Defines the return values.
 *
 * @since 9
 * @version 1.0
 */
typedef enum TEEC_ReturnCode TEEC_Result;

/**
 * @brief Defines the universally unique identifier (UUID) as defined in RFC4122 [2]. The UUIDs are used to identify TAs.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    uint32_t timeLow;
    uint16_t timeMid;
    uint16_t timeHiAndVersion;
    uint8_t clockSeqAndNode[8];
} TEEC_UUID;

/**
 * @brief Defines the context, a logical connection between a CA and a TEE.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    int32_t fd;
    uint8_t *ta_path;
    struct ListNode session_list;
    struct ListNode shrd_mem_list;
    union {
        struct {
            void *buffer;
            sem_t buffer_barrier;
        } share_buffer;
        uint64_t imp;
    };
} TEEC_Context;

/**
 * @brief Defines the session between a CA and a TA.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    uint32_t session_id;
    TEEC_UUID service_id;
    uint32_t ops_cnt;
    union {
        struct ListNode head;
        uint64_t imp;
    };
    TEEC_Context *context;
} TEEC_Session;

/**
 * @brief Defines a shared memory block, which can be registered or allocated.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    void *buffer;
    uint32_t size;
    uint32_t flags;
    uint32_t ops_cnt;
    bool is_allocated;
    union {
        struct ListNode head;
        void* imp;
    };
    TEEC_Context *context;
} TEEC_SharedMemory;

/**
 * @brief Defines a pointer to a temporary buffer.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    void *buffer;
    uint32_t size;
} TEEC_TempMemoryReference;

/**
 * @brief Defines a pointer to the shared memory that is registered or allocated.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    TEEC_SharedMemory *parent;
    uint32_t size;
    uint32_t offset;
} TEEC_RegisteredMemoryReference;

/**
 * @brief Describes a parameter that carries small raw data passed by <b>value</b>.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    uint32_t a;
    uint32_t b;
} TEEC_Value;

/**
 * @brief Describes the size and handle of the ION memory.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    int ionShareFd;
    uint32_t ionSize;
} TEEC_IonReference;

/**
 * @brief Defines a parameter of {@code TEEC_Operation}.
 *
 * @since 9
 * @version 1.0
 */
typedef union {
    TEEC_TempMemoryReference tmpref;
    TEEC_RegisteredMemoryReference memref;
    TEEC_Value value;
    TEEC_IonReference ionref;
} TEEC_Parameter;

/**
 * @brief Defines the parameters for opening a session or sending a command.
 *
 * @since 9
 * @version 1.0
 */
typedef struct {
    /** The value 0 means to cancel the command, and other values mean to execute the command. */
    uint32_t started;
    /** Use {@code TEEC_PARAM_TYPES} to create this parameter. */
    uint32_t paramTypes;
    TEEC_Parameter params[TEEC_PARAM_NUM];
    TEEC_Session *session;
    bool cancel_flag;
} TEEC_Operation;

/** @} */
#endif
