
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    int times[3];
} TaskData;

typedef struct {
    int total;
    int longest_task;
    int maximum_time;
} TaskResult;

int write_all(int fd, const void *buffer, size_t size) {
    const char *p = buffer;

    while (size > 0) {
        ssize_t n = write(fd, p, size);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        p += n;
        size -= n;
    }

    return 0;
}

int read_all(int fd, void *buffer, size_t size) {
    char *p = buffer;

    while (size > 0) {
        ssize_t n = read(fd, p, size);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        p += n;
        size -= n;
    }

    return 0;
}

int main() {
    char studentID[50];
    TaskData data;
    TaskResult result;

    int pipe1[2], pipe2[2];

    printf("Enter Student ID: ");
    if (scanf("%49s", studentID) != 1)
        return 1;

    for (int i = 0; i < 3; i++) {
        printf("Enter Task %d Execution Time: ", i + 1);

        if (scanf("%d", &data.times[i]) != 1 ||
            data.times[i] <= 0) {
            printf("Enter positive integers only.\n");
            return 1;
        }
    }

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child Process

        close(pipe1[1]);
        close(pipe2[0]);

        TaskData received;

        if (read_all(pipe1[0], &received,
                     sizeof(received)) == -1) {
            close(pipe1[0]);
            close(pipe2[1]);
            _exit(1);
        }

        close(pipe1[0]);

        result.total = 0;
        result.maximum_time = received.times[0];
        result.longest_task = 1;

        for (int i = 0; i < 3; i++) {
            result.total += received.times[i];

            if (received.times[i] > result.maximum_time) {
                result.maximum_time = received.times[i];
                result.longest_task = i + 1;
            }
        }

        if (write_all(pipe2[1], &result,
                      sizeof(result)) == -1) {
            close(pipe2[1]);
            _exit(1);
        }

        close(pipe2[1]);
        _exit(0);
    }

    else {
        // Parent Process

        close(pipe1[0]);
        close(pipe2[1]);

        if (write_all(pipe1[1], &data,
                      sizeof(data)) == -1) {
            perror("Sending data failed");
            close(pipe1[1]);
            close(pipe2[0]);
            waitpid(pid, NULL, 0);
            return 1;
        }

        close(pipe1[1]);

        int child_status;

        if (waitpid(pid, &child_status, 0) == -1 ||
            !WIFEXITED(child_status) ||
            WEXITSTATUS(child_status) != 0) {
            printf("Child process failed.\n");
            close(pipe2[0]);
            return 1;
        }

        if (read_all(pipe2[0], &result,
                     sizeof(result)) == -1) {
            printf("Receiving results failed.\n");
            close(pipe2[0]);
            return 1;
        }

        close(pipe2[0]);

        char report[1024];

        int length = snprintf(report, sizeof(report),
            "\n--- Task Processing Report ---\n"
            "Student ID: %s\n"
            "Task 1: %d units\n"
            "Task 2: %d units\n"
            "Task 3: %d units\n"
            "Total Execution Time: %d units\n"
            "Longest Task: Task %d\n"
            "Maximum Execution Time: %d units\n",
            studentID,
            data.times[0],
            data.times[1],
            data.times[2],
            result.total,
            result.longest_task,
            result.maximum_time);

        printf("%s", report);

        char filename[100];

        snprintf(filename, sizeof(filename),
                 "%s_report.txt", studentID);

        int fd = open(filename,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0644);

        if (fd == -1) {
            perror("Report file creation failed");
            return 1;
        }

        if (write_all(fd, report, length) == -1) {
            perror("Writing report failed");
            close(fd);
            return 1;
        }
                close(fd);

        printf("Report saved to: %s\n", filename);
    }

    return 0;
}

