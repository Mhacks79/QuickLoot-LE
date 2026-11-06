#pragma once

#include <cstdint>

#define KHAOS_API_VERSION 2u
#define KHAOS_API_VERSION_MIN 1u

#ifndef KHAOS_ABI
#define KHAOS_ABI extern "C" __declspec(dllexport)
#endif

struct KHAOS_PluginInfo
{
    unsigned int apiVersion;
    unsigned int structSize;
    unsigned int flags;
    const char* pluginId;
    const char* displayName;
    const char* category;
    const char* dllName;
    const char* originalHotkey;
    const char* description;
};

typedef bool (*KHAOS_QueryFn)(KHAOS_PluginInfo*);
typedef void (*KHAOS_SetHostedFn)(bool);
typedef void (*KHAOS_ToggleMenuFn)();
typedef bool (*KHAOS_IsMenuOpenFn)();
typedef void (*KHAOS_RenderFrameFn)(void*, void*, int, int);
typedef bool (*KHAOS_ProcessWndProcFn)(
    void*, unsigned int, std::uintptr_t, std::intptr_t);

enum KHAOS_WorkloadDomain : unsigned int
{
    KHAOS_DOMAIN_UNKNOWN = 0,
    KHAOS_DOMAIN_OAR = 1,
    KHAOS_DOMAIN_PIE = 2,
    KHAOS_DOMAIN_AMR = 3
};

enum KHAOS_WorkloadActivity : unsigned int
{
    KHAOS_ACTIVITY_UNKNOWN = 0,
    KHAOS_ACTIVITY_DIRECTORY_CACHE = 1,
    KHAOS_ACTIVITY_MOD_BUILD = 2,
    KHAOS_ACTIVITY_DEFAULT_PRELOAD = 3,
    KHAOS_ACTIVITY_CONFIG_RELOAD = 4,
    KHAOS_ACTIVITY_PAYLOAD_INIT = 5,
    KHAOS_ACTIVITY_MOTION_INIT = 6
};

enum KHAOS_WorkloadState : unsigned int
{
    KHAOS_WORKLOAD_BEGIN = 1,
    KHAOS_WORKLOAD_END = 2,
    KHAOS_WORKLOAD_FAIL = 3
};

#pragma pack(push, 8)
struct KHAOS_WorkloadEvent
{
    unsigned int apiVersion;
    unsigned int structSize;
    unsigned int domain;
    unsigned int activity;
    unsigned int state;
    unsigned int flags;
    std::uint64_t correlationId;
    std::uint64_t timestampQpc;
    std::uint64_t estimatedBytes;
    const char* sourceId;
    const char* label;
};

struct KHAOS_HostServices
{
    unsigned int apiVersion;
    unsigned int structSize;
    void* (*allocate)(unsigned int, unsigned int);
    void* (*reallocate)(void*, unsigned int, unsigned int);
    void (*deallocate)(void*);
    bool (*reportWorkload)(const KHAOS_WorkloadEvent*);
};
#pragma pack(pop)

#if defined(_WIN32) && defined(_M_IX86)
static_assert(sizeof(KHAOS_WorkloadEvent) == 56,
    "KHA-OS workload ABI packing mismatch");
static_assert(sizeof(KHAOS_HostServices) == 24,
    "KHA-OS host-services ABI packing mismatch");
#endif

typedef bool (*KHAOS_GetHostServicesFn)(KHAOS_HostServices*);

KHAOS_ABI bool KHAOS_GetHostServices(KHAOS_HostServices* outServices);
