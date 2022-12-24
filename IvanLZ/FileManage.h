#pragma once
#include <string>
#include <vector>
#include <stdlib.h>
#include <cstdio>
#include <io.h>
#include <conio.h>
#include <iostream>
#include "FileModuleDef.h"
#include <queue>
using namespace std;

class FileManage
{
public:
	//选择一个文件夹进行压缩
	string SelectFolder();
	//选择一个或多个文件压缩
	vector<string> SelectFiles();
	//将压缩包或者解压文件保存到
	string SaveAs();
	string SelectApackage();
};



