#pragma once
#include <vector>
#include <string>
using namespace std;
#define SET_P_LZO(M)    (M|=(uint8_t)0x01)
#define SET_BWT(M)      (M|=(uint8_t)0x02)
#define SET_MFT(M)      (M|=(uint8_t)0x04)
#define SET_P_BWT(M)    (M|=(uint8_t)0x08)
#define SET_SINGLE(M)   (M|=(uint8_t)0x10)
#define SET_CUDA(M)     (M|=(uint8_t)0x20)

#define IF_LZO(M)       (M&(uint8_t)0x01)
#define IF_BWT(M)       (M&(uint8_t)0x02)
#define IF_MFT(M)       (M&(uint8_t)0x04)
#define IF_P_BWT(M)     (M&(uint8_t)0x08)
#define IF_SINGLE(M)    (M&(uint8_t)0x10)
#define IF_CUDA(M)      (M&(uint8_t)0x20)

typedef struct {
    uint64_t BWT_index;
    unsigned char* pBuff;
    size_t size;
}Channel_Block_Decode;
typedef struct FileNode {
    string filename;
    int  parentId;
    bool isCatalog;
    int id;
    size_t size;
    vector<FileNode*> child;
    string Full_Path;
}FileNode;
typedef string FileName;
typedef string PathName;
typedef struct {
    string RootPath;
    FileNode* root;
    int Nums;//包括根节点
}FileTree;

typedef struct {
    string Path;
    int parentId;
}BFS;

typedef struct {
    unsigned char* out_buffer;//指向压缩后的数据
    size_t out_len;//该块数据的压缩后长度
    //size_t original_size;//原始长度
}outputUint;
typedef struct {
    unsigned char* in_buffer;
    size_t in_len;
}inputUint;

typedef struct HEADER {
    char LABLE[4];//4
    unsigned char M;//1
    uint32_t FileSection_offset;//4
    uint32_t StrSection_offset;//4
    uint32_t DataSection_offset;//4
    unsigned char padding[15];//填充
    //一共32字节 注意编译格式对齐1字节
    HEADER() {
        M = 0;
        LABLE[0] = 'I'; LABLE[1] = 'V'; LABLE[2] = 'A'; LABLE[3] = 'N';
        SET_P_LZO(M);
        //SET_SINGLE(M)
        FileSection_offset = 0;
        StrSection_offset = 0;
        DataSection_offset = 0;
        memset(padding, 0, 15);
    }
}HEADER;

bool ConstructSimpleFileTree(string packagename, vector<string> files);
bool ConstructFileTree(string rootPath);
void DecodeTree(unsigned char* file_section, uint32_t file_section_len, unsigned char* str_section, string rootPath);
void DumpTree(unsigned char** file_output, size_t* file_outlen, unsigned char** string_output, size_t* string_outlen);
void DecodeDumpToFile(vector<outputUint>& outvec);
void EncodeDumpToFile(PathName output_path, size_t block_size, vector<outputUint>& outvec, uint64_t BWT_Index);
void BFS_Folder();
void printBT(const std::string& prefix, const FileNode* node, bool isLeft);
void PrintfFileTree(FileTree& tree);
string getAstring(const unsigned char* str);