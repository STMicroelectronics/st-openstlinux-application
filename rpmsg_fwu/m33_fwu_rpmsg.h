/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2026 STMicroelectronics
 */

#ifndef M33_FWU_RPMSG_H
#define M33_FWU_RPMSG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPMSG_CTRL_DEV "/dev/rpmsg_ctrl0"
#define RPMSG_ENDPOINT_NAME "fwu"
#define RPMSG_ENDPOINT_ADDR "0x5B"

#define UIO_DEVICE_NAME "uio-fwu-shmem"

/* FWU RPMsg command identifiers */
typedef enum
{
    FWU_RPMSG_CMD_INIT    = 0U,
    FWU_RPMSG_CMD_LIST    = 1U,
    FWU_RPMSG_CMD_INFO    = 2U,
    FWU_RPMSG_CMD_CANCEL  = 3U,
    FWU_RPMSG_CMD_INSTALL = 4U,
    FWU_RPMSG_CMD_REBOOT  = 5U,
    FWU_RPMSG_CMD_ACCEPT  = 6U,
    FWU_RPMSG_CMD_REJECT  = 7U,
} FWU_RpmsgCommandId_t;

/* Special component ID values */
#define FWU_COMPONENT_ID_ALL     (0xFFFFFFFFUL)
#define FWU_COMPONENT_ID_UNKNOWN  (0xFFFFFFFEUL)

/* shared memory layout */
#define SHARED_MEM_BASE           (0x81300000UL)
#define SHARED_TFM_S_NS_SIZE      (0x00900000UL) /* 9 MiB */
#define SHARED_DDR_FW_SIZE        (0x00040000UL) /* 256 KiB */
#define SHARED_MEM_LIMIT_OFFSET   (0x00A00000UL)

#define SHARED_TFM_S_NS_OFFSET    (0x0UL)
#define SHARED_DDR_FW_OFFSET      (SHARED_TFM_S_NS_SIZE)

#define SHARED_TFM_S_NS           (SHARED_MEM_BASE + SHARED_TFM_S_NS_OFFSET)
#define SHARED_DDR_FW             (SHARED_MEM_BASE + SHARED_DDR_FW_OFFSET)

typedef int32_t psa_status_t;

typedef struct psa_fwu_image_version_t {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
    uint32_t build;
} psa_fwu_image_version_t;

typedef struct fwu_rpmsg_cmd {
    uint32_t command;
    uint32_t component_id;
    psa_status_t error;
    psa_fwu_image_version_t info;
} fwu_rpmsg_cmd_t;

typedef struct fwu_component_map {
    const char *name;
    uint32_t id;
    uint32_t offset;
} fwu_component_map_t;

/* API */
void fwu_print_usage(const char *prog_name);

const fwu_component_map_t *fwu_find_component_by_name(const char *name);
const char *fwu_component_name_from_id(uint32_t cid);

int fwu_open_rpmsg_device(char *dev_path, size_t dev_path_sz);
int fwu_write_binary_to_uio(const char *component_name, const char *binary_path);

int fwu_send_cmd_and_wait(int fd, const fwu_rpmsg_cmd_t *cmd, int expected_msgs);
int fwu_get_component_count(int fd, uint32_t *count_out);

int fwu_parse_command_line(int argc, char **argv, fwu_rpmsg_cmd_t *cmd,
                           const char **subcmd,
                           const char **component_name,
                           const char **binary_path);

void fwu_handle_response(const fwu_rpmsg_cmd_t *rsp);

#ifdef __cplusplus
}
#endif

#endif /* M33_FWU_RPMSG_H */

