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
            int cpu_id = 0;

            printf("%ld", (unsigned long) time(NULL));
            while(fgets(buf, sizeof(buf), f) != NULL) {
                if (strncmp(buf, "cpu", 3) == 0) {
                        FILE *f2;
                        unsigned long long user, nice, system, idle, iowait, irq, softirq;
                        unsigned long long active;
                        int cpu_nr = cpu_id - 1;

                        sscanf(buf, "%*s %lld %lld %lld %lld %lld %lld %lld", &user, &nice, &system, &idle, &iowait, &irq, &softirq);
                        active = user + nice + system;
                        irq += softirq;
                        if (cpu_id == 0) {
                                unsigned long long delta_active = (active + iowait + irq) - (cpu_stats[cpu_id].active + cpu_stats[cpu_id].iowait + cpu_stats[cpu_id].irq);
                                unsigned long long delta_idle = idle - cpu_stats[cpu_id].idle;
                                printf(" %5.0f%%", ((double) delta_active) / (delta_active + delta_idle) * 100);
                        } else {
                            sprintf(buf, "/sys/devices/system/cpu/cpu%d/online", cpu_nr);
                            if ((f2 = fopen(buf, "r")) != NULL) {
                                    int online = 1;
                                    fscanf(f2, "%d", &online);
                                    fclose(f2);
                                    if (! online) {
                                        cpu_id++;
                                        continue;
                                    }
                            }
                        }
                        printf(" %6lld/%-6lld", active-cpu_stats[cpu_id].active, idle-cpu_stats[cpu_id].idle);
                        cpu_stats[cpu_id].active = active;
                        cpu_stats[cpu_id].idle = idle;
                        cpu_stats[cpu_id].iowait = iowait;
                        cpu_stats[cpu_id].irq = irq;
                        sprintf(buf, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_nr);
                        if ((f2 = fopen(buf, "r")) != NULL) {
                            unsigned freq;
                            fscanf(f2, "%u", &freq);
                            printf(" %7u", freq);
                            fclose(f2);
                        }
                        cpu_id++;
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
