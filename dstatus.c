/* dstatus */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define INTERVAL    1
#define UNKNOWN_STR ("[n/a]")

char *cpu_perc(void);
char *datetime(const char *fmt);
char *net_addr(void);
char *net_gateway(void);
char *net_speed(void);
int pretty_bytes(char *str, double bytes, unsigned int n);
char *ram_used(void);
char *ret_fmt(char *fmt, ...);
void setstatus(char *str);
char *temp(const char *file);
char *uptime(void);

static Display *dpy;

char *
cpu_perc(void)
{
    int perc;
    static long double a[8];
    long double b[8];
    FILE *fp;

    if (!(fp = fopen("/proc/stat", "r")))
        return ret_fmt(UNKNOWN_STR);

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

    if (!strftime(str, sizeof(str), fmt, localtime(&t)))
        return ret_fmt(UNKNOWN_STR);

    return ret_fmt("%s", str);
}

char *
net_addr(void)
{
    char line[256], path[20] = "/proc/net/";
    char p[][10] = { "tcp", "udp" };
    unsigned int a, b, c, d, st, path_len = strlen(path);
    FILE *fp;

    for (unsigned int i = 0; i < sizeof(p) / sizeof(p[0]); ++i) {
        strcpy(path + path_len, p[i]);
        if (!(fp = fopen(path, "r")))
            return ret_fmt(UNKNOWN_STR);

        fgets(line, sizeof(line), fp);
        while (fgets(line, sizeof(line), fp) != NULL) {
            sscanf(line, "%*[^:]: %2x%2x%2x%2x:%*[^:]:%*x %x", &a, &b, &c, &d, &st);
            if (st < 7) {
                fclose(fp);
                return ret_fmt("%u.%u.%u.%u (%s)", d, c, b, a, p[i]);
            }
        }
        fclose(fp);
    }

    return ret_fmt("0.0.0.0");
}

char *
net_gateway(void)
{
    char line[256], iface[8];
    unsigned int a, b, c, d;
    FILE *fp;

    if (!(fp = fopen("/proc/net/route", "r")))
        return ret_fmt(UNKNOWN_STR);

    fgets(line, sizeof(line), fp);
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return ret_fmt("0.0.0.0");
    } else {
        sscanf(line, "%s %*x %2x%2x%2x%2x", iface, &a, &b, &c, &d);
        fclose(fp);
        return ret_fmt("%u.%u.%u.%u (%s)", d, c, b, a, iface);
    }
}

char *
net_speed(void)
{
    char line[256], ib[8], rx[8], tx[8];
    char *lb;
    static char la[100][32];
    static char iface[8];
    static long double ra, ta, rc, tc;
    long double rb, tb;
    unsigned int i = 0;
    FILE *fp;

    if (!(fp = fopen("/proc/net/dev", "r")))
        return ret_fmt(UNKNOWN_STR);

    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    while (i < sizeof(la) / sizeof(la[0]) && fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%s %Lf %*f %*f %*f %*f %*f %*f %*f %Lf", ib, &rb, &tb);
        lb = strstr(line, ":");
        if (strncmp(la[i], lb, sizeof(la[0])) != 0) {
            rc = rb;
            tc = tb;
            memcpy(iface, ib, strlen(ib) - 1);
            iface[strlen(ib) - 1] = '\0';
        }
        memcpy(la[i], lb, sizeof(la[0]));
        ++i;
    }
    fclose(fp);

    pretty_bytes(rx, (rc - ra) / INTERVAL, sizeof(rx));
    pretty_bytes(tx, (tc - ta) / INTERVAL, sizeof(tx));
    ra = rc;
    ta = tc;

    return ret_fmt("down: %-4s up: %-4s (%s)", rx, tx, iface);
}

int
pretty_bytes(char *str, double bytes, unsigned int n)
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
    long int total, free, buffers, cached;
    FILE *fp;

    if (!(fp = fopen("/proc/meminfo", "r")))
        return ret_fmt(UNKNOWN_STR);

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

    if (!(fp = fopen(file, "r")))
        return ret_fmt(UNKNOWN_STR);

    fscanf(fp, "%d", &temp);
    fclose(fp);

    return ret_fmt("%d\u00B0C", temp / 1000);
}

char *
uptime(void) {
    unsigned int d, h, m, s;
    FILE *fp;

    if (!(fp = fopen("/proc/uptime", "r")))
        return ret_fmt(UNKNOWN_STR);

    fscanf(fp, "%u", &s);
    fclose(fp);

    d = s / 86400;
    s %= 86400;
    h = s / 3600;
    s %= 3600;
    m = s / 60;
    s %= 60;

    return ret_fmt("%ud %02u:%02u:%02u", d, h, m, s);
}

int
main(void)
{
    char *cp, *t, *ru, *u, *dt, *ns, *na, *ng, *status;

    if (!(dpy = XOpenDisplay(NULL)))
        return fprintf(stderr, "dstatus: cannot open display.\n"), 1;

    for (;;) {
        cp     = cpu_perc();
        t      = temp("/sys/class/thermal/thermal_zone0/temp");
        ru     = ram_used();
        u      = uptime();
        dt     = datetime("%b %d %H:%M");
        ns     = net_speed();
        na     = net_addr();
        ng     = net_gateway();
        status = ret_fmt("%s / %s / %s / %s / %s;%s / addr: %s / gateway: %s",
                         cp, t, ru, u, dt, ns, na, ng);
        setstatus(status);

        free(cp);
        free(t);
        free(ru);
        free(u);
        free(dt);
        free(ns);
        free(na);
        free(ng);
        free(status);

        sleep(INTERVAL);
    }

    XCloseDisplay(dpy);
    return 0;
}
