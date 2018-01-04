#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <regex.h>
#include <dirent.h>

#define die(str, args...) do { \
        perror(str); \
        exit(EXIT_FAILURE); \
    } while(0)

int
main(int argc, char *argv[]) {
    int fdo, fdi;
    struct uinput_user_dev uidev;
    struct input_event ev;

    if (argc != 2) die("error: specify input device");

    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fdo < 0) die("error: open");


    DIR *dirp;
    struct dirent *dp;
    regex_t kbd;
    char fullPath[1024];
    static char *dirName = "/dev/input/by-id";

    if (regcomp(&kbd, "event-kbd", 0) != 0) {
        die("regcomp for kbd failed");
    }

    if ((dirp = opendir(dirName)) == NULL) {
        die("couldn't open '/dev/input/by-id'");
    }

    do {
        if ((dp = readdir(dirp)) != NULL) {
            if (regexec(&kbd, dp->d_name, 0, NULL, 0) == 0) {
                sprintf(fullPath, "%s/%s", dirName, dp->d_name);

                fdi = open(fullPath, O_RDONLY | O_NONBLOCK);
                if (fdi < 0) die("error: open");

                if (ioctl(fdi, EVIOCGRAB, 1) < 0) die("error: ioctl");

                if (ioctl(fdo, UI_SET_EVBIT, EV_SYN) < 0) die("error: ioctl");
                if (ioctl(fdo, UI_SET_EVBIT, EV_KEY) < 0) die("error: ioctl");
                if (ioctl(fdo, UI_SET_EVBIT, EV_MSC) < 0) die("error: ioctl");

                for (int i = 0; i < KEY_MAX; ++i)
                    if (ioctl(fdo, UI_SET_KEYBIT, i) < 0) die("error: ioctl");

                memset(&uidev, 0, sizeof(uidev));
                snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "tahometer-keyhandler");
                uidev.id.bustype = BUS_USB;
                uidev.id.vendor = 0x1;
                uidev.id.product = 0x1;
                uidev.id.version = 1;

                if (write(fdo, &uidev, sizeof(uidev)) < 0) die("error: write");
                if (ioctl(fdo, UI_DEV_CREATE) < 0) die("error: ioctl");

                while (1) {
                    if (read(fdi, &ev, sizeof(struct input_event)) < 0)
                        die("error: read");

                    ev.time.tv_sec = 0;
                    ev.time.tv_usec = 0;

                    if (write(fdo, &ev, sizeof(struct input_event)) < 0)
                        die("error: write");
                }

                if (ioctl(fdo, UI_DEV_DESTROY) < 0) die("error: ioctl");

                close(fdi);
                close(fdo);
            }
        }
    } while (dp != NULL);

    return 0;
}
