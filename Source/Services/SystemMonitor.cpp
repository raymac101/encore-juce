/*
  ==============================================================================

    SystemMonitor.cpp

    See SystemMonitor.h. Platform-specific process CPU-time and resident-
    memory queries, kept in plain C/C++ (no Objective-C++ needed on macOS --
    mach/libproc are plain C APIs, unlike e.g. AVFoundation elsewhere in this
    codebase) so this stays one file on both platforms.

  ==============================================================================
*/

#include "SystemMonitor.h"

#if JUCE_MAC
 #include <mach/mach.h>
 #include <libproc.h>
 #include <unistd.h>
#elif JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#endif

namespace
{
    /** Total CPU time (user + system) this process has consumed since it
        started, in seconds. Returns -1 if the platform query failed. */
    double getProcessCpuTimeSeconds()
    {
       #if JUCE_MAC
        // proc_pid_rusage/RUSAGE_INFO_V2 gives user+system time directly in
        // nanoseconds and needs no extra entitlement -- task_info's
        // TASK_THREAD_TIMES_INFO only covers *live* threads, undercounting
        // anything that already exited (e.g. the short-lived
        // juce::Thread::launch() workers FirestoreClient spins up for every
        // network call), which would make CPU% read artificially low on
        // exactly the kind of network-heavy workload this exists to help
        // diagnose.
        struct rusage_info_v2 info;
        if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, (rusage_info_t*) &info) == 0)
            return (double) (info.ri_user_time + info.ri_system_time) / 1.0e9;
        return -1.0;
       #elif JUCE_WINDOWS
        FILETIME creation, exit, kernel, user;
        if (GetProcessTimes (GetCurrentProcess(), &creation, &exit, &kernel, &user))
        {
            ULARGE_INTEGER k, u;
            k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user.dwLowDateTime;   u.HighPart = user.dwHighDateTime;
            // FILETIME units are 100ns.
            return (double) (k.QuadPart + u.QuadPart) / 1.0e7;
        }
        return -1.0;
       #else
        return -1.0;
       #endif
    }

    /** Current resident memory footprint in bytes, or -1 if unavailable. */
    double getProcessResidentBytes()
    {
       #if JUCE_MAC
        mach_task_basic_info_data_t info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
            return (double) info.resident_size;
        return -1.0;
       #elif JUCE_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)))
            return (double) pmc.WorkingSetSize;
        return -1.0;
       #else
        return -1.0;
       #endif
    }
}

SystemMonitor& SystemMonitor::getInstance()
{
    static SystemMonitor instance;
    return instance;
}

SystemMonitor::Sample SystemMonitor::sample()
{
    Sample result;

    const double residentBytes = getProcessResidentBytes();
    if (residentBytes >= 0.0)
        result.memoryMB = residentBytes / (1024.0 * 1024.0);

    const double cpuSeconds = getProcessCpuTimeSeconds();
    const juce::int64 nowMs = juce::Time::getMillisecondCounter();

    if (cpuSeconds >= 0.0)
    {
        if (haveLastSample_)
        {
            const double deltaCpuSeconds = cpuSeconds - lastProcessCpuSeconds_;
            const double deltaWallSeconds = (double) (nowMs - lastWallTimeMs_) / 1000.0;

            if (deltaWallSeconds > 0.01) // guard against a near-zero divisor on back-to-back calls
            {
                const int numCores = juce::jmax (1, juce::SystemStats::getNumCpus());
                const double percent = (deltaCpuSeconds / deltaWallSeconds) / (double) numCores * 100.0;
                result.cpuPercent = (float) juce::jlimit (0.0, 100.0, percent);
            }
        }

        lastProcessCpuSeconds_ = cpuSeconds;
        lastWallTimeMs_ = nowMs;
        haveLastSample_ = true;
    }

    return result;
}
