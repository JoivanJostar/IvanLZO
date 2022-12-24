#include "FileModuleDef.h"
#include "global_var.h"
#include <io.h>
#include <direct.h>
#include <Windows.h>
#include <iostream>
#include <fstream>
using namespace std;
void dfsFolder(string folderPath)
{
    _finddata_t FileInfo;
    intptr_t Handle = -1;
    string strfind = folderPath + "\\*";
    Handle = _findfirst(strfind.c_str(), &FileInfo);

    if (Handle == -1L)
    {
        cerr << "can not match the folder path" << endl;
        exit(-1);
    }
    do {
        //判断是否有子目录 
        if (FileInfo.attrib & _A_SUBDIR)
        {
            //这个语句很重要 
            if ((strcmp(FileInfo.name, ".") != 0) && (strcmp(FileInfo.name, "..") != 0))
            {
                string newPath = folderPath + "\\" + FileInfo.name;
                dfsFolder(newPath);
            }
        }
        else
        {
            // fout << folderPath.c_str() << "\\" << FileInfo.name << " ";
            cout << folderPath.c_str() << "\\" << FileInfo.name << " size: " << FileInfo.size << endl;
        }
    } while (_findnext(Handle, &FileInfo) == 0);

    _findclose(Handle);
    //fout.close();
}

//用指定的多个文件构造一个单层的文件树 
// vector中每个string 是完整路径名。
//注意提前检测压缩包名称的合法性，可以用windows另存为功能强行让其合法
bool ConstructSimpleFileTree(FileName packagename, vector<string> files) {
    FileVectorInBFSOrder.clear();
    tree.Nums = 0;
    FileNode* file;

    file = new FileNode;
    file->filename = packagename;
    file->id = tree.Nums++;
    file->isCatalog = true;
    file->size = 0;
    file->parentId = -1;
    tree.RootPath = "";
    tree.root = file;
    FileVectorInBFSOrder.push_back(file);
    ifstream infile;
    size_t filelen;
    string path;
    for (int i = 0; i < files.size(); ++i) {
        infile.open(files[i], ifstream::binary);
        infile.seekg(0, infile.end);
        filelen = infile.tellg();
        infile.close();
        file = new FileNode;
        file->parentId = 0;
        file->size = filelen;
        file->id = tree.Nums++;
        file->isCatalog = false;
        auto it = files[i].end() - 1;
        for (; it != files[i].begin(); --it) {
            if (*it == '\\' || *it == '/')
                break;
        }
        file->filename = string(it + 1, files[i].end());
        path = string(files[i].begin(), it);//获得路径前缀
        file->Full_Path = path + "\\" + file->filename;
        FileVectorInBFSOrder.push_back(file);
        tree.root->child.push_back(file);
    }
    tree.RootPath = path;//别忘了写入根路径，用于后面压缩时能够找到文件
    return true;
}
//调用前必然路径存在
void BFS_Folder() {
    _finddata_t FileInfo;
    intptr_t Handle = -1;
    int parentId = -1;
    BFS bfs;
    string strfind;
    while (!que.empty()) {
        BFS bfs = que.front();
        que.pop();
        parentId = bfs.parentId;//更新parentId
        strfind = bfs.Path + "\\*";
        Handle = _findfirst(strfind.c_str(), &FileInfo);
        do {
            //判断是否是目录文件
            if (FileInfo.attrib & _A_SUBDIR)//子目录
            {
                //这个语句很重要 
                if ((strcmp(FileInfo.name, ".") != 0) && (strcmp(FileInfo.name, "..") != 0))
                {
                    //该目录文件加入文件树
                    FileNode* dirfile = new FileNode;
                    dirfile->filename = FileInfo.name;
                    dirfile->size = 0;
                    dirfile->isCatalog = true;
                    dirfile->parentId = parentId;
                    dirfile->id = tree.Nums++;
                    dirfile->Full_Path = FileVectorInBFSOrder[dirfile->parentId]->Full_Path + "\\" + dirfile->filename;
                    FileVectorInBFSOrder.push_back(dirfile);
                    FileVectorInBFSOrder[parentId]->child.push_back(dirfile);
                    BFS foo;
                    foo.parentId = dirfile->id;
                    foo.Path = bfs.Path + "\\" + FileInfo.name;
                    que.push(foo);//加入队列等待下一层BFS遍历
                }
            }
            else//非子目录
            {
                FileNode* file = new FileNode;
                file->filename = FileInfo.name;
                file->size = FileInfo.size;
                file->isCatalog = false;
                file->parentId = parentId;
                file->id = tree.Nums++;
                file->Full_Path = FileVectorInBFSOrder[parentId]->Full_Path + "\\" + file->filename;
                FileVectorInBFSOrder.push_back(file);//加入到树中
                FileVectorInBFSOrder[parentId]->child.push_back(file);

            }
        } while (_findnext(Handle, &FileInfo) == 0);//当前目录下所有文件遍历完毕

    }


    _findclose(Handle);
}

//指定一个文件夹，以此文件夹的路径为根目录构造文件树
bool ConstructFileTree(string rootPath) {
    _finddata_t FileInfo;
    intptr_t Handle = -1;
    string strfind = rootPath + "\\*";
    Handle = _findfirst(strfind.c_str(), &FileInfo);//
    if (Handle == -1) {

        return false;
    }
    else {
        //创建根节点
        FileNode* file = new FileNode;
        string::iterator it = rootPath.end() - 1;

        for (; it != rootPath.begin(); --it) {
            if (*it == '\\' || *it == '/')
                break;
        }
        //distance()
        string filename{ it + 1,rootPath.end() };
        file->filename = filename;
        file->Full_Path = rootPath;
        file->id = tree.Nums++;
        file->isCatalog = true;
        file->parentId = -1;
        file->size = 0;
        tree.root = file;
        tree.RootPath = rootPath;
        FileVectorInBFSOrder.push_back(file);
        BFS foo;
        foo.parentId = 0;
        foo.Path = rootPath;
        que.push(foo);
        BFS_Folder();
        _findclose(Handle);
        return true;
    }
}

void printBT(const std::string& prefix, const FileNode* node, bool isLeft) {
    if (node != nullptr) {
        std::cout << prefix << "|" << endl;;
        std::cout << prefix;
        std::cout << (isLeft ? "|-----" : "|-----");
        //cout << endl;

        // print the value of the node
        std::cout << node->filename;
        if (node->isCatalog == true)
            std::cout << ":" << std::endl;
        else
            std::cout << endl;

        // enter the next tree level - left and right branch
        int i = 0;
        if (node->child.size() != 0) {
            for (i = 0; i < node->child.size() - 1; ++i) {
                printBT(prefix + (isLeft ? "|      " : "      "), node->child[i], true);

            }
            printBT(prefix + (isLeft ? "|      " : "      "), node->child[i], false);
        }

    }
}
void PrintfFileTree(FileTree& tree) {
    //printBT()
    FileNode* root = tree.root;
    if (root != NULL) {
        printBT("  ", root, false);
    }
}

//将文件树dump到output缓冲区，文件名dump到字串缓冲区
//函数内部分配out空间，用完记得释放
void DumpTree(unsigned char** file_output, size_t* file_outlen, unsigned char** string_output, size_t* string_outlen) {
    //计算file_output一共需要多少空间
    size_t foplen = (sizeof(bool) + sizeof(size_t) + sizeof(int32_t)) * FileVectorInBFSOrder.size();//13字节*文件个数
    *file_output = new unsigned char[foplen];
    unsigned char* fop = *file_output;
    std::memset(fop, 0, foplen);

    //计算string_output一共需要多少空间
    size_t soplen = 0;
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        soplen += (1 + FileVectorInBFSOrder[i]->filename.length());//1字节的'\0'+字符串长度
    }
    *string_output = new unsigned char[soplen];
    unsigned char* sop = *string_output;
    std::memset(sop, 0, soplen);

    //进行Dump
    for (int i = 0; i < FileVectorInBFSOrder.size(); ++i) {
        FileNode* file = FileVectorInBFSOrder[i];
        memcpy(fop, &(file->isCatalog), sizeof(bool));//1Byte 文件类型
        fop++;
        memcpy(fop, &(file->size), sizeof(size_t));//8Byte原文件长度
        fop += sizeof(size_t);
        memcpy(fop, &(file->parentId), sizeof(int32_t));//4Byte父亲节点
        fop += sizeof(int32_t);
        memcpy(sop, &(file[i].filename[0]), file[i].filename.length());//字串dump到字符串节区
        sop += file[i].filename.length();
        *sop++ = '\0';
    }
    assert((fop - *file_output) == foplen);
    assert((sop - *string_output) == soplen);
    *file_outlen = foplen;
    *string_outlen = soplen;
}

string getAstring(const unsigned char* str) {
    int i = 0;
    string s = "";
    while (str[i] != '\0') {
        s += str[i];
        i++;
    }
    return string(s);
}

//提前把各个所需节区的数据读到缓冲区中传入进来解析出文件树
void DecodeTree(unsigned char* file_section, uint32_t file_section_len, unsigned char* str_section, string rootPath) {
    int Nums = file_section_len / (1 + 8 + 4);//文件个数
    assert(Nums > 0);
    assert(file_section_len % (1 + 8 + 4) == 0);
    FileVectorInBFSOrder.clear();
    tree.Nums = 0;
    //tree.RootPath = rootPath;
    unsigned char* fin = file_section;
    unsigned char* sin = str_section;
    //对根节点解码
    FileNode* file = new FileNode;
    memcpy(&file->isCatalog, fin, sizeof(bool));
    fin++;
    memcpy(&file->size, fin, sizeof(size_t));
    fin += sizeof(size_t);
    memcpy(&file->parentId, fin, sizeof(int32_t));
    fin += sizeof(int32_t);
    file->parentId = -1;
    file->id = tree.Nums++;
    string name = getAstring(sin);
    sin += name.length() + 1;//注意+1
    file->filename = name;
    file->Full_Path = rootPath + "\\" + file->filename;
    FileVectorInBFSOrder.push_back(file);
    tree.root = file;
    tree.RootPath = tree.root->Full_Path;
    //对其他节点解码
    for (int i = 1; i < Nums; ++i) {
        file = new FileNode;
        memcpy(&file->isCatalog, fin, sizeof(bool));//1字节
        fin++;
        memcpy(&file->size, fin, sizeof(size_t));//8字节
        fin += sizeof(size_t);
        memcpy(&file->parentId, fin, sizeof(int32_t));//4字节
        fin += sizeof(int32_t);
        file->id = tree.Nums++;
        name = getAstring(sin);
        sin += name.length() + 1;//注意+1
        file->filename = name;
        file->Full_Path = FileVectorInBFSOrder[file->parentId]->Full_Path + "\\" + file->filename;
        FileVectorInBFSOrder.push_back(file);
        FileVectorInBFSOrder[file->parentId]->child.push_back(file);//添加父亲->孩子的连接关系
    }
}
