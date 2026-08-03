#include "buffered_open.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

buffered_file_t *buffered_open(const char *pathname, int flags, ...)
{
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }

    int real_flags = flags & ~O_PREAPPEND;

    buffered_file_t *bf = malloc(sizeof(buffered_file_t));
    if (!bf) {
        perror("malloc");
        errno = ENOMEM;
        return NULL; 
    }

    bf->fd = open(pathname, real_flags, mode);
    if (bf->fd < 0) {
        perror("open");
        free(bf);
        return NULL;
    }

    bf->flags = flags;
    bf->preappend = (flags & O_PREAPPEND) ? 1 : 0;

    bf->read_buffer = malloc(BUFFER_SIZE);
    bf->write_buffer = malloc(BUFFER_SIZE);
    if (!bf->read_buffer || !bf->write_buffer) {
        perror("malloc");
        close(bf->fd);
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        errno = ENOMEM;
        return NULL;
    }

    bf->read_buffer_size = BUFFER_SIZE;
    bf->write_buffer_size = BUFFER_SIZE;
    bf->read_buffer_pos = 0;
    bf->write_buffer_pos = 0;

    // ---------------------------------------
    // PREAPPEND: read existing file content into buffer
    // ---------------------------------------
    if (bf->preappend) {
        off_t original_pos = lseek(bf->fd, 0, SEEK_CUR);
        lseek(bf->fd, 0, SEEK_SET);

        char temp[4096];
        ssize_t r;
        while ((r = read(bf->fd, temp, sizeof(temp))) > 0) {
            // ensure buffer capacity
            while (bf->write_buffer_pos + r > bf->write_buffer_size) {
                bf->write_buffer_size *= 2;
                char *new_buf = realloc(bf->write_buffer, bf->write_buffer_size);
                if (!new_buf) {
                    perror("realloc");
                    close(bf->fd);
                    free(bf->read_buffer);
                    free(bf->write_buffer);
                    free(bf);
                    return NULL;
                }
                bf->write_buffer = new_buf;
            }
            for (ssize_t i = 0; i < r; i++) {
                bf->write_buffer[bf->write_buffer_pos + i] = temp[i];
            }
            bf->write_buffer_pos += r;
        }
        lseek(bf->fd, original_pos, SEEK_SET);
    }

    return bf;
}

// -------------------------------------------------------------
// Buffered WRITE
// -------------------------------------------------------------
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count)
{
    if (!bf) {
        errno = ENOENT;
        perror("buffered_write");
        return -1;
    }

    const char *src = (const char *)buf;

    // -------------------------------
    // PREAPPEND
    // -------------------------------
    if (bf->preappend) {
        if (buffered_flush(bf) < 0) return -1;

        // Seek to beginning of file
        off_t original_pos = lseek(bf->fd, 0, SEEK_SET);
        if (original_pos == (off_t)-1) {
            perror("lseek");
            return -1;
        }

        // Get current file size
        off_t file_size = lseek(bf->fd, 0, SEEK_END);
        if (file_size == (off_t)-1) {
            perror("lseek");
            return -1;
        }

        // Allocate temporary buffer for old content
        char *temp = malloc(file_size);
        if (!temp) {
            perror("malloc");
            return -1;
        }

        // Read existing file content into temp
        if (file_size > 0) {
            lseek(bf->fd, 0, SEEK_SET);
            ssize_t r = read(bf->fd, temp, file_size);
            if (r < 0) {
                perror("read");
                free(temp);
                return -1;
            }
        }

        // Seek back to beginning to write new data
        lseek(bf->fd, 0, SEEK_SET);

        // Write new data first
        ssize_t w = write(bf->fd, src, count);
        if (w < 0) {
            perror("write");
            free(temp);
            return -1;
        }

        // Append old content from temporary buffer
        if (file_size > 0) {
            w = write(bf->fd, temp, file_size);
            if (w < 0) {
                perror("write");
                free(temp);
                return -1;
            }
        }

        free(temp);

        // Clear write buffer position since everything is directly written
        bf->write_buffer_pos = 0;
        return count;
    }

    // -------------------------------
    // Normal buffered write 
    // -------------------------------
    while (bf->write_buffer_pos + count > bf->write_buffer_size) {
        bf->write_buffer_size *= 2;
        char *new_buf = realloc(bf->write_buffer, bf->write_buffer_size);
        if (!new_buf) {
            perror("realloc");
            return -1;
        }
        bf->write_buffer = new_buf;
    }

    for (size_t i = 0; i < count; i++) {
        bf->write_buffer[bf->write_buffer_pos + i] = src[i];
    }

    bf->write_buffer_pos += count;
    return count;
}


// -------------------------------------------------------------
// Buffered READ
// -------------------------------------------------------------
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count)
{
    if (!bf) {
        errno = ENOENT;                  // Set error code: "No such file or directory"
        perror("buffered_read");
        return -1;
    }

    if (bf->write_buffer_pos > 0) {
        if (buffered_flush(bf) < 0) return -1;
        bf->read_buffer_pos = 0;
        bf->read_buffer_size = 0;
    }

    if (bf->read_buffer_pos == 0) {
        ssize_t r = read(bf->fd, bf->read_buffer, BUFFER_SIZE);
        if (r <= 0) return r;
        bf->read_buffer_size = r;
    }

    // How many bytes we can actually read from the buffer
    size_t available = bf->read_buffer_size - bf->read_buffer_pos;
    size_t to_read;
    if (available < count) {
        to_read = available;
    } else {
        to_read = count;
    }

    char *dst = (char *)buf;
    char *src = bf->read_buffer + bf->read_buffer_pos;

    // Copy the bytes from internal buffer to user's buffer
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = src[i];
    }

    bf->read_buffer_pos += to_read;

    return to_read;
}

// -------------------------------------------------------------
// Flush write buffer to file
// -------------------------------------------------------------
int buffered_flush(buffered_file_t *bf)
{
    if (!bf) {
        errno = ENOENT;
        perror("buffered_flush");
        return -1;
    }

    // Only flush if there is data in the write buffer
    if (bf->write_buffer_pos > 0) {

        if (bf->preappend) {
            if (lseek(bf->fd, 0, SEEK_SET) == (off_t)-1) {
                perror("lseek");
                return -1;
            }
        } else if (bf->flags & O_APPEND) {
            if (lseek(bf->fd, 0, SEEK_END) == (off_t)-1) {
                perror("lseek");
                return -1;
            }
        }

        ssize_t total_written = 0;
        while ((size_t)total_written < bf->write_buffer_pos) {
            ssize_t w = write(bf->fd, bf->write_buffer + total_written,bf->write_buffer_pos - total_written);
            if (w < 0) {
                perror("write");
                return -1;
            }
            total_written += w;
        }

        bf->write_buffer_pos = 0;
    }

    return 0;
}

// -------------------------------------------------------------
// Close
// -------------------------------------------------------------
int buffered_close(buffered_file_t *bf)
{
    if (!bf) {
        errno = ENOENT;
        perror("buffered_close");
        return -1;
    }

    buffered_flush(bf);
    close(bf->fd);
    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);
    return 0;
}
