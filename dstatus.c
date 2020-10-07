// dstatus

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define INTERVAL    2
#define UNKNOWN_STR ("[n/a]")

char *cpu_perc(void);
char *datetime(const char *fmt);
char *net_speed(void);
int pretty_bytes(char *str, double bytes, unsigned n);
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
net_speed(void)
{
    char line[256], ib[8], rx[8], tx[8];
    char *ls;
    static char la[100][32];
    static char iface[8];
    static long double ra, ta, rc, tc;
    long double rb, tb;
    int i = 0;
    FILE *fp;

    if (!(fp = fopen("/proc/net/dev", "r"))) {
        return ret_fmt(UNKNOWN_STR);
    }
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    while (i < sizeof(la) / sizeof(la[0]) && fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%s %Lf %*f %*f %*f %*f %*f %*f %*f %Lf", ib, &rb, &tb);
        ls = strstr(line, ":");
        if (strncmp(la[i], ls, sizeof(la[0])) != 0) {
            rc = rb;
            tc = tb;
            memcpy(iface, ib, sizeof(iface) - 1);
            iface[sizeof(iface) - 1] = '\0';
        }
        memcpy(la[i], ls, sizeof(la[0]));
        ++i;
    }
    fclose(fp);

    pretty_bytes(rx, (rc - ra) / INTERVAL, sizeof(rx));
    pretty_bytes(tx, (tc - ta) / INTERVAL, sizeof(tx));
    ra = rc;
    ta = tc;

    return ret_fmt("%s %-4s^%-4s", iface, rx, tx);
}

int
pretty_bytes(char *str, double bytes, unsigned n)
{
    const char *s[] = { "B", "K", "M", "G" };
    int i = 0;

    if (bytes < 0)
        bytes = 0;
    while (bytes >= 1024 && i < 4) {
        ++i;
        bytes /= 1024;
    }

    return snprintf(str, n, "%d%s", (int)bytes, s[i]);
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

    return ret_fmt("%dM", (total - free - buffers - cached) / 1024);
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
    char *cp, *t, *ru, *ns, *dt, *status;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dstatus: cannot open display.\n");
        return 1;
    }

    for (;;) {
        cp = cpu_perc();
        t  = temp("/sys/class/thermal/thermal_zone0/temp");
        ru = ram_used();
        ns = net_speed();
        dt = datetime("%b %d %H:%M");

        status = ret_fmt("%s / %s / %s / %s / %s",
                         cp, t, ru, ns, dt);
        setstatus(status);

        free(cp);
        free(t);
        free(ru);
        free(ns);
        free(dt);
        free(status);

        sleep(INTERVAL);
    }

    XCloseDisplay(dpy);
    return 0;
}
