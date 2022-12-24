#pragma once
#include <vector>
#include <string>
#include "FileModuleDef.h"
using namespace std;
void DecodeDumpToFile(vector<outputUint>& outvec);
bool SingleDecode(PathName filePath, PathName outputPath);
void BootMutilThread_Decode(PathName fromWhere, PathName toWhere, int ThreadNum);