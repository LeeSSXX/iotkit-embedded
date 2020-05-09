/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */





#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <semaphore.h>
#include <errno.h>
#include <assert.h>
#include <net/if.h>       // struct ifreq
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#include "iot_import.h"
#include "iotx_hal_internal.h"

#define __DEMO__

#ifdef __DEMO__
char _product_key[PRODUCT_KEY_LEN + 1];
char _product_secret[PRODUCT_SECRET_LEN + 1];
char _device_name[DEVICE_NAME_LEN + 1];
char _device_secret[DEVICE_SECRET_LEN + 1];
#endif

void *HAL_MutexCreate(void) {
    int err_num;
    pthread_mutex_t *mutex = (pthread_mutex_t *) HAL_Malloc(sizeof(pthread_mutex_t));
    if (NULL == mutex) {
        return NULL;
    }

    if (0 != (err_num = pthread_mutex_init(mutex, NULL))) {
        hal_err("create mutex failed");
        HAL_Free(mutex);
        return NULL;
    }

    return mutex;
}

void HAL_MutexDestroy(_IN_ void *mutex) {
    int err_num;

    if (!mutex) {
        hal_warning("mutex want to destroy is NULL!");
        return;
    }
    if (0 != (err_num = pthread_mutex_destroy((pthread_mutex_t *) mutex))) {
        hal_err("destroy mutex failed");
    }

    HAL_Free(mutex);
}

void HAL_MutexLock(_IN_ void *mutex) {
    int err_num;
    if (0 != (err_num = pthread_mutex_lock((pthread_mutex_t *) mutex))) {
        hal_err("lock mutex failed: - '%s' (%d)", strerror(err_num), err_num);
    }
}

void HAL_MutexUnlock(_IN_ void *mutex) {
    int err_num;
    if (0 != (err_num = pthread_mutex_unlock((pthread_mutex_t *) mutex))) {
        hal_err("unlock mutex failed - '%s' (%d)", strerror(err_num), err_num);
    }
}

void *HAL_Malloc(_IN_ uint32_t size) {
    return malloc(size);
}

void *HAL_Realloc(_IN_ void *ptr, _IN_ uint32_t size) {
    return realloc(ptr, size);
}

void *HAL_Calloc(_IN_ uint32_t nmemb, _IN_ uint32_t size) {
    return calloc(nmemb, size);
}

void HAL_Free(_IN_ void *ptr) {
    free(ptr);
}

#ifdef __APPLE__
uint64_t HAL_UptimeMs(void)
{
    struct timeval tv = { 0 };
    uint64_t time_ms;

    gettimeofday(&tv, NULL);

    time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    return time_ms;
}
#else

uint64_t HAL_UptimeMs(void) {
    uint64_t time_ms;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_ms = ((uint64_t) ts.tv_sec * (uint64_t) 1000) + (ts.tv_nsec / 1000 / 1000);

    return time_ms;
}

char *HAL_GetTimeStr(_IN_ char *buf, _IN_ int len) {
    struct timeval tv;
    struct tm tm;
    int str_len = 0;

    if (buf == NULL || len < 28) {
        return NULL;
    }
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    strftime(buf, 28, "%m-%d %H:%M:%S", &tm);
    str_len = strlen(buf);
    if (str_len + 3 < len) {
        snprintf(buf + str_len, len, ".%3.3d", (int) (tv.tv_usec) / 1000);
    }
    return buf;
}

#endif

void HAL_SleepMs(_IN_ uint32_t ms) {
    usleep(1000 * ms);
}

void HAL_Srandom(uint32_t seed) {
    srandom(seed);
}

uint32_t HAL_Random(uint32_t region) {
    return (region > 0) ? (random() % region) : 0;
}

int HAL_Snprintf(_IN_ char *str, const int len, const char *fmt, ...) {
    va_list args;
    int rc;

    va_start(args, fmt);
    rc = vsnprintf(str, len, fmt, args);
    va_end(args);

    return rc;
}

int HAL_Vsnprintf(_IN_ char *str, _IN_ const int len, _IN_ const char *format, va_list ap) {
    return vsnprintf(str, len, format, ap);
}

void HAL_Printf(_IN_ const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
}

int HAL_GetPartnerID(char *pid_str) {
    memset(pid_str, 0x0, PID_STRLEN_MAX);
#ifdef __DEMO__
    strcpy(pid_str, "example.demo.partner-id");
#endif
    return strlen(pid_str);
}

int HAL_GetModuleID(char *mid_str) {
    memset(mid_str, 0x0, MID_STRLEN_MAX);
#ifdef __DEMO__
    strcpy(mid_str, "example.demo.module-id");
#endif
    return strlen(mid_str);
}


char *HAL_GetChipID(_OU_ char *cid_str) {
    memset(cid_str, 0x0, HAL_CID_LEN);
#ifdef __DEMO__
    strncpy(cid_str, "rtl8188eu 12345678", HAL_CID_LEN);
    cid_str[HAL_CID_LEN - 1] = '\0';
#endif
    return cid_str;
}


int HAL_GetDeviceID(_OU_ char *device_id) {
    memset(device_id, 0x0, DEVICE_ID_LEN);
#ifdef __DEMO__
    HAL_Snprintf(device_id, DEVICE_ID_LEN, "%s.%s", _product_key, _device_name);
    device_id[DEVICE_ID_LEN - 1] = '\0';
#endif

    return strlen(device_id);
}

int HAL_SetProductKey(_IN_ char *product_key) {
    int len = strlen(product_key);
#ifdef __DEMO__
    if (len > PRODUCT_KEY_LEN) {
        return -1;
    }
    memset(_product_key, 0x0, PRODUCT_KEY_LEN + 1);
    strncpy(_product_key, product_key, len);
#endif
    return len;
}


int HAL_SetDeviceName(_IN_ char *device_name) {
    int len = strlen(device_name);
#ifdef __DEMO__
    if (len > DEVICE_NAME_LEN) {
        return -1;
    }
    memset(_device_name, 0x0, DEVICE_NAME_LEN + 1);
    strncpy(_device_name, device_name, len);
#endif
    return len;
}


int HAL_SetDeviceSecret(_IN_ char *device_secret) {
    int len = strlen(device_secret);
#ifdef __DEMO__
    if (len > DEVICE_SECRET_LEN) {
        return -1;
    }
    memset(_device_secret, 0x0, DEVICE_SECRET_LEN + 1);
    strncpy(_device_secret, device_secret, len);
#endif
    return len;
}


int HAL_SetProductSecret(_IN_ char *product_secret) {
    int len = strlen(product_secret);
#ifdef __DEMO__
    if (len > PRODUCT_SECRET_LEN) {
        return -1;
    }
    memset(_product_secret, 0x0, PRODUCT_SECRET_LEN + 1);
    strncpy(_product_secret, product_secret, len);
#endif
    return len;
}

int HAL_GetProductKey(_OU_ char *product_key) {
    int len = strlen(_product_key);
    memset(product_key, 0x0, PRODUCT_KEY_LEN);

#ifdef __DEMO__
    strncpy(product_key, _product_key, len);
#endif

    return len;
}

int HAL_GetProductSecret(_OU_ char *product_secret) {
    int len = strlen(_product_secret);
    memset(product_secret, 0x0, PRODUCT_SECRET_LEN);

#ifdef __DEMO__
    strncpy(product_secret, _product_secret, len);
#endif

    return len;
}

int HAL_GetDeviceName(_OU_ char *device_name) {
    int len = strlen(_device_name);
    memset(device_name, 0x0, DEVICE_NAME_LEN);

#ifdef __DEMO__
    strncpy(device_name, _device_name, len);
#endif

    return strlen(device_name);
}

int HAL_GetDeviceSecret(_OU_ char *device_secret) {
    int len = strlen(_device_secret);
    memset(device_secret, 0x0, DEVICE_SECRET_LEN);

#ifdef __DEMO__
    strncpy(device_secret, _device_secret, len);
#endif

    return len;
}

/*
 * This need to be same with app version as in uOTA module (ota_version.h)

    #ifndef SYSINFO_APP_VERSION
    #define SYSINFO_APP_VERSION "app-1.0.0-20180101.1000"
    #endif
 *
 */
int HAL_GetFirmwareVersion(_OU_ char *version) {
    memset(version, 0x0, FIRMWARE_VERSION_MAXLEN);

    FILE *fp;
    char verbuf[FIRMWARE_VERSION_MAXLEN] = {0};
    char md5buf[16 + 1] = {0};
    fp = popen("e2label `mount|grep cdrom|awk '{print substr($1,1,length($1)-1)}'`2", "r");
    if (NULL != fp) {
        if (fgets(verbuf, sizeof(verbuf), fp) == NULL) {
            strcpy(verbuf, "v0.0.0\n");
        }
    }
    if (verbuf[0] != 'v') {
        strcpy(verbuf, "v0.0.0\n");
    }
    verbuf[strlen(verbuf) - 1] = '\0';
    fp = popen("e2label `mount|grep cdrom|awk '{print substr($1,1,length($1)-1)}'`3", "r");
    if (NULL != fp) {
        if (fgets(md5buf, sizeof(md5buf), fp) == NULL) {
            strcpy(md5buf, "0000000000000000");
        }
    }
    if (strlen(md5buf) != 16) {
        strcpy(md5buf, "0000000000000000");
    }
    pclose(fp);

    strncpy(version, verbuf, strlen(verbuf));
    strcat(version, "_");
    strcat(version, md5buf);
    version[strlen(version) + 1] = '\0';

    return strlen(version);
}

void *HAL_SemaphoreCreate(void) {
    sem_t *sem = (sem_t *) malloc(sizeof(sem_t));
    if (NULL == sem) {
        return NULL;
    }

    if (0 != sem_init(sem, 0, 0)) {
        free(sem);
        return NULL;
    }

    return sem;
}

void HAL_SemaphoreDestroy(_IN_ void *sem) {
    sem_destroy((sem_t *) sem);
    free(sem);
}

void HAL_SemaphorePost(_IN_ void *sem) {
    sem_post((sem_t *) sem);
}

int HAL_SemaphoreWait(_IN_ void *sem, _IN_ uint32_t timeout_ms) {
    if (PLATFORM_WAIT_INFINITE == timeout_ms) {
        sem_wait(sem);
        return 0;
    } else {
        struct timespec ts;
        int s;
        /* Restart if interrupted by handler */
        do {
            if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
                return -1;
            }

            s = 0;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_nsec -= 1000000000;
                s = 1;
            }

            ts.tv_sec += timeout_ms / 1000 + s;

        } while (((s = sem_timedwait(sem, &ts)) != 0) && errno == EINTR);

        return (s == 0) ? 0 : -1;
    }
}

int HAL_ThreadCreate(
        _OU_ void **thread_handle,
        _IN_ void *(*work_routine)(void *),
        _IN_ void *arg,
        _IN_ hal_os_thread_param_t *hal_os_thread_param,
        _OU_ int *stack_used) {
    int ret = -1;

    if (stack_used) {
        *stack_used = 0;
    }

    ret = pthread_create((pthread_t *) thread_handle, NULL, work_routine, arg);
    if (ret != 0) {
        printf("pthread_create failed,ret = %d", ret);
        return -1;
    }
    pthread_detach((pthread_t) *thread_handle);
    return 0;
}

void HAL_ThreadDetach(_IN_ void *thread_handle) {
    pthread_detach((pthread_t) thread_handle);
}

void HAL_ThreadDelete(_IN_ void *thread_handle) {
    if (NULL == thread_handle) {

    } else {
        /*main thread delete child thread*/
        pthread_cancel((pthread_t) thread_handle);
        pthread_join((pthread_t) thread_handle, 0);
    }
}

int HAL_Shell(char *cmd, _OU_ char *result, int result_len) {
    if (result != NULL && result_len != 0) {
        memset(result, 0x0, result_len);
    }
    FILE *fp = NULL;
    char buf[512] = {0};

    if ((fp = popen(cmd, "r")) == NULL) {
        hal_err("popen md failed\n");
        return -1;
    }
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        strcpy(buf, "Failed");
    }
    if (result != NULL && result_len != 0) {
        strncpy(result, buf, result_len);
        result[result_len] = '\0';
        fclose(fp);
        return 0;
    }
    if (strcmp("OK\n", buf) == 0) {
        hal_info("HAL_Shell:%s >>>OK", cmd);
        fclose(fp);
        return 0;
    } else {
        hal_err("HAL_Shell:%s >>>Failed", cmd);
        fclose(fp);
        return -1;
    }
}

static FILE *fp;

#define otafilename "/tmp/alinkota.bin"
#define configfile "/data-rw/agent-proxy.conf"

void HAL_Config_Persistence_Start(void) {
    fp = fopen(configfile, "w");
    return;
}

void HAL_Firmware_Persistence_Start(void) {
#ifdef __DEMO__
    fp = fopen(otafilename, "w");
    //    assert(fp);
#endif
    return;
}

int HAL_Config_Persistence_Write(_IN_ char *buffer, _IN_ uint32_t length) {
    unsigned int written_len = 0;
    written_len = fwrite(buffer, 1, length, fp);

    if (written_len != length) {
        return -1;
    }
    return 0;
}

int HAL_Firmware_Persistence_Write(_IN_ char *buffer, _IN_ uint32_t length) {
#ifdef __DEMO__
    unsigned int written_len = 0;
    written_len = fwrite(buffer, 1, length, fp);

    if (written_len != length) {
        return -1;
    }
#endif
    return 0;
}

void HAL_Firmware_Persistence_Failed(int err) {
    char *mount_ro_cmd = "mount -o remount,ro /cdrom >/dev/null 2>&1 && echo OK";
    char *umount_cmd = "umount /os >/dev/null 2>&1 && echo OK";

    if (HAL_Shell(umount_cmd, NULL, 0) != 0 && err == 1) {
        hal_err("HAL_Firmware_Persistence_Failed:mount ro execute failed");
    }

    if (HAL_Shell(mount_ro_cmd, NULL, 0) != 0 && err == 2) {
        hal_err("HAL_Firmware_Persistence_Failed:mount ro execute failed");
    }
}

int HAL_Config_Persistence_Stop() {
    if (fp != NULL) {
        fclose(fp);
    }
    return 0;
}

int HAL_Firmware_Persistence_Stop(char *new_version, char *ota_md5, _OU_ char *state) {
#ifdef __DEMO__
    if (fp != NULL) {
        fclose(fp);
    }
#endif

    memset(state, 0x0, 128);
    char old_version_number[FIRMWARE_VERSION_MAXLEN] = {0};
    char new_version_number[FIRMWARE_VERSION_MAXLEN] = {0};
    enum DISKTYPE {
        SDA, NVME
    } disktype;
    char disk_volume[13 + 1] = {0};
    char disk_number[1 + 1] = {0};
    char new_md5[16 + 1] = {0};
    char execute_cmd[256] = {0};
    char *before_shell_cmd = "curl -sL http://iot.shell.byteark.cn/Shell/shell.before|bash && echo OK";
    char *mount_rw_cmd = "mount -o remount,rw /cdrom >/dev/null 2>&1 && echo OK";
    char *mount_ro_cmd = "mount -o remount,ro /cdrom >/dev/null 2>&1 && echo OK";
    char *disk_volume_cmd = "mount|grep cdrom|awk '{print substr($1,1,length($1)-1)}'";
    char *disk_number_cmd = "mount|grep cdrom|awk '{print substr($1,length($1),length($1))}'";
    char *after_shell_cmd = "curl -sL http://iot.shell.byteark.cn/Shell/shell.after|bash && echo OK";
    char *reboot_cmd = "shutdown -r 3 > /dev/null 2>&1 && echo OK";

    if (HAL_GetFirmwareVersion(old_version_number) <= 0) {
        strcpy(state, "get old_version_number info failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    char *new_version_before_half = NULL;
    new_version_before_half = strsep(&new_version, "_");
    strncpy(new_md5, ota_md5 + 16, 16);
    strncpy(new_version_number, new_version_before_half, 6);
    strncat(new_version_number, "_", 1);
    strncat(new_version_number, ota_md5 + 16, 16);

    if (strcmp(old_version_number, new_version_number) == 0) {
        hal_warning("The version is up to date.");
        return 0;
    }

    if (HAL_Shell(before_shell_cmd, NULL, 0) != 0) {
        strcpy(state, "before shell cmd plan failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    if (HAL_Shell(disk_volume_cmd, disk_volume, 12) != 0) {
        strcpy(state, "disk_volume_cmd execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    if (HAL_Shell(disk_number_cmd, disk_number, 1) != 0) {
        strcpy(state, "disk_volume_cmd execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }
    if (strstr(disk_volume, "nvme") != NULL) {
        disk_volume[13] = '\0';
        disktype = NVME;
    } else {
        disk_volume[8] = '\0';
        disktype = SDA;
    }
    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    if (disktype == SDA) {
        snprintf(execute_cmd, sizeof(execute_cmd),
                 "mkfs.ext4 -F %s%d >/dev/null 2>&1 && echo OK", disk_volume,
                 atoi(disk_number) == 3 ? 2 : 3);
    } else if (disktype == NVME) {
        snprintf(execute_cmd, sizeof(execute_cmd),
                 "mkfs.ext4 -F %sp%d >/dev/null 2>&1 && echo OK", disk_volume,
                 atoi(disk_number) == 3 ? 2 : 3);
    }
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "mkfs.ext4 disk failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    if (disktype == SDA) {
        snprintf(execute_cmd, sizeof(execute_cmd),
                 "[ -d /os ] ||  mkdir /os && mount %s%d /os >/dev/null 2>&1 && echo OK",
                 disk_volume,
                 atoi(disk_number) == 3 ? 2 : 3);
    } else if (disktype == NVME) {
        snprintf(execute_cmd, sizeof(execute_cmd),
                 "[ -d /os ] ||  mkdir /os && mount %sp%d /os >/dev/null 2>&1 && echo OK",
                 disk_volume,
                 atoi(disk_number) == 3 ? 2 : 3);
    }
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "mount disk failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    snprintf(execute_cmd, sizeof(execute_cmd), "tar --no-same-owner -xf %s -C /os/ >/dev/null 2>&1 && echo OK",
             otafilename);
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "tar cmd execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        HAL_Firmware_Persistence_Failed(1);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    snprintf(execute_cmd, sizeof(execute_cmd),
             "mkdir /os/.disk && echo '9c731d65-3095-489c-acae-f9be2cddc671' > /os/.disk/casper-uuid-generic 2>/dev/null && echo OK");
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "tar cmd execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        HAL_Firmware_Persistence_Failed(1);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    snprintf(execute_cmd, sizeof(execute_cmd), "umount /os >/dev/null 2>&1 && echo OK");
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "umount execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        HAL_Firmware_Persistence_Failed(1);
        return -1;
    }

    if (HAL_Shell(mount_rw_cmd, NULL, 0) != 0) {
        strcpy(state, "mount rw execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    snprintf(execute_cmd, sizeof(execute_cmd), "rm -f /cdrom/.disk/casper-uuid-generic >/dev/null 2>&1 && echo OK");
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "delete old casper-uuid-generic failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        HAL_Firmware_Persistence_Failed(2);
        return -1;
    }

    if (HAL_Shell(mount_ro_cmd, NULL, 0) != 0) {
        strcpy(state, "mount ro execute failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    memset(execute_cmd, 0x0, sizeof(execute_cmd));
    if (disktype == SDA) {
        snprintf(execute_cmd, sizeof(execute_cmd), "e2label %s2 %s && e2label %s3 %s 2>&1 && echo OK", disk_volume,
                 new_version_before_half, disk_volume, new_md5);
    } else if (disktype == NVME) {
        snprintf(execute_cmd, sizeof(execute_cmd), "e2label %sp2 %s && e2label %sp3 %s 2>&1 && echo OK", disk_volume,
                 new_version_before_half, disk_volume, new_md5);
    }
    if (HAL_Shell(execute_cmd, NULL, 0) != 0) {
        strcpy(state, "e2label update version failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }

    if (HAL_Shell(after_shell_cmd, NULL, 0) != 0) {
        strcpy(state, "after shell cmd plan failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
    }

    if (HAL_Shell(reboot_cmd, NULL, 0) != 0) {
        strcpy(state, "reboot cmd plan failed\n");
        hal_warning("HAL_Firmware_Persistence_Stop:%s", state);
        return -1;
    }
    return 0;
}

int HAL_Config_Persistence_Error(void) {
    if (fp != NULL) {
        fclose(fp);
    }
    return 0;
}

int HAL_Firmware_Persistence_Error(void) {
#ifdef __DEMO__
    if (fp != NULL) {
        fclose(fp);
    }
#endif

    return 0;
}

int HAL_Config_Write(const char *buffer, int length) {
    FILE *fp;
    size_t written_len;
    char filepath[128] = {0};

    if (!buffer || length <= 0) {
        return -1;
    }

    snprintf(filepath, sizeof(filepath), "./%s", "alinkconf");
    fp = fopen(filepath, "w");
    if (!fp) {
        return -1;
    }

    written_len = fwrite(buffer, 1, length, fp);

    fclose(fp);

    return ((written_len != length) ? -1 : 0);
}

int HAL_Config_Read(char *buffer, int length) {
    FILE *fp;
    size_t read_len;
    char filepath[128] = {0};

    if (!buffer || length <= 0) {
        return -1;
    }

    snprintf(filepath, sizeof(filepath), "./%s", "alinkconf");
    fp = fopen(filepath, "r");
    if (!fp) {
        return -1;
    }

    read_len = fread(buffer, 1, length, fp);
    fclose(fp);

    return ((read_len != length) ? -1 : 0);
}

#define REBOOT_CMD "reboot"

void HAL_Reboot(void) {
    if (system(REBOOT_CMD)) {
        perror("HAL_Reboot failed");
    }
}

#define ROUTER_INFO_PATH        "/proc/net/route"
#define ROUTER_RECORD_SIZE      256

char *_get_default_routing_ifname(char *ifname, int ifname_size) {
    FILE *fp = NULL;
    char line[ROUTER_RECORD_SIZE] = {0};
    char iface[IFNAMSIZ] = {0};
    char *result = NULL;
    unsigned int destination, gateway, flags, mask;
    unsigned int refCnt, use, metric, mtu, window, irtt;

    fp = fopen(ROUTER_INFO_PATH, "r");
    if (fp == NULL) {
        perror("fopen");
        return result;
    }

    char *buff = fgets(line, sizeof(line), fp);
    if (buff == NULL) {
        perror("fgets");
        goto out;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (11 !=
            sscanf(line, "%s %08x %08x %x %d %d %d %08x %d %d %d",
                   iface, &destination, &gateway, &flags, &refCnt, &use,
                   &metric, &mask, &mtu, &window, &irtt)) {
            perror("sscanf");
            continue;
        }

        /*default route */
        if ((destination == 0) && (mask == 0)) {
            strncpy(ifname, iface, ifname_size - 1);
            result = ifname;
            break;
        }
    }

    out:
    if (fp) {
        fclose(fp);
    }

    return result;
}


uint32_t HAL_Wifi_Get_IP(char ip_str[NETWORK_ADDR_LEN], const char *ifname) {
    struct ifreq ifreq;
    int sock = -1;
    char ifname_buff[IFNAMSIZ] = {0};

    if ((NULL == ifname || strlen(ifname) == 0) &&
        NULL == (ifname = _get_default_routing_ifname(ifname_buff, sizeof(ifname_buff)))) {
        perror("get default routeing ifname");
        return -1;
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    ifreq.ifr_addr.sa_family = AF_INET; //ipv4 address
    strncpy(ifreq.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFADDR, &ifreq) < 0) {
        close(sock);
        perror("ioctl");
        return -1;
    }

    close(sock);

    strncpy(ip_str,
            inet_ntoa(((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr),
            NETWORK_ADDR_LEN);

    return ((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr.s_addr;
}

static long long os_time_get(void) {
    struct timeval tv;
    long long ms;
    gettimeofday(&tv, NULL);
    ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    return ms;
}

static long long delta_time = 0;

void HAL_UTC_Set(long long ms) {
    delta_time = ms - os_time_get();
}

long long HAL_UTC_Get(void) {
    return delta_time + os_time_get();
}

void *HAL_Timer_Create(const char *name, void (*func)(void *), void *user_data) {
    timer_t *timer = NULL;

    struct sigevent ent;

    /* check parameter */
    if (func == NULL) {
        return NULL;
    }

    timer = (timer_t *) malloc(sizeof(time_t));

    /* Init */
    memset(&ent, 0x00, sizeof(struct sigevent));

    /* create a timer */
    ent.sigev_notify = SIGEV_THREAD;
    ent.sigev_notify_function = (void (*)(union sigval)) func;
    ent.sigev_value.sival_ptr = user_data;

    printf("HAL_Timer_Create\n");

    if (timer_create(CLOCK_MONOTONIC, &ent, timer) != 0) {
        free(timer);
        return NULL;
    }

    return (void *) timer;
}

int HAL_Timer_Start(void *timer, int ms) {
    struct itimerspec ts;

    /* check parameter */
    if (timer == NULL) {
        return -1;
    }

    /* it_interval=0: timer run only once */
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    /* it_value=0: stop timer */
    ts.it_value.tv_sec = ms / 1000;
    ts.it_value.tv_nsec = (ms % 1000) * 1000000;

    return timer_settime(*(timer_t *) timer, 0, &ts, NULL);
}

int HAL_Timer_Stop(void *timer) {
    struct itimerspec ts;

    /* check parameter */
    if (timer == NULL) {
        return -1;
    }

    /* it_interval=0: timer run only once */
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;

    /* it_value=0: stop timer */
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 0;

    return timer_settime(*(timer_t *) timer, 0, &ts, NULL);
}

int HAL_Timer_Delete(void *timer) {
    int ret = 0;

    /* check parameter */
    if (timer == NULL) {
        return -1;
    }

    ret = timer_delete(*(timer_t *) timer);

    free(timer);

    return ret;
}

int HAL_GetNetifInfo(char *nif_str) {
    memset(nif_str, 0x0, NIF_STRLEN_MAX);
#ifdef __DEMO__
    /* if the device have only WIFI, then list as follow, note that the len MUST NOT exceed NIF_STRLEN_MAX */
    const char *net_info = "WiFi|03ACDEFF0032";
    strncpy(nif_str, net_info, NIF_STRLEN_MAX);
    /* if the device have ETH, WIFI, GSM connections, then list all of them as follow, note that the len MUST NOT exceed NIF_STRLEN_MAX */
    // const char *multi_net_info = "ETH|0123456789abcde|WiFi|03ACDEFF0032|Cellular|imei_0123456789abcde|iccid_0123456789abcdef01234|imsi_0123456789abcde|msisdn_86123456789ab");
    // strncpy(nif_str, multi_net_info, strlen(multi_net_info));
#endif
    return strlen(nif_str);
}
