/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <time.h>
#include "common.h"
#include "arch_wrapper.h"


uint32_t get_systick(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}


int uart_receive(uart_t *uart, uint8_t *buf, uint16_t len)
{
    // not block, so len should be 1
    int ret = read(uart->fd, buf, len);
    return ret != len;
}

int uart_transmit(uart_t *uart, const uint8_t *buf, uint16_t len)
{
    if (uart->is_stdout) {
        fwrite(buf, len, 1, stdout);
        return 0;
    }

    int ret = write(uart->fd, buf, len);
    return ret != len;
}


int uart_set_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        d_error("cduart: tcgetattr error: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        d_error("cduart: tcsetattr error: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void uart_set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        d_error("cduart: tcgetattr error: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 1;        /* x 0.1 second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        d_error("cduart: tcsetattr error: %s\n", strerror(errno));
}


void _dprintf(char* format, ...)
{
    uint32_t flags;
    list_node_t *node;

    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}

