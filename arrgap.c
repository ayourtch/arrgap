#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <signal.h>

#define BUF_SIZE 1024

void set_winsize(int slave_fd, int rows, int cols) {
    struct winsize ws;

    // Set desired rows and columns
    ws.ws_row = rows; // Number of rows
    ws.ws_col = cols; // Number of columns
    ws.ws_xpixel = 0; // Pixel width of entire terminal
    ws.ws_ypixel = 0; // Pixel height of entire terminal
    if (ioctl(slave_fd, TIOCSWINSZ, &ws) < 0) {
        perror("ioctl TIOCSWINSZ");
        return;
    }
}

int master_fd_global;

void sigwinch_handler(int signum) {
    struct winsize ws;
    char buf[128];

    // Get the current size of your terminal
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        // Set the size of the child pty
        // ioctl(master_fd_global, TIOCSWINSZ, &ws);
        snprintf(buf, sizeof(buf)-1, "\001\033]7777;r=%d,c=%d\002", ws.ws_row, ws.ws_col);
        write(master_fd_global, buf, strlen(buf));
        
    }
}


int main() {
    int master_fd, slave_fd;
    pid_t pid;
    char cmd_buffer[BUF_SIZE];
    char cmd_buffer_i = 0;
    char data_buffer[BUF_SIZE];
    char data_buffer_i = 0;
    char in_cmd_state = 0;
    char buffer[BUF_SIZE];
    char slave_name[BUF_SIZE];
    int nread;

    // Open a new pseudo-terminal
    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    master_fd_global = master_fd;


    if (master_fd < 0) {
        perror("posix_openpt");
        return 1;
    }
    if (grantpt(master_fd) < 0 || unlockpt(master_fd) < 0) {
        perror("grantpt/unlockpt");
        return 1;
    }
    printf("master fd: %d\n", master_fd);
    ptsname_r(master_fd, slave_name, sizeof(slave_name)-1);
    printf("ptsname: %s\n", slave_name);

    // Fork the process
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) { // Child Process
        setsid();
        // Open the slave side of the pty
        slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            perror("open slave pty");
            return 1;
        }

        // Redirect stdin, stdout, stderr to the pty
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);

        // Execute the shell
        execlp("/bin/sh", "/bin/sh", (char *) NULL);
        perror("execlp");
        return 1;
    } else { // Parent Process
        fd_set read_fds;
        struct termios orig_termios, new_termios;

        struct sigaction sa;
        sa.sa_handler = sigwinch_handler;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);
        if (getenv("AIRGAP_MASTER")) {
            printf("MASTER!\n");
            sigaction(SIGWINCH, &sa, NULL);
        }

        // Save original terminal attributes and set to raw mode
        tcgetattr(STDIN_FILENO, &orig_termios);
        new_termios = orig_termios;
        cfmakeraw(&new_termios);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            FD_SET(master_fd, &read_fds);

            if (select(master_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("select");
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                // Read from stdin and write to master pty
                nread = read(STDIN_FILENO, buffer, BUF_SIZE);
                if (nread > 0) {
                    int i;
                    data_buffer_i = 0;
                    for(i=0; i<nread; i++) {
                       if (in_cmd_state) {
                          if (buffer[i] == 2) {
                             int r = -1;
                             int c = -1;
                             cmd_buffer[cmd_buffer_i++] = 2;
                             cmd_buffer[cmd_buffer_i++] = 0;
                             int n = sscanf(cmd_buffer, "\001\033]7777;r=%d,c=%d\002", &r, &c);
                             // printf("DETECTED IN: %d %d!\n", r, c);
                             // printf("CMD buffer: '%s'\n", cmd_buffer);
                             if (r >= 0 && c >= 0) {
                                 set_winsize(master_fd, r, c);
                             }
                             in_cmd_state = 0;
                          } else {
                             cmd_buffer[cmd_buffer_i++] = buffer[i];
                          }
                       } else {
                         if (buffer[i] == 1) {
                             cmd_buffer[0] = buffer[i];
                             in_cmd_state = 1;
                             cmd_buffer_i = 1;
                         } else {
                             data_buffer[data_buffer_i++] = buffer[i];
                         }
                       }
                    }
                    if (data_buffer_i > 0) {
                       write(master_fd, data_buffer, data_buffer_i);
                    }
                }
            }

            if (FD_ISSET(master_fd, &read_fds)) {
                // Read from master pty and write to stdout
                nread = read(master_fd, buffer, BUF_SIZE);
                if (nread > 0) {
                    write(STDOUT_FILENO, buffer, nread);
                } else {
                    break;
                }
            }
        }

        // Restore original terminal attributes
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }

    return 0;
}

