#ifndef __BOOT_CONF_H__
#define __BOOT_CONF_H__

enum {
    SFX_MOVE,
    SFX_SCROLL,
    SFX_WARNING,
    SFX_CONFIRM,
    SFX_START_APP,
    SFX_NOWAY,
    SFX_MAX,
};

#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SFX_DIR_PATH (BOOT_SYS_DIR_PATH"/sfx")
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_BIN_DIR_NAME "BIN"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME
#define BOOT_STARTUP_MUSIC_PATH (BOOT_SYS_DIR_PATH"/mus/title.wav")

#endif /*__BOOT_CONF_H__*/
