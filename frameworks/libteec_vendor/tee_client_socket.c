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

#include "tee_client_socket.h"
#include <errno.h>     /* for errno */
#include <sys/types.h> /* for open close */
#include <sys/ioctl.h> /* for ioctl */
#include <sys/socket.h>
#include <sys/un.h>
#include <securec.h>
#include <time.h>
#include "tee_log.h"
#include "tc_ns_client.h"
#include "tee_client_inner.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "libteec_vendor"

#define EACCES_ERR                (-3)
#define SEND_MESS_ERR             (-4)
#define IOV_LEN 1

static int ConnectTeecdSocket(int *socketFd)
{
    int ret;
    uint32_t len;
    struct sockaddr_un remote;

    if (socketFd == NULL) {
        tloge("bad parameter\n");
        return -1;
    }

    errno_t rc = memset_s(&remote, sizeof(remote), 0, sizeof(remote));
    if (rc != EOK) {
        return -1;
    }

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        tloge("can't open stream socket, errno=%" PUBLIC "d", errno);
        return -1;
    }

    tlogd("Trying to connect...\n");
    remote.sun_family = AF_UNIX;

    rc = strncpy_s(remote.sun_path, sizeof(remote.sun_path), TC_NS_SOCKET_NAME, sizeof(TC_NS_SOCKET_NAME));
    if (rc != EOK) {
        tloge("strncpy_s failed, errno=%" PUBLIC "d", rc);
        close(s);
        return -1;
    }
    len = (uint32_t)(strlen(remote.sun_path) + sizeof(remote.sun_family));
    remote.sun_path[0] = 0;

    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        tloge("connect() failed, errno=%" PUBLIC "d\n", errno);
        ret = -1;
        if (errno == EACCES) {
            ret = EACCES_ERR;
        }
        close(s);
        return ret;
    }
    tlogd("Connected.\n");

    *socketFd = s;
    return 0;
}

static int InitRecvMsg(struct msghdr *recvMsg, struct iovec *iov, size_t iovLen,
                       char *ctrlBuf, size_t ctrlBufLen)
{
    if (recvMsg == NULL || iov == NULL || ctrlBuf == NULL) {
        tloge("param error!\n");
        return EINVAL;
    }
    recvMsg->msg_iov        = iov;
    recvMsg->msg_iovlen     = iovLen;
    recvMsg->msg_name       = NULL;
    recvMsg->msg_namelen    = 0;
    recvMsg->msg_control    = ctrlBuf;
    recvMsg->msg_controllen = ctrlBufLen;
    return EOK;
}

/* Socket from which the file descriptor is read */
static int RecvFileDescriptor(int cmd, int socketFd)
{
    struct msghdr hmsg;
    struct iovec iov[IOV_LEN];
    struct cmsghdr *controlMsg = NULL;
    char ctrlBuf[CMSG_SPACE(sizeof(int))];
    char data[IOV_LEN] = "0";
    int version  = 0;
    ssize_t res;
    errno_t rc;
    int *cmdata = NULL;

    rc = memset_s(&hmsg, sizeof(struct msghdr), 0, sizeof(struct msghdr));
    if (rc != EOK) {
        return -1;
    }
    rc = memset_s(ctrlBuf, sizeof(ctrlBuf), 0, CMSG_SPACE(sizeof(int)));
    if (rc != EOK) {
        return -1;
    }
    /* For the dummy data */
    iov[0].iov_base = data;
    iov[0].iov_len  = sizeof(data);

    rc = InitRecvMsg(&hmsg, iov, IOV_LEN, ctrlBuf, CMSG_SPACE(sizeof(int)));
    if (rc != EOK) {
        tloge("init msg failed!\n");
        return -1;
    }

    if (cmd == GET_TEEVERSION) {
        iov[0].iov_base        = &version;
        iov[0].iov_len         = sizeof(int);
        hmsg.msg_control    = NULL;
        hmsg.msg_controllen = 0;
        res                    = recvmsg(socketFd, &hmsg, 0);
        if (res <= 0) {
            return -1;
        }
        return version;
    }

    res = recvmsg(socketFd, &hmsg, 0);
    if (res <= 0) {
        return -1;
    }

    /* Iterate through header to find if there is a file descriptor */
    for (controlMsg = CMSG_FIRSTHDR(&hmsg); controlMsg != NULL; controlMsg = CMSG_NXTHDR(&hmsg, controlMsg)) {
        if ((controlMsg->cmsg_level == SOL_SOCKET) && (controlMsg->cmsg_type == SCM_RIGHTS)) {
            cmdata = (int *)(uintptr_t)CMSG_DATA(controlMsg);
            return *cmdata;
        }
    }

    return -1;
}

static int FillMsgBuffer(const CaAuthInfo *caInfo, CaRevMsg **revMsg, int cmd)
{
    CaRevMsg *revBuffer = (CaRevMsg *)malloc(sizeof(*revBuffer));
    if (revBuffer == NULL) {
        tloge("alloc mem failed.\n");
        return (int)TEEC_ERROR_OUT_OF_MEMORY;
    }
    errno_t ret = memset_s((void *)revBuffer, sizeof(*revBuffer), 0, sizeof(*revBuffer));
    if (ret != EOK) {
        tloge("memset failed\n");
        free(revBuffer);
        return -1;
    }
    int temp = memcpy_s(&(revBuffer->caAuthInfo), sizeof(CaAuthInfo), caInfo, sizeof(*caInfo));
    if (temp != EOK) {
        tloge("memcpy_s error!\n");
        free(revBuffer);
        return -1;
    }
    revBuffer->cmd = cmd;
    *revMsg = revBuffer;
    return EOK;
}

static void InitSockMsg(struct msghdr *message, CaRevMsg *revMsg, struct iovec *iov)
{
    message->msg_name            = NULL;
    message->msg_namelen         = 0;
    message->msg_flags           = 0;
    message->msg_iov             = iov;
    message->msg_iovlen          = 1;
    message->msg_control         = NULL;
    message->msg_controllen      = 0;
    (message->msg_iov[0]).iov_base = revMsg;
    (message->msg_iov[0]).iov_len  = sizeof(*revMsg);
}

static void SleepNs(long num)
{
    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = num;

    if (nanosleep(&ts, NULL) != 0) {
        tlogd("nanosleep ms error\n");
    }
}


/* sleep 200 ms, and 50 times */
#define SLEEP_TIME (200 * 1000 * 1000)
#define SLEEP_COUNT 50
int CaDaemonConnectWithCaInfo(const CaAuthInfo *caInfo, int cmd)
{
    int s = -1;
    struct msghdr message;
    struct iovec iov[1];
    CaRevMsg *revMsg = NULL;
    int32_t failCount = 0; /* allow fail 50 times */
    int32_t cRet;

    if (caInfo == NULL) {
        tloge("ca daemon: ca auth info is NULL\n");
        return -1;
    }

    /* add retry to avoid app start before daemon */
    while (failCount++ < SLEEP_COUNT) {
        cRet = ConnectTeecdSocket(&s);
        if (cRet != -1) {
            break;
        }

        tlogd("open device failed, retry!\n");
        SleepNs(SLEEP_TIME);
    }

    if (cRet < 0) {
        tloge("try connect ca daemon failed, fail_counts = %" PUBLIC "d\n", failCount);
        return -1;
    }

    if (memset_s(&message, sizeof(message), 0, sizeof(message)) != EOK) {
        tloge("ca daemon: memset failed\n");
        close(s);
        return -1;
    }

    if (FillMsgBuffer(caInfo, &revMsg, cmd) != EOK) {
        tloge("memcpy_s error!\n");
        close(s);
        return -1;
    }

    /* For the dummy data */
    InitSockMsg(&message, revMsg, iov);
    if (sendmsg(s, &message, 0) < 0) {
        tloge("send message error %" PUBLIC "d \n", errno);
        close(s);
        free(revMsg);
        return SEND_MESS_ERR;
    }

    int fd = RecvFileDescriptor(cmd, s);
    if (fd >= 0) {
        tlogd("FD received!");
    }
    close(s);
    free(revMsg);
    return fd;
}