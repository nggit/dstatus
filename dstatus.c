// dstatus

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define UNKNOWN_STR ("[n/a]")

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
    static long double a[8];
    long double b[8];
    FILE *fp;

    if (!(fp = fopen("/proc/stat", "r"))) {
        return ret_fmt(UNKNOWN_STR);
    }
    /* cpu user nice system idle iowait irq softirq steal */
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]);
    fclose(fp);

    perc = (int)(100 * ((b[0] + b[1] + b[2] + b[5] + b[6] + b[7]) - (a[0] + a[1] + a[2] + a[5] + a[6] + a[7])) /
                       ((b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + b[7]) - (a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6] + a[7])));

    memcpy(a, b, 8 * sizeof(long double));
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

    return ret_fmt("%dMiB", (total - free - buffers - cached) / 1024);
}

char *
ret_fmt(char *fmt, ...)
{
    va_list ap, ap1;
    int len;
    char *ret_str;

    va_start(ap, fmt);
    va_copy(ap1, ap);
    len = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);
    ret_str = malloc(len);
    vsprintf(ret_str, fmt, ap1);
    va_end(ap1);

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

    return ret_fmt("%d\u00B0C", temp / 1000);
}

int
main(int argc, char **argv)
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

        status = ret_fmt("%s / %s / %s / %s",
                         cp, t, ru, dt);
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

