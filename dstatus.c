// dstatus

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define UNKNOWN_STR "n/a"

char *cpu_perc(void);
char *datetime(const char *fmt);
char *ram_used(void);
char *ret_fmt(char *fmt, ...);
void setstatus(char *str);
char *temp(const char *file);

static Display *dpy;

char *
cpu_perc(void)
{
    int perc;
    static long double ps_old[4];
    long double ps[4];
    FILE *fp;

    if (!(fp = fopen("/proc/stat", "r"))) {
        return ret_fmt(UNKNOWN_STR);
    }
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf",
            &ps[0], &ps[1], &ps[2], &ps[3]);
    fclose(fp);

    perc = 100 * ((ps_old[0] + ps_old[1] + ps_old[2]) - (ps[0] + ps[1] + ps[2])) /
                 ((ps_old[0] + ps_old[1] + ps_old[2] + ps_old[3]) - (ps[0] + ps[1] + ps[2] + ps[3]));

    ps_old[0] = ps[0];
    ps_old[1] = ps[1];
    ps_old[2] = ps[2];
    ps_old[3] = ps[3];

    return ret_fmt("%d%%", perc);
}

char *
datetime(const char *fmt)
{
    char str[20];
    time_t t;
    t = time(NULL);

    if (!strftime(str, sizeof(str), fmt, localtime(&t))) {
        return ret_fmt(UNKNOWN_STR);
    }

    return ret_fmt("%s", str);
}

char *
ram_used(void)
{
    long total, free, buffers, cached;
    FILE *fp;

    if (!(fp = fopen("/proc/meminfo", "r"))) {
        return ret_fmt(UNKNOWN_STR);
    }
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(fp, "Cached: %ld kB\n", &cached);
    fclose(fp);

    return ret_fmt("%dMiB", (int)(total - free - buffers - cached) / 1024);
}

char *
ret_fmt(char *fmt, ...)
{
    va_list ap;
    int len;
    char *ret_str;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap) + 1;
    ret_str = malloc(len);
    vsprintf(ret_str, fmt, ap);
    va_end(ap);

    return ret_str;
}

void
setstatus(char *str)
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, 0);
}

char *
temp(const char *file)
{
    int temp;
    FILE *fp;

    if (!(fp = fopen(file, "r"))) {
        return ret_fmt(UNKNOWN_STR);
    }
    fscanf(fp, "%d", &temp);
    fclose(fp);

    return ret_fmt("%d°C", temp / 1000);
}

int main(int argc, char **argv)
{
    char *cp, *t, *ru, *dt, *status;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dstatus: cannot open display.\n");
        return 1;
    }

    while (1) {
        cp = cpu_perc();
        t = temp("/sys/class/thermal/thermal_zone0/temp");
        ru = ram_used();
        dt = datetime("%Y-%m-%d %H:%M");

        status = ret_fmt("%s / %s / %s / %s", cp, t, ru, dt);
        setstatus(status);

        free(cp);
        free(t);
        free(ru);
        free(dt);
        free(status);

        sleep(2);
    }

    XCloseDisplay(dpy);
    return 0;
}
