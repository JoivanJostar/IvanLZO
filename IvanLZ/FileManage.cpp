#include "FileManage.h"
#include <Windows.h>
#include <atlstr.h> 
#include <Commdlg.h>
#include <Shlobj.h>


// 
//1.ѡ��һ���ļ��н���ѹ�� 
//2.��ѹ�ļ������Ŀ¼
string FileManage::SelectFolder() {
	TCHAR szBuffer[MAX_PATH] = { 0 };
	BROWSEINFO bi = { 0 };
	bi.hwndOwner = NULL;//ӵ���Ŵ��ھ����ΪNULL��ʾ�Ի����Ƿ�ģ̬�ģ�ʵ��Ӧ����һ�㶼Ҫ��������
	bi.pszDisplayName = szBuffer;//�����ļ��еĻ�����
	bi.lpszTitle = TEXT("ѡ��һ���ļ���");//����
	bi.ulFlags = BIF_NEWDIALOGSTYLE;
	LPITEMIDLIST idl = SHBrowseForFolder(&bi);
	if (SHGetPathFromIDList(idl, szBuffer)) {
		return string(szBuffer);
		//MessageBox(NULL, szBuffer, TEXT("��ѡ����ļ���"), 0);
	}
	else {
		return "";
		//MessageBox(NULL, TEXT("��ѡ��һ���ļ���"), NULL, MB_ICONERROR);
	}
}
//ѡ��һ�������ļ�ѹ��
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
    ofn.lpstrFilter = _T("�����ļ�(*.*)\0*.*\0\0");
    ofn.nFilterIndex = 1;
    // ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn))
    {
        //�ѵ�һ���ļ���ǰ�ĸ��Ƶ�szPath,��:
       //���ֻѡ��һ���ļ�,�͸��Ƶ����һ��'/'
      //���ѡ�˶���ļ�,�͸��Ƶ���һ��NULL�ַ�
        lstrcpyn(szPath, szOpenFileNames, ofn.nFileOffset);
        //��ֻѡ��һ���ļ�ʱ,�������NULL�ַ��Ǳ����.
        //���ﲻ����Դ�ѡ��һ���Ͷ���ļ������

        szPath[ofn.nFileOffset] = '\0';
        nLen = lstrlen(szPath);

        if (szPath[nLen - 1] != '\\')   //���ѡ�˶���ļ�,��������'\\'
        {
            lstrcat(szPath, TEXT("\\"));
        }


        p = szOpenFileNames + ofn.nFileOffset; //��ָ���Ƶ���һ���ļ�

        ZeroMemory(szFileName, sizeof(szFileName));
        while (*p)
        {
            lstrcat(szFileName, szPath);  //���ļ�������·��  
            lstrcat(szFileName, p);    //�����ļ���    
            lstrcat(szFileName, TEXT("\n")); //���� 
            p += lstrlen(p) + 1;     //������һ���ļ�
        }
        lstrcat(szFileName, TEXT("\0")); 
        const char s[2] = "\n";
        char* token;
        /* ��ȡ��һ�����ַ��� */
        token = strtok(szFileName, s);
        /* ������ȡ���������ַ��� */
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

//��ѹ�������浽
string FileManage::SaveAs() {
    OPENFILENAME ofn = { 0 };
    TCHAR strFilename[MAX_PATH] = { 0 };//���ڽ����ļ���
    ofn.lStructSize = sizeof(OPENFILENAME);//�ṹ���С
    ofn.hwndOwner = NULL;//ӵ���Ŵ��ھ����ΪNULL��ʾ�Ի����Ƿ�ģ̬�ģ�ʵ��Ӧ����һ�㶼Ҫ��������
    ofn.lpstrFilter = _T("IVAN ��׼ѹ���ļ�(.ivan)\0*.ivan\0\0");//���ù���
    ofn.nFilterIndex = 1;//����������
    ofn.lpstrFile = strFilename;//���շ��ص��ļ�����ע���һ���ַ���ҪΪNULL
    ofn.nMaxFile = sizeof(strFilename);//����������
    ofn.lpstrInitialDir = NULL;//��ʼĿ¼ΪĬ��

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;//Ŀ¼������ڣ������ļ�ǰ��������
    ofn.lpstrTitle = TEXT("���浽");//ʹ��ϵͳĬ�ϱ������ռ���
    ofn.lpstrDefExt = TEXT("ivan");//Ĭ��׷�ӵ���չ��
    if (GetSaveFileName(&ofn))
    {
        return strFilename;
    }
    else {
        return "";
    }
}


//��ѹ����

//ѡ��һ��ѹ���ļ���ѹ
string FileManage:: SelectApackage() {
    OPENFILENAME ofn = { 0 };
 	TCHAR strFilename[MAX_PATH] = { 0 };//���ڽ����ļ���
 	ofn.lStructSize = sizeof(OPENFILENAME);//�ṹ���С
 	ofn.hwndOwner = NULL;//ӵ���Ŵ��ھ����ΪNULL��ʾ�Ի����Ƿ�ģ̬�ģ�ʵ��Ӧ����һ�㶼Ҫ��������
 	ofn.lpstrFilter = TEXT("IVANѹ���ļ�(*.ivan)\0*.ivan\0\0");//���ù���
 	ofn.nFilterIndex = 1;//����������
 	ofn.lpstrFile = strFilename;//���շ��ص��ļ�����ע���һ���ַ���ҪΪNULL
 	ofn.nMaxFile = sizeof(strFilename);//����������
 	ofn.lpstrInitialDir = NULL;//��ʼĿ¼ΪĬ��
 	ofn.lpstrTitle = TEXT("��ѡ��һ��ѹ���ļ�");//ʹ��ϵͳĬ�ϱ������ռ���
 	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY ;//�ļ���Ŀ¼������ڣ�����ֻ��ѡ��
 	if (GetOpenFileName(&ofn))
 	{
        return string(strFilename);//, TEXT("ѡ����ļ�"), 0);
 	}
 	else {
        return "";
 	}
}



