#include "stdafx.h"
#include "CrArkKrnl.h"
#include "ntapi.h"
#include "shlwapi.h"

#define SERVICENAME (_T("CrArk"))
#define SERVICEPATH (_T("System\\CurrentControlSet\\Services\\CrArk"))
#define SERVICENAME_UNICODE (L"CrArk")
#define DEVICENAME (_T("\\\\.\\CRARKSYS"))

//�ͷ�������ϵͳĿ¼
//���ɹ�, Path��������·��
BOOL ReleaseDriver(TCHAR* Path)
{
    TCHAR szCurrentDir[MAX_PATH];
    TCHAR szSysDir[MAX_PATH];
    TCHAR szFileDir[MAX_PATH];
    UINT ret;
    BOOL bRet;

    GetCurrentDirectory(MAX_PATH, szCurrentDir);
    lstrcat(szCurrentDir, _T("\\CrArkSys.sys"));
    
    GetSystemDirectory(szSysDir, MAX_PATH);
    ret = GetTempFileName(szSysDir, _T("cr"), 0, szFileDir);

    if(ret == 0)
        return FALSE;

    bRet= CopyFile(szCurrentDir, szFileDir, FALSE);
    if(bRet == FALSE)
        return FALSE;

    lstrcpy(Path, _T("\\??\\"));
    lstrcat(Path, szFileDir);
    return TRUE;
}

//��ע���System\CurrentControlSet\Services����ע��һ������
//������ZwLoadDriver��װ����
BOOL MakeRegistryEntry(TCHAR *Path)
{
    BOOL retVal = FALSE;
    HKEY hKey = NULL;
    DWORD value1, value0;

    if(RegOpenKey(HKEY_LOCAL_MACHINE, SERVICEPATH, &hKey) != ERROR_SUCCESS &&
       RegCreateKey(HKEY_LOCAL_MACHINE, SERVICEPATH, &hKey) != ERROR_SUCCESS)
    {
        //���ܴ�, ���ܴ���, ʧ��
        goto _MakeRegistryEntryExit;
    }
    
    value0 = 0;
    value1 = 1;
    if(RegSetValueEx(hKey, _T("DisplayName"), 0, 
                     REG_SZ, (const PBYTE)SERVICENAME, sizeof(SERVICENAME)) != ERROR_SUCCESS ||
       RegSetValueEx(hKey, _T("ErrorControl"), 0,
                     REG_DWORD, (const PBYTE)&value1, sizeof(value1)) != ERROR_SUCCESS ||
       RegSetValueEx(hKey, _T("ImagePath"), 0,
                     REG_SZ, (const BYTE*)Path, (lstrlen(Path) + 1) * sizeof(TCHAR)) != ERROR_SUCCESS ||
       RegSetValueEx(hKey, _T("Type"), 0,
                     REG_DWORD, (const PBYTE)&value1, sizeof(value1)) != ERROR_SUCCESS ||
       RegSetValueEx(hKey, _T("Start"), 0,
                     REG_DWORD, (const PBYTE)&value0, sizeof(value0)) != ERROR_SUCCESS 
       )
    {
        goto _MakeRegistryEntryExit;
    }

    retVal = TRUE;

_MakeRegistryEntryExit:
    if(hKey != NULL)
    {
        if(retVal == FALSE)
            RegDeleteKey(HKEY_LOCAL_MACHINE, SERVICEPATH);
        RegCloseKey(hKey);
    }
    
    return retVal;
}

BOOL LoadDriver()
{
    BOOL retVal = FALSE;
    TCHAR path[MAX_PATH];
    WCHAR registryPath[MAX_PATH];
    UNICODE_STRING uniDriverName;
    NTSTATUS status;

    //�����ͷ�����
    retVal = ReleaseDriver(path);
    if(!retVal)
        return retVal;

    //��дע�����
    retVal = MakeRegistryEntry(path);
    if(!retVal)
    {
        DeleteFile(path);
        return retVal;
    }
    
    //��������
    wsprintfW(registryPath, L"%s\\%s", 
              L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\",
              SERVICENAME_UNICODE);
    RtlInitUnicodeString(&uniDriverName,
                         registryPath);
    status = ZwLoadDriver(&uniDriverName);
    
    //������Ͽ�������ע�������ļ���
    SHDeleteKey(HKEY_LOCAL_MACHINE, SERVICEPATH);
    DeleteFile(&path[4]);

    retVal = NT_SUCCESS(status);

    return retVal;
}

BOOL CrInitialize()
{
    HANDLE handle;

    //��������Ѿ�����
    handle = CreateFile(DEVICENAME, 
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
    if(handle != INVALID_HANDLE_VALUE)
    {
        DriverHandle = handle;
        return TRUE;
    }

    //����û�м���
    if(!LoadDriver())
        return FALSE;

    handle = CreateFile(DEVICENAME,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
    if(handle == INVALID_HANDLE_VALUE)
        return FALSE;

    DriverHandle = handle;
    return TRUE;
}