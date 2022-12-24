#pragma once
#include <vector>
#include <string>
#include "FileModuleDef.h"

using namespace std;

void Encode(string output_path, size_t block_size);
void CudaEncode(string output_path, size_t block_size);
void EncodeDumpToFile(PathName output_path, size_t block_size, vector<outputUint>& outvec, uint64_t BWT_Index);
void BootMutilThread_Encode( PathName toWhere, int ThreadNum, size_t blocksize);