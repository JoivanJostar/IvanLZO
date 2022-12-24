#include<iostream>
#include <string>
#include <vector>
#include <fstream>
#include <queue>
#include <cstdio>
#include <assert.h>
#include "global_var.h"
#include <string.h>
#include "Encoder.h"
#include "Decoder.h"
#include <Windows.h>
#include <atlstr.h> 
#include "FileManage.h"
#include <Shlobj.h>
#include <thread>
#include "Console.h"
#include "cuda_lzo.h"
using namespace std;
enum Fuction
{
    ENCODE=1,DECODE
};
enum Moden {
    SINGLE = 1,
    MULTI= 2,
    CUDA = 3
};
//需要封装到类里面




bool IsDeveloper;
int loop_times;
int total_time;


void LoadConfig() {
    FILE* fp;
    fp=freopen("config.ini", "r", stdin);
    if (fp == NULL) {
        cout << "找不到配置文件 config.ini\n";
        exit(0);
    }
  /*  if (scanf("[block_size]=%d;\n", &block_size)) {
        if (block_size <= 0 || block_size>1024) {
            cout << "配置文件损坏\n";
            exit(0);
        }
        g_config.block_size = block_size;
    }
    else {
        cout << "配置文件损坏\n";
        exit(0);
    }
    if (scanf("[BWT]=%d;\n", &use_bwt)) {
        if (use_bwt!=0&&use_bwt!=1) {
            cout << "配置文件损坏\n";
            exit(0);
        }
        g_config.use_bwt = use_bwt;
    }
    else {
        cout << "配置文件损坏\n";
        exit(0);
    }
    if (scanf("[block_dim]=%d;\n", &g_config.block_dim)) {
        if (g_config.block_dim <= 0 || g_config.block_dim > 1024) {
            cout << "配置文件损坏\n";
            exit(0);
        }
    }
    else {
        cout << "配置文件损坏\n";
        exit(0);
    }
    if (scanf("[cuda_block_size]=%d;\n", &g_config.cuda_block_size)) {
        if (g_config.cuda_block_size <= 0 || g_config.cuda_block_size > 1024) {
            cout << "配置文件损坏\n";
            exit(0);
        }
    }
    else {
        cout << "配置文件损坏\n";
        exit(0);
    }*/
    if (scanf("[block_size]=%d;\n[BWT]=%d;\n[block_dim]=%d;\n[cuda_block_size]=%d;\n[threads]=%d;\n[loops]=%d\n[tree]=%d\n", 
        &g_config.block_size,
        &g_config.use_bwt,
        &g_config.block_dim,
        &g_config.cuda_block_size,
        &g_config.thread_num,
        &g_config.loop_time,
        &g_config.print_tree)) {

    }
    else {
        cout << "配置文件损坏\n";
        exit(0);
    }
    fclose(fp);
    
    freopen("CON", "r", stdin);
}
void LoadSysInfo() {
    FILE* fp;
    fp = freopen("sysinfo.xd", "r", stdin);
    if (fp == NULL) {
        cout << "找不到系统文件 sysinfo.xd\n";
        exit(0);
    }
    char line[100] = { 0 };
    gets_s(line);
    int ret=scanf("\n");
    char * pos=strchr(line, '=');
    if (pos == NULL) {
        cout << "系统文件信息异常\n";
        exit(0);
    }
    string strname(pos+1);
    g_system_info.CPU_Name = strname;
    if(!(scanf("[CoreNums]=%d\n", &g_system_info.Core_Num) > 0) ){
        cout << "系统文件信息异常\n";
        exit(0);
    }
    if (!(scanf("[ThreadNums]=%d\n", &g_system_info.Thread_Num) > 0)) {
        cout << "系统文件信息异常\n";
        exit(0);
    }
    if (!(scanf("[DRAM]=%f\n", &g_system_info.DRAM) > 0)) {
        cout << "系统文件信息异常\n";
        exit(0);
    }
    gets_s(line);
    if (!(scanf("%d", &g_system_info.L2CacheSize) > 0)) {
        cout << "获取L2Cache信息失败\n";
        g_system_info.L2CacheSize = 0; 

    }
    gets_s(line);
    gets_s(line);
    if (!(scanf("%d", &g_system_info.L3CacheSize) > 0)) {
        cout << "获取L3Cache信息失败\n";
        g_system_info.L3CacheSize = 0;
    }
    fclose(fp);

    freopen("CON", "r", stdin);
}
int main(int argc,char ** argv) {
   // Test_CUDA_NOBWT();
    //return 0;
    ifstream infile;
    try_get_cpu_info:
    infile.open(".\\sysinfo.xd");
    if (!infile) {
        system("start .\\CPUINFO.exe");
        cout << "正在导入系统信息.........\n";
        Sleep(1000);
        goto try_get_cpu_info;

    }
    infile.close();

    IsDeveloper = true;
    FileManage Mannger;
    ResetGlobalVar();
    loop_times = IsDeveloper ? 10 : 1;
    total_time = 0;

    LoadConfig();
    loop_times = g_config.loop_time;
    LoadSysInfo();
select_En_De:
    cout << "选择功能\n\n【1】压缩\n\n【2】解压\n\n\n";
    string input;
    cin >> input;
    int En_De;
    try {
        En_De = stoul(input);
        if (En_De != 1 && En_De != 2)
            goto select_En_De;
    }
    catch (...) {
        goto select_En_De;
    }
    if (En_De == ENCODE) {
        Console::ClearScreen();
        int input_num;
        vector<string> files;
        string folder;
        cout << "===================================== 压  缩 =======================================" << endl << endl;
    select_2:
        cout << "【1】压缩文件\n\n【2】压缩文件夹\n\n";
        cin >> input;
        try {
            input_num = stoul(input);
            if (input_num != 1 && input_num != 2)
                goto select_2;
        }
        catch (...) {
            goto select_2;
        }
        if (input_num == 1) {
            files = Mannger.SelectFiles();
            if (files.empty())
                goto select_2;
        }
        else {
            folder = Mannger.SelectFolder();
            if (folder == "")
                goto select_2;
        }
        select_6:
        cout << "请选择保存路径\n";
        Sleep(500);
        string SavePath = Mannger.SaveAs();
        if (SavePath == "") {
            cout << "尚未选择目录  可按任意键重新选择输出\n\n";
            system("pause");
            Console::ClearScreen();
            goto select_6;
        }
        auto it1 = SavePath.end() - 1;
        while (*it1 != '\\')
            it1--;
        auto it2 = SavePath.end() - 1;
        while (*it2 != '.')
            it2--;
        string PackageName(it1 + 1, it2);
        string PackageRootPath(SavePath.begin(), it1);

        cout << "\n包名称:" << PackageName << endl;
        if (input_num == 1) {
            ConstructSimpleFileTree(PackageName, files);
        }
        else {
            ConstructFileTree(folder);
        }
        //cout<<"是否要查看文件树的结"
        if (g_config.print_tree) {
            cout << "\n待压缩文件树结构:\n";
            PrintfFileTree(tree);
            cout << "\n\n请确认该文件夹的结构\n\n";
            system("pause");
        }

        Console::ClearScreen();
        //system("cls");
    select_3:
        cout << "请选择压缩模式\n";
        cout << "\n【1】单线程 \n\n【2】CPU多核多线程\n\n【3】GPU CUDA多线程\n\n";
        cin >> input;
        try {
            input_num = stoul(input);
            if (!(input_num <= 3 && input_num >= 1))
                goto select_3;
        }
        catch (...) {
            goto select_3;
        }

        switch (input_num)
        {
        case SINGLE: {
            Console::PrintSystemInfo(g_system_info);
            for (int i = 0; i < loop_times; ++i) {                      
                cout << endl << "正在压缩.......\n";
                Sleep(500);
                g_M = 0;
                finish_rate = 0.0f;
                g_out_totallen = 0;
                g_src_total_len = 0;
                g_time1 = 0;
                g_time2 = 0;
                SET_SINGLE(g_M);
                if (g_config.use_bwt) {
                    SET_BWT(g_M);
                }
                thread t(Encode, SavePath, 0);
                Console::PrintProcedure();
                t.join();
                total_time += (g_time2 - g_time1);
                cout << "\n当前压缩耗时: " << (g_time2 - g_time1) << "ms\n";
                system("pause");
            }              
            break;
        }
        case MULTI: {
            //SET_BWT(g_M);
            Console::PrintSystemInfo(g_system_info);
            cout << endl << "正在压缩.......\n";
            Sleep(500);
            int num = std::thread::hardware_concurrency();
            num = (num > 0) ? (num / 2) : 2;//取核心并发能力一半 胖线程
            num = g_config.thread_num > 0 ? g_config.thread_num : num;
            for (int i = 0; i < loop_times; ++i) {
                g_M = 0;
                finish_rate = 0.0f;
                g_out_totallen = 0;
                g_src_total_len = 0;
                g_time1 = 0;
                g_time2 = 0;
                if (g_config.use_bwt) {
                    SET_BWT(g_M);
                    SET_P_BWT(g_M);
                }
                BootMutilThread_Encode(SavePath, num, g_config.block_size*1024);
                total_time += (g_time2 - g_time1);
                cout << "\n当前压缩耗时: " << (g_time2 - g_time1) << "ms\n";
            }

            break;
        }
        case CUDA: {
            InitGPU();
            for (int i = 0; i < loop_times; ++i) {
                g_M = 0;
                finish_rate = 0.0f;
                g_out_totallen = 0;
                g_src_total_len = 0;
                g_time1 = 0;
                g_time2 = 0;
                SET_CUDA(g_M);
                thread t(Encode, SavePath, g_config.cuda_block_size*1024);
                Console::PrintProcedure();
                t.join();
                total_time += (g_time2 - g_time1);
                cout << "\n当前压缩耗时: " << (g_time2 - g_time1) << "ms\n";
            }

            break;
        }
        default:
            break;
        }

        cout << "平均耗时:" << total_time * 1.0f / loop_times << "ms\n";
        cout << "压缩前:" << g_src_total_len << "字节\n压缩后:" << g_out_totallen << "字节\n";
        if (g_src_total_len == 0)//防止除0
            cout << "压缩率:" << 1.0 << endl;
        else
            cout << "压缩率:" << g_out_totallen * 1.0f / g_src_total_len << endl;
  
        Sleep(1000);
        string cmd = "start " + PackageRootPath;
        system(cmd.c_str());
        cout << "压缩完毕\n";


    }
    else {
        Console::ClearScreen();
        cout << "===================================== 解压缩 ========================================" << endl << endl;
        select_4:
        cout << "\n请选择一个ivan压缩包\n";
        Sleep(500);
        string pack = Mannger.SelectApackage();
        string outPath;
        if (pack != "") {
            select_5:
            cout << "\n请选择输出目录\n";
            Sleep(500);
            outPath = Mannger.SelectFolder();
            if (outPath != "") {
                cout << "正在解压.......\n";
                Sleep(500);
                g_M = 0;
                total_time = 0;
                for (int i = 0; i < loop_times; ++i) {
                    finish_rate = 0;
                    g_time1 = GetTickCount64();
                    BootMutilThread_Decode(pack, outPath, g_config.thread_num);
                    g_time2 = GetTickCount64();
                    cout << "\n当前解压缩耗时: " << (g_time2 - g_time1) << "ms\n";
                    total_time += g_time2 - g_time1;
                }
                cout << "平均耗时:" << total_time * 1.0f / loop_times << "ms\n";
            }
            else {
          
                cout << "尚未选取目录，按任意键后可重新选择\n\n";
                system("pause");
                goto select_5;
            }
               
        }
        else {
            cout << "尚未选取压缩文件，按任意键后可重新选择\n\n";
            system("pause");
            goto select_4;
        }
            
        //cout<<"请选择解压缩模式:"

        
        cout << "\n解压完毕\n";
        Sleep(500);
        string cmd = "start " + outPath;
        system(cmd.c_str());
    }
    cout << "\n\n";
    system("pause");
}

