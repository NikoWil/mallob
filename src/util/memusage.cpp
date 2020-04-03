
#include <sched.h>

#include "memusage.h"
#include "util/timer.h"

// https://stackoverflow.com/a/671389
void process_mem_usage(int& cpu, double& vm_usage, double& resident_set)
{
   using std::ios_base;
   using std::ifstream;
   using std::string;

   vm_usage     = 0.0;
   resident_set = 0.0;

   // 'file' stat seems to give the most reliable results
   //
   ifstream stat_stream("/proc/self/stat", ios_base::in);

   // dummy vars for leading entries in stat that we don't care about
   //
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string utime, stime, cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;

   // the two fields we want
   //
   unsigned long vsize;
   long rss;

   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / 1024.0;
   resident_set = rss * page_size_kb;

   cpu = sched_getcpu();
}

bool thread_rusage(double& cpuTimeMicros, long& voluntaryCtxSwitches, long& involuntaryCtxSwitches) {
   rusage usage;
   int result = getrusage(RUSAGE_THREAD, &usage);
   if (result < 0) return false;
   cpuTimeMicros = 0.001 * 0.001 * (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec);
   cpuTimeMicros += usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
   voluntaryCtxSwitches = usage.ru_nvcsw;
   involuntaryCtxSwitches = usage.ru_nivcsw;
   return true;
}

bool thread_cpuratio(int tid, float age, double& cpuRatio) {
   
   using std::ios_base;
   using std::ifstream;
   using std::string;

   // Get uptime in seconds
   ifstream uptime_stream("/proc/uptime", ios_base::in);
   unsigned long uptime;
   uptime_stream >> uptime;
   uptime_stream.close();

   // Get hertz
   unsigned long hertz = sysconf(_SC_CLK_TCK);

   // Get actual stats of interest
   std::string filepath = "/proc/" + std::to_string(getpid()) + "/task/" + std::to_string(tid) + "/stat";
   ifstream stat_stream(filepath, ios_base::in);
   // dummy vars for leading entries in stat that we don't care about
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;
   // the two fields we want
   unsigned long utime;
   unsigned long stime;
   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime;
   stat_stream.close();

   // Compute result
   cpuRatio = 100 * ((utime + stime) / hertz) / age;

   return true;
}