#pragma once
#include "lock_free_queue.h"
#include "FileModuleDef.h"
#include <condition_variable>
typedef  int thread_index;
typedef struct {
    string CPU_Name;
    int Core_Num;
    int Thread_Num;
    float DRAM;
    int L2CacheSize;
    int L3CacheSize;

}SysInfo;
typedef struct {
    int32_t  use_bwt;
    int32_t block_size;
    int32_t block_dim;
    int32_t cuda_block_size;
    int32_t thread_num;
    int32_t loop_time;
    int32_t print_tree;
}Config;
extern Config g_config;
extern SysInfo g_system_info;


extern LockFreeLinkedQueue<thread_index> g_read_order_record;
extern std::atomic_bool g_runable;
extern std::atomic<uint64_t> g_parallel_outlen;
extern std::atomic<int> g_thread_active_num;
extern int concurrcy_num;;
extern std::atomic<float> finish_rate;
extern bool IsDeveloper;
extern float ElapseTime;
extern int loop_times;
extern unsigned char g_M;
extern uint64_t g_src_total_len;
extern uint64_t g_out_totallen;
extern FileTree tree;
extern int g_time1;
extern int g_time2;
extern vector<FileNode*> FileVectorInBFSOrder;//”√”⁄±È¿˙¥Ê¥¢

extern queue<BFS>que;
extern vector<int> debug1;
extern vector<int>debug2;
void ResetGlobalVar();