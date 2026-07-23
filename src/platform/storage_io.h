#ifndef STORAGE_IO_H
#define STORAGE_IO_H

typedef enum {
    STORAGE_IO_BACKGROUND = 0,
    STORAGE_IO_FOREGROUND = 1
} storage_io_priority_t;

void storage_io_init(void);
void storage_io_lock(storage_io_priority_t priority);
void storage_io_unlock(void);

#endif
