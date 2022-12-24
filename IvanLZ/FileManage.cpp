#include "FileManage.h"
#include <Windows.h>
#include <atlstr.h> 
#include <Commdlg.h>
#include <Shlobj.h>


// 
//1.选择一个文件夹进行压缩 
//2.解压文件输出到目录
string FileManage::SelectFolder() {
	TCHAR szBuffer[MAX_PATH] = { 0 };
	BROWSEINFO bi = { 0 };
	bi.hwndOwner = NULL;//拥有着窗口句柄，为NULL表示对话框是非模态的，实际应用中一般都要有这个句柄
	bi.pszDisplayName = szBuffer;//接收文件夹的缓冲区
	bi.lpszTitle = TEXT("选择一个文件夹");//标题
	bi.ulFlags = BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST idl = SHBrowseForFolder(&bi);
	if (SHGetPathFromIDList(idl, szBuffer)) {
		return string(szBuffer);
		//MessageBox(NULL, szBuffer, TEXT("你选择的文件夹"), 0);
	}
	else {
		return "";
		//MessageBox(NULL, TEXT("请选择一个文件夹"), NULL, MB_ICONERROR);
	}
}
//选择一个或多个文件压缩
vector<string>FileManage::SelectFiles() {
    vector <string> out;
    OPENFILENAME ofn;
    TCHAR* szOpenFileNames = new TCHAR[80 * MAX_PATH];
    TCHAR* szPath = new TCHAR[80 * MAX_PATH];
    TCHAR* szFileName=new TCHAR[80 * MAX_PATH];
    memset(szOpenFileNames, 0, 80 * MAX_PATH);
    memset(szPath, 0, 80 * MAX_PATH);
    memset(szFileName, 0, 80 * MAX_PATH);
    TCHAR* p;
    int nLen = 0;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szOpenFileNames;
    ofn.nMaxFile = 80 * MAX_PATH;
    ofn.lpstrFilter = _T("所有文件(*.*)\0*.*\0\0");
    ofn.nFilterIndex = 1;
    // ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn))
    {
        //把第一个文件名前的复制到szPath,即:
       //如果只选了一个文件,就复制到最后一个'/'
      //如果选了多个文件,就复制到第一个NULL字符
        lstrcpyn(szPath, szOpenFileNames, ofn.nFileOffset);
        //当只选了一个文件时,下面这个NULL字符是必需的.
        //这里不区别对待选了一个和多个文件的情况

        szPath[ofn.nFileOffset] = '\0';
        nLen = lstrlen(szPath);

        if (szPath[nLen - 1] != '\\')   //如果选了多个文件,则必须加上'\\'
        {
            lstrcat(szPath, TEXT("\\"));
        }


        p = szOpenFileNames + ofn.nFileOffset; //把指针移到第一个文件

        ZeroMemory(szFileName, sizeof(szFileName));
        while (*p)
        {
            lstrcat(szFileName, szPath);  //给文件名加上路径  
            lstrcat(szFileName, p);    //加上文件名    
            lstrcat(szFileName, TEXT("\n")); //换行 
            p += lstrlen(p) + 1;     //移至下一个文件
        }
        lstrcat(szFileName, TEXT("\0")); 
        const char s[2] = "\n";
        char* token;
        /* 获取第一个子字符串 */
        token = strtok(szFileName, s);
        /* 继续获取其他的子字符串 */
        while (token != NULL) {
            out.push_back(string(token));
            token = strtok(NULL, s);
        }
    }
    delete[] szOpenFileNames;
    delete[] szPath;
    delete[] szFileName;
    return out;
}

//将压缩包保存到
string FileManage::SaveAs() {
    OPENFILENAME ofn = { 0 };
    TCHAR strFilename[MAX_PATH] = { 0 };//用于接收文件名
    ofn.lStructSize = sizeof(OPENFILENAME);//结构体大小
    ofn.hwndOwner = NULL;//拥有着窗口句柄，为NULL表示对话框是非模态的，实际应用中一般都要有这个句柄
    ofn.lpstrFilter = _T("IVAN 标准压缩文件(.ivan)\0*.ivan\0\0");//设置过滤
    ofn.nFilterIndex = 1;//过滤器索引
    ofn.lpstrFile = strFilename;//接收返回的文件名，注意第一个字符需要为NULL
    ofn.nMaxFile = sizeof(strFilename);//缓冲区长度
    ofn.lpstrInitialDir = NULL;//初始目录为默认

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;//目录必须存在，覆盖文件前发出警告
    ofn.lpstrTitle = TEXT("保存到");//使用系统默认标题留空即可
    ofn.lpstrDefExt = TEXT("ivan");//默认追加的扩展名
    if (GetSaveFileName(&ofn))
    {
        return strFilename;
    }
    else {
        return "";
    }
}


//解压缩：

//选择一个压缩文件解压
string FileManage:: SelectApackage() {
    OPENFILENAME ofn = { 0 };
 	TCHAR strFilename[MAX_PATH] = { 0 };//用于接收文件名
 	ofn.lStructSize = sizeof(OPENFILENAME);//结构体大小
 	ofn.hwndOwner = NULL;//拥有着窗口句柄，为NULL表示对话框是非模态的，实际应用中一般都要有这个句柄
 	ofn.lpstrFilter = TEXT("IVAN压缩文件(*.ivan)\0*.ivan\0\0");//设置过滤
 	ofn.nFilterIndex = 1;//过滤器索引
 	ofn.lpstrFile = strFilename;//接收返回的文件名，注意第一个字符需要为NULL
 	ofn.nMaxFile = sizeof(strFilename);//缓冲区长度
 	ofn.lpstrInitialDir = NULL;//初始目录为默认
 	ofn.lpstrTitle = TEXT("请选择一个压缩文件");//使用系统默认标题留空即可
 	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY ;//文件、目录必须存在，隐藏只读选项
 	if (GetOpenFileName(&ofn))
 	{
        return string(strFilename);//, TEXT("选择的文件"), 0);
 	}
 	else {
        return "";
 	}
}



