#ifndef COMPAT_H
#define COMPAT_H

#include <time.h>
#include <stddef.h>

// Platform-compatible monotonic clock for progress timing
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    static inline int clock_gettime_mono(struct timespec *ts) {
        static LARGE_INTEGER freq = {0};
        LARGE_INTEGER counter;
        if (freq.QuadPart == 0) {
            QueryPerformanceFrequency(&freq);
        }
        QueryPerformanceCounter(&counter);
        ts->tv_sec = (long)(counter.QuadPart / freq.QuadPart);
        ts->tv_nsec = (long)(((counter.QuadPart % freq.QuadPart) * 1000000000LL) / freq.QuadPart);
        return 0;
    }
#else
    #include <time.h>
    static inline int clock_gettime_mono(struct timespec *ts) {
        return clock_gettime(CLOCK_MONOTONIC, ts);
    }
#endif

// Platform-compatible usleep for spinner
#ifdef _WIN32
    #define cdrive_usleep(us) Sleep((us) / 1000)
#else
    #include <unistd.h>
    #define cdrive_usleep(us) usleep(us)
#endif

// Socket abstraction for local HTTP server only.
// IMPORTANT: winsock2.h must be included BEFORE windows.h.
// Since cdrive.h also includes these, we just declare the wrappers here.
#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET cdrive_socket_t;
    #define CDRIVE_INVALID_SOCKET INVALID_SOCKET
    static inline int cdrive_socket_close(cdrive_socket_t fd) { return closesocket(fd); }
    static inline int cdrive_socket_read(cdrive_socket_t fd, char *buf, int len) { return recv(fd, buf, len, 0); }
    static inline int cdrive_socket_write(cdrive_socket_t fd, const char *buf, int len) { return send(fd, buf, len, 0); }
#else
    #include <sys/socket.h>
    #include <unistd.h>
    typedef int cdrive_socket_t;
    #define CDRIVE_INVALID_SOCKET (-1)
    static inline int cdrive_socket_close(cdrive_socket_t fd) { return close(fd); }
    static inline int cdrive_socket_read(cdrive_socket_t fd, char *buf, int len) { return read(fd, buf, len); }
    static inline int cdrive_socket_write(cdrive_socket_t fd, const char *buf, int len) { return write(fd, buf, len); }
#endif

// Platform-compatible getch for interactive terminal
#ifdef _WIN32
    #include <conio.h>
    static inline int cdrive_getch(void) { return _getch(); }
#else
    #include <unistd.h>
    static inline int cdrive_getch(void) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        return c;
    }
#endif

#endif
