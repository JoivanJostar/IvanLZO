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
	//ѡ��һ���ļ��н���ѹ��
	string SelectFolder();
	//ѡ��һ�������ļ�ѹ��
	vector<string> SelectFiles();
	//��ѹ�������߽�ѹ�ļ����浽
	string SaveAs();
	string SelectApackage();
};



