
#include "global_var.h"
#include "FileModuleDef.h"
LockFreeLinkedQueue<thread_index> g_read_order_record;
std::atomic<float> finish_rate;
std::atomic_bool g_runable{ true };
std::atomic<uint64_t> g_parallel_outlen{ 0 };
uint64_t g_src_total_len;
uint64_t g_out_totallen;
std::atomic<int> g_thread_active_num{ 0 };
int concurrcy_num;
unsigned char g_M;
FileTree tree;
vector<FileNode*> FileVectorInBFSOrder;//”√”⁄±È¿˙¥Ê¥¢
queue<BFS>que;
vector<int> debug1;
vector<int>debug2;
int g_time1;
SysInfo g_system_info;
Config g_config;
int g_time2;
void ResetGlobalVar() {
    g_read_order_record.Clear();
    g_runable.store(false);
    g_parallel_outlen.store(0);
    g_thread_active_num.store(0);
    concurrcy_num = 0;
    g_src_total_len = 0;
    g_out_totallen = 0;
    g_M = 0;

    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        FileNode* File = FileVectorInBFSOrder[i];
        delete File;
    }
    FileVectorInBFSOrder.clear();
    tree.Nums = 0;
    tree.root = NULL;
    tree.RootPath = "";
    while (!que.empty()) {
        que.pop();
    }
}