#ifndef __POSIX_SPAWN_H__
#define __POSIX_SPAWN_H__

typedef struct {
} posix_spawn_file_actions_t;

typedef struct {
} posix_spawnattr_t;

int posix_spawn(pid_t *restrict pid, const char *restrict path,
              const posix_spawn_file_actions_t *file_actions,
              const posix_spawnattr_t *restrict attrp,
              char *const argv[restrict], char *const envp[restrict]);

#endif /* __POSIX_SPAWN_H__ */

