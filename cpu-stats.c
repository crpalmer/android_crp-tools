#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

struct {
    unsigned long long active;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
} cpu_stats[1000];

char buf[100*1024];

int main(int argc, char **argv)
{
    while(1) {
        FILE *f;

        if ((f = fopen("/proc/stat", "r")) != NULL) {
            int cpu = 0;

            printf("%ld", (unsigned long) time(NULL));
            while(fgets(buf, sizeof(buf), f) != NULL) {
                if (strncmp(buf, "cpu", 3) == 0) {
                        FILE *f2;
                        unsigned long long user, nice, system, idle, iowait, irq, softirq;
                        unsigned long long active;

                        sscanf(buf, "%*s %lld %lld %lld %lld %lld %lld %lld", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
                        active = user + nice + system;
                        irq += softirq;
                        if (cpu == 0) {
                                unsigned long long delta_active = (active + iowait + irq) - (cpu_stats[cpu].active + cpu_stats[cpu].iowait + cpu_stats[cpu].irq);
                                unsigned long long delta_idle = idle - cpu_stats[cpu].idle;
                                printf(" %5.0f%%", ((double) delta_active) / (delta_active + delta_idle) * 100);
                        }
                        printf(" %6lld/%-6lld", active-cpu_stats[cpu].active, idle-cpu_stats[cpu].idle);
                        cpu_stats[cpu].active = active;
                        cpu_stats[cpu].idle = idle;
                        cpu_stats[cpu].iowait = iowait;
                        cpu_stats[cpu].irq = irq;
                        sprintf(buf, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
                        if ((f2 = fopen(buf, "r")) != NULL) {
                            unsigned freq;
                            fscanf(f2, "%u", &freq);
                            printf(" %7u", freq);
                            fclose(f2);
                        }
                        cpu++;
                } else if (strncmp(buf, "procs_running", 13) == 0) {
                        unsigned running = atoi(&buf[13]);
                        printf(" %d", running);
                }
            }
            fclose(f);
            printf("\n");
        }
        sleep(1);
    }
}
