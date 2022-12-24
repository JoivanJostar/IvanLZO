#pragma once
#include <iostream>
#include <Windows.h>
#include "global_var.h"
class Console
{
public:
    //设置坐标[x:第x个字符，y:第y行]
    static  void setpos(int x, int y)
    {
        COORD coord;
        coord.X = x;
        coord.Y = y;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);  //回到给定的坐标位置进行重新输出
    }


    // 获取当前标准输出流位置 [x:第x个字符，y:第y行]
    static void getpos(int* x, int* y)
    {
        CONSOLE_SCREEN_BUFFER_INFO b;           // 包含控制台屏幕缓冲区的信息
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &b);    //  获取标准输出句柄
        *x = b.dwCursorPosition.X;
        *y = b.dwCursorPosition.Y;
    }
    static void PrintProcedure() {
        cout << endl;
        int x, y;
        string ch = "";
        int rate_int = 0;
        while (finish_rate < 0.99f) {
            getpos(&x, &y);
            setpos(0, y);
            ch = "";
            rate_int =(int) (finish_rate * 100);
            if (rate_int > 100)rate_int = 100;
            cout << "[ " << rate_int << "%]";
            for (int i = 0; i < (int)(40*finish_rate); ++i) {
                ch += "▉";
            }
            cout << ch;
            Sleep(200);
        }
        getpos(&x, &y);
        setpos(0, y);
        cout << "[100%]▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉▉\n";//40个
    }

   static  void PrintLogo() {
        cout << R"(========================================================================================)" << endl;
        cout << R"(--------- ___________   ___            ___     ____         _____        __          ---)" << endl;
        cout << R"(------   /___   ____/   \  \          /  /    /    \       |     \      |  |       -----)" << endl;
        cout << R"(-----       /  /   ----- \  \        /  /    /  /\  \      |  |\  \     |  |     -------)" << endl;
        cout << R"(--------   /  /  -------  \  \      /  /    /  /  \  \     |  | \  \    |  |    --------)" << endl;
        cout << R"(-------   /  /  ---------- \  \    /  /    /  /____\  \    |  |  \  \   |  |  ----------)" << endl;
        cout << R"(-------  /  /  -----------  \  \  /  /    /  /______\  \   |  |   \  \  |  |  ----------)" << endl;
        cout << R"(----    /  /  -----------    \  \/  /    /  /        \  \  |  |    \  \ |  |   ---------)" << endl;
        cout << R"(--  ___/  /___ -----------    \    /    /  /          \  \ |  |     \  \|  |      ------)" << endl;
        cout << R"(-- /_________/  ---------      \__/    /__/            \__\|__|      \_____|         ---)" << endl;
        cout << R"(-                                                                                      -)" << endl;
        cout << R"(----------------------------------------------------------  By XD Ivan_Zhang   ver 1.0 -)" << endl;
        cout << R"(========================================================================================)" << endl;

    }
    static void ClearScreen() {
        system("cls");
       // PrintLogo();
    }
    static void PrintSystemInfo(const SysInfo& info) {
        printf("正在查询系统硬件信息.........\n");
        Sleep(750);
        printf("\n");
        printf("CPU名称:      %s\n", info.CPU_Name.c_str());
        printf("核心数量:     %d\n", info.Core_Num);
        printf("线程数量:     %d\n", info.Thread_Num);
        printf("内存大小:     %.1fG\n", info.DRAM);
        printf("L2Cache大小:  %dB\n", info.L2CacheSize);
        printf("L3Cache大小:  %dB\n", info.L3CacheSize);
        printf("\n");
    }
};

