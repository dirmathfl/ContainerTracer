/**
 * @copyright "Container Tracer" which executes the container performance mesurements
 * Copyright (C) 2020 SuhoSon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @file docker-mq.c
 * @brief Message Queue를 생성 및 사용하는 방식이 구현되어 있습니다.
 * @author SuhoSon (ngeol564@gmail.com)
 * @version 0.1
 * @date 2020-08-19
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>

#include <trace_replay.h>
#include <log.h>
#include <driver/docker-driver.h>

/**
 * @brief Message Queue를 초기화하는 함수입니다.
 *
 * @param info 초기화를 진행하고자 하는 대상을 가리키는 구조체의 포인터입니다.
 *
 * @return 성공적으로 종료된 경우에는 메시지 큐의 ID가 반환되고, 그렇지 않은 경우에는 음수 값이 반환됩니다.
 */
static int __docker_mq_init(struct docker_info *info)
{
        char *mq_path;
        key_t mq_key = 0;
        int mqid = -1, ret = -1;

        mq_path = (char *)malloc(BASE_KEY_PATHNAME_LEN);
        if (!mq_path) {
                pr_info(ERROR, "Memory allocation fail. (\"%s\")", "mq_path");
                ret = -ENOMEM;
                goto exception;
        }
        snprintf(mq_path, BASE_KEY_PATHNAME_LEN, "/tmp/%s%s_%d",
                 info->cgroup_id, MSGQ_KEY_PATHNAME, info->pid);

        /* 파일이 존재하지 않는 경우에 파일을 생성합니다. */
        (void)close(open(mq_path, O_WRONLY | O_CREAT, 0));

        if (0 > (mq_key = ftok(mq_path, PROJECT_ID))) {
                pr_info(ERROR, "Key generation failed (name: %s, token: %c)\n",
                        mq_path, PROJECT_ID);
                ret = -ENOKEY;
                goto exception;
        }

        if (0 > (mqid = msgget(mq_key, IPC_CREAT | PROJECT_PERM))) {
                pr_info(ERROR, "Message Queue get failed (key: %d)\n", mq_key);
                ret = -EINVAL;
                goto exception;
        }

        free(mq_path);
        ret = mqid;
        return ret;
exception:
        if (mq_path) {
                free(mq_path);
        }
        if (0 <= mqid) {
                msgctl(mqid, IPC_RMID, NULL);
        }
        mqid = ret;
        return mqid;
}

/**
 * @brief Message Queue와 관련된 설정에 대해서 초기화를 진행하도록 합니다.
 *
 * @param info 초기화를 진행하고자 하는 대상을 가리키는 구조체의 포인터입니다.
 *
 * @return 정상 종료가 된 경우에는 0이 반환되고, 그렇지 않은 경우에는 음수 값이 반환됩니다.
 */
int docker_mq_init(struct docker_info *info)
{
        int mqid = -1;
        int ret = 0;

        assert(NULL != info);
        assert(0 != info->pid);

        if (0 > (mqid = __docker_mq_init(info))) {
                pr_info(ERROR,
                        "Message Queue initialization fail. (target pid :%d)\n",
                        info->pid);
                ret = mqid;
                goto exit;
        }
        pr_info(INFO, "Message Queue create success. (path: /tmp/%s%s_%d)\n",
                info->cgroup_id, SHM_KEY_PATHNAME, info->pid);

        info->mqid = mqid;

exit:
        return ret;
}

/**
 * @brief Message Queue로 부터 데이터를 가져오는 함수입니다.
 *
 * @param info 데이터를 가져와야 하는 곳을 가리키는 정보를 가진 구조체의 포인터에 해당합니다.
 * @param buffer 데이터가 실제 저장되는 버퍼를 가리킵니다.
 *
 * @return 정상 종료가 된 경우에는 0, 그렇지 않은 경우에는 음수 값이 반환됩니다.
 */
int docker_mq_get(const struct docker_info *info, void *buffer)
{
        struct realtime_msg rmsg;

        assert(NULL != buffer);
        assert(NULL != info);
        assert(-1 != info->mqid);

        if (0 > msgrcv(info->mqid, &rmsg, sizeof(struct realtime_log), 0, 0)) {
                pr_info(ERROR, "Cannot get message queue (mqid: %d)\n",
                        info->mqid);
                return -EFAULT;
        }
        memcpy(buffer, &rmsg.log, sizeof(struct realtime_log));
        return 0;
}

/**
 * @brief Message Queue의 할당된 내용을 해제하도록 합니다.
 *
 * @param info 할당 해제를 진행할 docker_info에 해당합니다.
 * @param flags 해제의 정도를 설정하는 flag에 해당합니다.
 */
void docker_mq_free(struct docker_info *info, int flags)
{
        int mqid;

        assert(NULL != info);

        mqid = info->mqid;

        if ((DOCKER_IPC_FREE & flags) && 0 <= mqid) {
                msgctl(mqid, IPC_RMID, NULL);
        }

        info->mqid = -1;
}
