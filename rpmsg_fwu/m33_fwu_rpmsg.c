/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2026 STMicroelectronics
 */

#define _GNU_SOURCE

#include "m33_fwu_rpmsg.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <limits.h>

#define DEFAULT_TIMEOUT_MS 3000

static const fwu_component_map_t g_components[] = {
    { "tfm_s_ns", 0, 0x00000000UL },
    { "ddr_fw",   1, 0x00900000UL },
    /* alias */
    { "m33fw",    0, 0x00000000UL },
    { "m33ddr",   1, 0x00900000UL },
};

static void fwu_init_cmd(fwu_rpmsg_cmd_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->component_id = FWU_COMPONENT_ID_ALL;
}

void fwu_print_usage(const char *prog)
{
    printf(
        "usage: %s <command> [options]\n"
        "\n"
        "Commands:\n"
        "   list                          Return the number of supported components\n"
        "   info        [-c <component>]  Show current image information\n"
        "                                  for all components or one component\n"
        "   cancel      [-c <component>]  Select the affected component and disable\n"
        "                                  it in the pending update flow\n"
        "   install     [-c <component>]  Select the affected component and enable\n"
        "                                  it in the pending update flow\n"
        "   reboot                        Request M33 to stop Linux, apply the FWU\n"
        "                                  sequence, then reboot the platform\n"
        "   accept                        Accept all components currently in trial\n"
        "   reject                        Reject all components currently in trial\n"
        "   write       -c <component> -b <binary_file>\n"
        "                                 Copy a binary file to the shared memory\n"
        "                                  area between Linux and M33\n"
        "\n"
        "Components:\n"
        "   tfm_s_ns                      Secure firmware\n"
        "   ddr_fw                        DDR firmware\n"
        "   m33fw                         Alias for tfm_s_ns\n"
        "   m33ddr                        Alias for ddr_fw\n"
        "\n"
        "Examples:\n"
        "   %s info\n"
        "   %s info -c tfm_s_ns\n"
        "   %s cancel -c ddr_fw\n"
        "   %s install -c m33fw\n"
        "   %s write -c m33fw -b /home/root/download/tfm_s_ns.bin\n"
        "   %s write -b /home/root/download/ddr_fw.bin -c m33ddr\n"
        "   %s accept\n"
        "   %s reboot\n"
        "\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog
    );
}

const fwu_component_map_t *fwu_find_component_by_name(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(g_components) / sizeof(g_components[0]); ++i) {
        if (strcmp(g_components[i].name, name) == 0) {
            return &g_components[i];
        }
    }
    return NULL;
}

const char *fwu_component_name_from_id(uint32_t cid)
{
    switch (cid) {
    case 0: return "tfm_s_ns";
    case 1: return "ddr_fw";
    case FWU_COMPONENT_ID_ALL: return "ALL";
    case FWU_COMPONENT_ID_UNKNOWN: return "UNKNOWN";
    default: return "comp_id=?";
    }
}

static int fwu_find_rpmsg_device(char *out, size_t out_sz)
{
    DIR *d = opendir("/sys/class/rpmsg");
    struct dirent *de;

    if (!d) {
        return -1;
    }

    while ((de = readdir(d)) != NULL) {
        char namepath[PATH_MAX];
        char name[128];
        FILE *f;

        if (strncmp(de->d_name, "rpmsg", 5) != 0) {
            continue;
        }

        snprintf(namepath, sizeof(namepath), "/sys/class/rpmsg/%s/name", de->d_name);
        f = fopen(namepath, "r");
        if (!f) {
            continue;
        }

        if (!fgets(name, sizeof(name), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        name[strcspn(name, "\r\n")] = '\0';

        if (strcmp(name, RPMSG_ENDPOINT_NAME) == 0) {
            snprintf(out, out_sz, "/dev/%s", de->d_name);
            closedir(d);
            return 0;
        }
    }

    closedir(d);
    return -1;
}

static int fwu_create_rpmsg_device(void)
{
    int ret = system("rpmsg_create_ept /dev/rpmsg_ctrl0 fwu 0x5B > /dev/null 2>&1");
    if (ret == -1) {
        return -1;
    }
    return (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) ? 0 : -1;
}

int fwu_open_rpmsg_device(char *dev_path, size_t dev_path_sz)
{
    int fd;

    if (fwu_find_rpmsg_device(dev_path, dev_path_sz) != 0) {
        fprintf(stderr, "No RPMsg endpoint '%s' found, creating it...\n", RPMSG_ENDPOINT_NAME);
        if (fwu_create_rpmsg_device() != 0) {
            fprintf(stderr, "Error: unable to create RPMsg endpoint\n");
            return -1;
        }
        if (fwu_find_rpmsg_device(dev_path, dev_path_sz) != 0) {
            fprintf(stderr, "Error: endpoint created but device not found\n");
            return -1;
        }
    }

    fd = open(dev_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open RPMsg device");
        return -1;
    }

    return fd;
}

static int fwu_find_uio_device(char *dev_path, size_t dev_path_sz,
                               char *size_path, size_t size_path_sz)
{
    DIR *d = opendir("/sys/class/uio");
    struct dirent *de;

    if (!d) {
        return -1;
    }

    while ((de = readdir(d)) != NULL) {
        char namepath[PATH_MAX];
        char name[128];
        FILE *f;

        if (strncmp(de->d_name, "uio", 3) != 0) {
            continue;
        }

        snprintf(namepath, sizeof(namepath), "/sys/class/uio/%s/name", de->d_name);
        f = fopen(namepath, "r");
        if (!f) {
            continue;
        }

        if (!fgets(name, sizeof(name), f)) {
            fclose(f);
            continue;
        }
        fclose(f);

        name[strcspn(name, "\r\n")] = '\0';

        if (strcmp(name, UIO_DEVICE_NAME) == 0) {
            snprintf(dev_path, dev_path_sz, "/dev/%s", de->d_name);
            snprintf(size_path, size_path_sz, "/sys/class/uio/%s/maps/map0/size", de->d_name);
            closedir(d);
            return 0;
        }
    }

    closedir(d);
    return -1;
}

static int fwu_read_file_to_buffer(const char *path, uint8_t **buf, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *p;

    if (!f) {
        perror("fopen binary file");
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    p = (uint8_t *)malloc((size_t)sz);
    if (!p && sz > 0) {
        fclose(f);
        return -1;
    }

    if (sz > 0 && fread(p, 1, (size_t)sz, f) != (size_t)sz) {
        free(p);
        fclose(f);
        return -1;
    }

    fclose(f);
    *buf = p;
    *len = (size_t)sz;
    return 0;
}

int fwu_write_binary_to_uio(const char *component_name, const char *binary_path)
{
    const fwu_component_map_t *c = fwu_find_component_by_name(component_name);
    char dev_path[PATH_MAX];
    char size_path[PATH_MAX];
    FILE *f;
    long map_size;
    int fd;
    void *map;
    uint8_t *src = NULL;
    size_t src_len = 0;

    if (!c) {
        fprintf(stderr, "Unknown component: %s\n", component_name);
        return -1;
    }

    if (fwu_read_file_to_buffer(binary_path, &src, &src_len) != 0) {
        fprintf(stderr, "Cannot read file: %s\n", binary_path);
        return -1;
    }

    if (fwu_find_uio_device(dev_path, sizeof(dev_path), size_path, sizeof(size_path)) != 0) {
        fprintf(stderr, "No UIO device named '%s' found\n", UIO_DEVICE_NAME);
        free(src);
        return -1;
    }

    f = fopen(size_path, "r");
    if (!f) {
        perror("fopen size");
        free(src);
        return -1;
    }

    if (fscanf(f, "%lx", &map_size) != 1) {
        fprintf(stderr, "Failed to read UIO map size\n");
        fclose(f);
        free(src);
        return -1;
    }
    fclose(f);

    printf("File: %s, size = %zu bytes\n", binary_path, src_len);
    printf("Component: %s\n", component_name);
    printf("Destination offset: 0x%lx\n", (unsigned long)c->offset);
    printf("Map0 size from sysfs: 0x%lx\n", map_size);

    if ((unsigned long)c->offset > (unsigned long)map_size ||
        (unsigned long)src_len > (unsigned long)(map_size - c->offset)) {
        fprintf(stderr, "file size + offset exceeds UIO map size\n");
        free(src);
        return -1;
    }

    fd = open(dev_path, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open UIO device");
        free(src);
        return -1;
    }

    map = mmap(NULL, (size_t)map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        free(src);
        return -1;
    }

    printf("map OK, copying file...\n");
    memcpy((uint8_t *)map + c->offset, src, src_len);
    printf("Copy completed.\n");

    munmap(map, (size_t)map_size);
    close(fd);
    free(src);
    return 0;
}

static int fwu_write_full(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t done = 0;

    while (done < len) {
        ssize_t w = write(fd, p + done, len - done);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)w;
    }

    return 0;
}

static int fwu_read_rpmsg_message(int fd, fwu_rpmsg_cmd_t *rsp, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    uint8_t buf[sizeof(fwu_rpmsg_cmd_t)];
    ssize_t r;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) {
        return 0;
    }

    r = read(fd, buf, sizeof(buf));
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }

    if ((size_t)r < sizeof(fwu_rpmsg_cmd_t)) {
        return 0;
    }

    memcpy(rsp, buf, sizeof(fwu_rpmsg_cmd_t));
    return 1;
}

void fwu_handle_response(const fwu_rpmsg_cmd_t *rsp)
{
    const char *name = fwu_component_name_from_id(rsp->component_id);

    switch (rsp->command) {
    case FWU_RPMSG_CMD_LIST:
        printf("LIST: component count = %u\n", rsp->component_id);
      break;
    case FWU_RPMSG_CMD_INFO:
        if (rsp->error != 0) {
            printf("INFO: %s: error=%d\n", name, rsp->error);
        } else {
            printf("INFO: %s: v%u.%u.%u:%u\n",
                   name,
                   rsp->info.major,
                   rsp->info.minor,
                   rsp->info.patch,
                   rsp->info.build);
        }
        break;
    case FWU_RPMSG_CMD_ACCEPT:
        printf("ACCEPT: status=%d\n", rsp->error);
        break;
    case FWU_RPMSG_CMD_REJECT:
        printf("REJECT: status=%d\n", rsp->error);
        break;
    case FWU_RPMSG_CMD_INSTALL:
        printf("INSTALL: %s, status=%d\n", name, rsp->error);
        break;
    case FWU_RPMSG_CMD_CANCEL:
        printf("CANCEL: %s, status=%d\n", name, rsp->error);
        break;
    case FWU_RPMSG_CMD_REBOOT:
        printf("REBOOT: status=%d\n", rsp->error);
        break;
    default:
        printf("cmd=%u, comp=%s, status=%d\n",
               rsp->command, name, rsp->error);
        break;
    }
}

int fwu_send_cmd_and_wait(int fd, const fwu_rpmsg_cmd_t *cmd, int expected_msgs)
{
    int i;

    if (fwu_write_full(fd, cmd, sizeof(*cmd)) != 0) {
        perror("write RPMsg");
        return -1;
    }

    for (i = 0; i < expected_msgs; ++i) {
        fwu_rpmsg_cmd_t rsp;
        int r = fwu_read_rpmsg_message(fd, &rsp, DEFAULT_TIMEOUT_MS);

        if (r < 0) {
            perror("read RPMsg");
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "Timeout waiting for response %d/%d\n", i + 1, expected_msgs);
            return -1;
        }

        fwu_handle_response(&rsp);
    }

    return 0;
}

int fwu_get_component_count(int fd, uint32_t *count_out)
{
    fwu_rpmsg_cmd_t cmd;
    fwu_rpmsg_cmd_t rsp;

    fwu_init_cmd(&cmd);
    cmd.command = FWU_RPMSG_CMD_LIST;
    cmd.component_id = FWU_COMPONENT_ID_ALL;

    if (fwu_write_full(fd, &cmd, sizeof(cmd)) != 0) {
        perror("write LIST");
        return -1;
    }

    for (;;) {
        int r = fwu_read_rpmsg_message(fd, &rsp, DEFAULT_TIMEOUT_MS);
        if (r < 0) {
            perror("read LIST");
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "Timeout waiting LIST response\n");
            return -1;
        }
        if (rsp.command == FWU_RPMSG_CMD_LIST) {
            *count_out = rsp.component_id;
            return 0;
        }
    }
}

int fwu_parse_command_line(int argc, char **argv, fwu_rpmsg_cmd_t *cmd,
                           const char **subcmd,
                           const char **component_name,
                           const char **binary_path)
{
    int i;
    bool is_write = false;

    if (argc < 2) {
        return -1;
    }

    *subcmd = argv[1];
    *component_name = NULL;
    *binary_path = NULL;
    fwu_init_cmd(cmd);

    if (strcmp(*subcmd, "list") == 0) {
        cmd->command = FWU_RPMSG_CMD_LIST;
    } else if (strcmp(*subcmd, "info") == 0) {
        cmd->command = FWU_RPMSG_CMD_INFO;
    } else if (strcmp(*subcmd, "cancel") == 0) {
        cmd->command = FWU_RPMSG_CMD_CANCEL;
    } else if (strcmp(*subcmd, "install") == 0) {
        cmd->command = FWU_RPMSG_CMD_INSTALL;
    } else if (strcmp(*subcmd, "reboot") == 0) {
        cmd->command = FWU_RPMSG_CMD_REBOOT;
    } else if (strcmp(*subcmd, "accept") == 0) {
        cmd->command = FWU_RPMSG_CMD_ACCEPT;
    } else if (strcmp(*subcmd, "reject") == 0) {
        cmd->command = FWU_RPMSG_CMD_REJECT;
    } else if (strcmp(*subcmd, "write") == 0) {
        is_write = true;
    } else if (strcmp(*subcmd, "help") == 0) {
        return 0;
    } else {
        return -1;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc || *component_name != NULL) {
                return -1;
            }
            *component_name = argv[++i];
        } else if (strcmp(argv[i], "-b") == 0) {
            if (!is_write || i + 1 >= argc || *binary_path != NULL) {
                return -1;
            }
            *binary_path = argv[++i];
        } else {
            return -1;
        }
    }

    if (*component_name) {
        const fwu_component_map_t *c = fwu_find_component_by_name(*component_name);
        if (!c) {
            return -1;
        }
        cmd->component_id = c->id;
    } else {
        cmd->component_id = FWU_COMPONENT_ID_ALL;
    }

    if (is_write && !*binary_path) {
        return -1;
    }

    if (!is_write && *binary_path) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    char rpmsg_path[PATH_MAX];
    int rpmsg_fd;
    fwu_rpmsg_cmd_t cmd;
    const char *subcmd;
    const char *component_name;
    const char *binary_path;
    uint32_t component_count = 0;
    int ret;

    if (fwu_parse_command_line(argc, argv, &cmd, &subcmd, &component_name, &binary_path) != 0) {
        fwu_print_usage(argv[0]);
        return 1;
    }

    if (strcmp(subcmd, "help") == 0) {
        fwu_print_usage(argv[0]);
        return 0;
    }

    if (strcmp(subcmd, "write") == 0) {
        ret = fwu_write_binary_to_uio(component_name, binary_path);
        return (ret == 0) ? 0 : 1;
    }

    rpmsg_fd = fwu_open_rpmsg_device(rpmsg_path, sizeof(rpmsg_path));
    if (rpmsg_fd < 0) {
        return 1;
    }

    printf("Connected to %s\n", rpmsg_path);

    if (strcmp(subcmd, "list") == 0) {
        ret = fwu_send_cmd_and_wait(rpmsg_fd, &cmd, 1);
        close(rpmsg_fd);
        return (ret == 0) ? 0 : 1;
    }

    if (strcmp(subcmd, "info") == 0 &&
        cmd.component_id == FWU_COMPONENT_ID_ALL) {

        if (fwu_get_component_count(rpmsg_fd, &component_count) != 0) {
            close(rpmsg_fd);
            return 1;
        }

        if (component_count == 0) {
            fprintf(stderr, "No component reported by M33\n");
            close(rpmsg_fd);
            return 1;
        }

        printf("M33 reports %u component(s)\n", component_count);
        ret = fwu_send_cmd_and_wait(rpmsg_fd, &cmd, (int)component_count);
        close(rpmsg_fd);
        return (ret == 0) ? 0 : 1;
    }

    if (strcmp(subcmd, "reboot") == 0) {
        ret = fwu_write_full(rpmsg_fd, &cmd, sizeof(cmd));
        close(rpmsg_fd);
        if (ret != 0) {
            perror("write RPMsg");
            return 1;
        }
        return 0;
    }

    ret = fwu_send_cmd_and_wait(rpmsg_fd, &cmd, 1);
    close(rpmsg_fd);
    return (ret == 0) ? 0 : 1;
}
