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

#include "secfile_load_agent.h"
#include <limits.h>
#include <securec.h>
#include <sys/prctl.h>
#include "tc_ns_client.h"
#include "tee_log.h"

#define MAX_PATH_LEN 256
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "teecd_agent"
#define MAX_BUFFER_LEN (8 * 1024 * 1024)
int g_secLoadAgentFd = -1;

int GetSecLoadAgentFd(void)
{
    return g_secLoadAgentFd;
}

void SetSecLoadAgentFd(int secLoadAgentFd)
{
    g_secLoadAgentFd = secLoadAgentFd;
}

static int GetImgLen(FILE *fp, long *totalLlen)
{
    int ret;

    ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) {
        tloge("fseek error\n");
        return -1;
    }
    *totalLlen = ftell(fp);
    if (*totalLlen <= 0 || *totalLlen > MAX_BUFFER_LEN) {
        tloge("file is not exist or size is too large, filesize = %ld\n", *totalLlen);
        return -1;
    }
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        tloge("fseek error\n");
        return -1;
    }
    return ret;
}

static int32_t SecFileLoadWork(int tzFd, const char *filePath, enum SecFileType fileType, const TEEC_UUID *uuid)
{
    char realPath[PATH_MAX + 1] = { 0 };
    FILE *fp                    = NULL;
    int ret;

    if (tzFd < 0) {
        tloge("fd of tzdriver is valid!\n");
        return -1;
    }
    if (realpath(filePath, realPath) == NULL) {
        tloge("realpath open file err=%d, filePath=%s\n", errno, filePath);
        return -1;
    }
    if (strncmp(realPath, TEE_DEFAULT_PATH, strlen(TEE_DEFAULT_PATH)) != 0) {
        tloge("realpath -%s- is invalid\n", realPath);
        return -1;
    }
    fp = fopen(realPath, "r");
    if (fp == NULL) {
        tloge("open file err=%d, path=%s\n", errno, filePath);
        return -1;
    }
    ret = LoadSecFile(tzFd, fp, fileType, uuid);
    if (fp != NULL) {
        fclose(fp);
    }
    return ret;
}

// input param uuid may be NULL, so don need to check if uuid is NULL
int32_t LoadSecFile(int tzFd, FILE *fp, enum SecFileType fileType, const TEEC_UUID *uuid)
{
    int32_t ret;
    char *fileBuffer                   = NULL;
    struct SecLoadIoctlStruct ioctlArg = { 0, { 0 }, 0, { NULL } };

    if (tzFd < 0 || fp == NULL) {
        tloge("param erro!\n");
        return -1;
    }

    do {
        long totalLen = 0;
        ret           = GetImgLen(fp, &totalLen);
        if (ret != 0) {
            break;
        }

        if (totalLen <= 0 || totalLen > MAX_BUFFER_LEN) {
            ret = -1;
            tloge("totalLen is invalid\n");
            break;
        }

        /* alloc a less than 8M heap memory, it needn't slice. */
        fileBuffer = malloc((size_t)totalLen);
        if (fileBuffer == NULL) {
            tloge("alloc TA file buffer(size=%ld) failed\n", totalLen);
            ret = -1;
            break;
        }

        /* read total ta file to file buffer */
        long fileSize = (long)fread(fileBuffer, 1, totalLen, fp);
        if (fileSize != totalLen) {
            tloge("read ta file failed, read size/total size=%ld/%ld\n", fileSize, totalLen);
            ret = -1;
            break;
        }
        ioctlArg.fileType   = fileType;
        ioctlArg.fileSize   = (uint32_t)totalLen;
        ioctlArg.fileBuffer = fileBuffer;
        if (uuid != NULL && memcpy_s((void *)(&ioctlArg.uuid), sizeof(ioctlArg.uuid), uuid, sizeof(*uuid)) != EOK) {
            tloge("memcpy uuid fail\n");
            ret = -1;
            break;
        }

        ret = ioctl(tzFd, (int)TC_NS_CLIENT_IOCTL_LOAD_APP_REQ, &ioctlArg);
        if (ret != 0) {
            tloge("ioctl to load sec file failed, ret = 0x%x\n", ret);
        }
    } while (false);

    if (fileBuffer != NULL) {
        free(fileBuffer);
    }
    return ret;
}

static bool IsTaLib(const TEEC_UUID *uuid)
{
    char *chr = (char *)uuid;
    uint32_t i;

    for (i = 0; i < sizeof(*uuid); i++) {
        if (chr[i] != 0) {
            return true;
        }
    }
    return false;
}

static void LoadLib(struct SecAgentControlType *secAgentControl)
{
    int32_t ret;
    char fname[MAX_PATH_LEN] = { 0 };

    if (secAgentControl == NULL) {
        tloge("secAgentControl is null\n");
        return;
    }
    if (strnlen(secAgentControl->LibSec.libName, MAX_SEC_FILE_NAME_LEN) >= MAX_SEC_FILE_NAME_LEN) {
        tloge("libName is too long!\n");
        secAgentControl->ret = -1;
        return;
    }

    if (IsTaLib(&(secAgentControl->LibSec.uuid))) {
        ret =
            snprintf_s(fname, sizeof(fname), MAX_PATH_LEN - 1,
                "%s/%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x%s.sec",
                TEE_DEFAULT_PATH, secAgentControl->LibSec.uuid.timeLow, secAgentControl->LibSec.uuid.timeMid,
                secAgentControl->LibSec.uuid.timeHiAndVersion, secAgentControl->LibSec.uuid.clockSeqAndNode[0],
                secAgentControl->LibSec.uuid.clockSeqAndNode[1], secAgentControl->LibSec.uuid.clockSeqAndNode[2],
                secAgentControl->LibSec.uuid.clockSeqAndNode[3], secAgentControl->LibSec.uuid.clockSeqAndNode[4],
                secAgentControl->LibSec.uuid.clockSeqAndNode[5], secAgentControl->LibSec.uuid.clockSeqAndNode[6],
                secAgentControl->LibSec.uuid.clockSeqAndNode[7], secAgentControl->LibSec.libName);
    } else {
        ret = snprintf_s(fname, sizeof(fname), MAX_PATH_LEN - 1,
            "%s/%s.sec", TEE_DEFAULT_PATH, secAgentControl->LibSec.libName);
    }
    if (ret < 0) {
        tloge("pack fname err\n");
        secAgentControl->ret = -1;
        return;
    }
    ret = SecFileLoadWork(g_secLoadAgentFd, (const char *)fname, LOAD_LIB, NULL);
    if (ret != 0) {
        tloge("teec load app failed\n");
        secAgentControl->ret   = -1;
        secAgentControl->error = errno;
        return;
    }
    secAgentControl->ret = 0;
    return;
}

static void LoadTa(struct SecAgentControlType *secAgentControl)
{
    int32_t ret;
    char fname[MAX_PATH_LEN] = { 0 };

    if (secAgentControl == NULL) {
        tloge("secAgentControl is null\n");
        return;
    }

    ret = snprintf_s(fname, sizeof(fname), MAX_PATH_LEN - 1, "%s/%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x.sec",
                     TEE_DEFAULT_PATH, secAgentControl->TaSec.uuid.timeLow, secAgentControl->TaSec.uuid.timeMid,
                     secAgentControl->TaSec.uuid.timeHiAndVersion, secAgentControl->TaSec.uuid.clockSeqAndNode[0],
                     secAgentControl->TaSec.uuid.clockSeqAndNode[1], secAgentControl->TaSec.uuid.clockSeqAndNode[2],
                     secAgentControl->TaSec.uuid.clockSeqAndNode[3], secAgentControl->TaSec.uuid.clockSeqAndNode[4],
                     secAgentControl->TaSec.uuid.clockSeqAndNode[5], secAgentControl->TaSec.uuid.clockSeqAndNode[6],
                     secAgentControl->TaSec.uuid.clockSeqAndNode[7]);
    if (ret < 0) {
        tloge("pack fname err\n");
        secAgentControl->ret = -1;
        return;
    }
    secAgentControl->ret = 0;
    ret = SecFileLoadWork(g_secLoadAgentFd, (const char *)fname, LOAD_TA, &(secAgentControl->TaSec.uuid));
    if (ret != 0) {
        tloge("teec load TA app failed\n");
        secAgentControl->ret   = ret;
        secAgentControl->error = errno;
        return;
    }
    return;
}

static void SecLoadAgentWork(struct SecAgentControlType *secAgentControl)
{
    if (secAgentControl == NULL) {
        tloge("secAgentControl is null\n");
        return;
    }
    switch (secAgentControl->cmd) {
        case LOAD_LIB_SEC:
            LoadLib(secAgentControl);
            break;
        case LOAD_TA_SEC:
            LoadTa(secAgentControl);
            break;
        case LOAD_SERVICE_SEC:
        default:
            tloge("gtask agent error cmd:%d\n", secAgentControl->cmd);
            secAgentControl->ret = -1;
            break;
    }
}

void *SecfileLoadAgentThread(void *control)
{
    (void)prctl(PR_SET_NAME, "teecd_sec_load_agent", 0, 0, 0);
    struct SecAgentControlType *secAgentControl = NULL;
    if (control == NULL) {
        tloge("control is NULL\n");
        return NULL;
    }
    secAgentControl = (struct SecAgentControlType *)control;
    if (g_secLoadAgentFd < 0) {
        tloge("m_gtask_agent_fd is -1\n");
        return NULL;
    }
    secAgentControl->magic = SECFILE_LOAD_AGENT_ID;
    while (true) {
        int ret = ioctl(g_secLoadAgentFd, (int)TC_NS_CLIENT_IOCTL_WAIT_EVENT, SECFILE_LOAD_AGENT_ID);
        if (ret) {
            tloge("gtask agent wait event failed, errno = %d\n", errno);
            break;
        }
        SecLoadAgentWork(secAgentControl);

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");

        secAgentControl->magic = SECFILE_LOAD_AGENT_ID;

        __asm__ volatile("isb");
        __asm__ volatile("dsb sy");
        ret = ioctl(g_secLoadAgentFd, (int)TC_NS_CLIENT_IOCTL_SEND_EVENT_RESPONSE, SECFILE_LOAD_AGENT_ID);
        if (ret) {
            tloge("gtask agent send reponse failed\n");
            break;
        }
    }
    return NULL;
}
