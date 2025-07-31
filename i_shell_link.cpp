#include <windows.h>
#include <shlobj.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <Intshcut.h>

#include <stdio.h>
#include <assert.h> 
#include <stdlib.h>
#include <time.h>

#include "goicondump.h"

#define MALLOC_HEADROOM 64

int writelog(WCHAR *wstr, const char *prepend, const char *append = "") {
    char *str;
    int utf8strlen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    str = (char*) malloc(utf8strlen+3);
    utf8strlen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, utf8strlen, NULL, NULL);
    printf("%s%s%s\n", prepend, str, append);
    free(str);
    return 0;
}

int proceed_desktop_shortcut(GUID targetLocationGuid, WCHAR *targetBackupPath, WCHAR *backupFolderName, WCHAR *iconPath, int iconIndex) {
    PWSTR comRetTmpStr;
    WCHAR *targetLocationPath = NULL, *targetLocationPathWildcard = NULL, *backupLocationPath = NULL; 
    WCHAR *realFullFileName = NULL, *backupFullFileName = NULL, *extensionName = NULL;
    WIN32_FIND_DATAW filedata;
    HANDLE findfilehandle = NULL;
    IShellLinkW *shellLinkPtr = NULL;
    IPersistFile *persistFilePtr = NULL;
    HRESULT retval;
    int returncode = 0;

    WCHAR targetIconPath[5001];
    int targetIconIndex = 0;

    retval = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_ALL, IID_IShellLinkW, (void**) &shellLinkPtr);
    if(!SUCCEEDED(retval)) {
        puts("Fail to init IShellLink");
        return -2;
    }
    retval = shellLinkPtr->QueryInterface(IID_IPersistFile, (void**) &persistFilePtr);
    if(!SUCCEEDED(retval)) {
        puts("Fail to init IPersistFile");
        shellLinkPtr->Release();
        return -3;
    }
    
    retval = SHGetKnownFolderPath(targetLocationGuid, 0, NULL, &comRetTmpStr);
    targetLocationPath = (WCHAR*) malloc(wcslen(comRetTmpStr)*2+MALLOC_HEADROOM);
    wcscpy(targetLocationPath, comRetTmpStr);
    wcscat(targetLocationPath, L"\\*");     //append asterisk wildcard
    CoTaskMemFree(comRetTmpStr);

    //create innie folder
    backupLocationPath = (WCHAR*) malloc(wcslen(targetBackupPath)*2 + wcslen(backupFolderName)*2+MALLOC_HEADROOM);
    wcscpy(backupLocationPath, targetBackupPath);
    wcscat(backupLocationPath, backupFolderName);
    wcscat(backupLocationPath, L"\\");
    if(!CreateDirectoryW(backupLocationPath, NULL)) {
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            puts("Can't create innie folder");
            returncode = -4;
            goto proceed_shortcut_desktop_fatal_return;
        }
    }
    
    findfilehandle = FindFirstFileW(targetLocationPath, &filedata);
    if(findfilehandle == INVALID_HANDLE_VALUE) {
        puts("can't indexing file!\n");
        returncode = -1;
        goto proceed_shortcut_desktop_fatal_return;
    }
    targetLocationPath[wcslen(targetLocationPath)-1] = (WCHAR) 0;     //pop asterisk wildcard after use
    do {
        extensionName = PathFindExtensionW(filedata.cFileName);
        if(wcscmp(extensionName, L".lnk") == 0) {
            //real target path
            realFullFileName = (WCHAR*) malloc(wcslen(targetLocationPath)*2 + wcslen(filedata.cFileName)*2 +MALLOC_HEADROOM);
            wcscpy(realFullFileName, targetLocationPath);
            wcscat(realFullFileName, filedata.cFileName);
            (void) writelog(realFullFileName, "Real path: ");
            //backup target path
            backupFullFileName = (WCHAR*) malloc(wcslen(backupLocationPath)*2 + wcslen(filedata.cFileName)*2 +MALLOC_HEADROOM);
            wcscpy(backupFullFileName, backupLocationPath);
            wcscat(backupFullFileName, filedata.cFileName);
            (void) writelog(backupFullFileName, "Backup path: ");

            //remove readonly and hidden attribute if present
            DWORD clearFileAttributeFlag = 0;
            if(filedata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                clearFileAttributeFlag |= FILE_ATTRIBUTE_HIDDEN;
            }
            if(filedata.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                clearFileAttributeFlag |= FILE_ATTRIBUTE_READONLY;
            }
            if(SetFileAttributesW(realFullFileName, filedata.dwFileAttributes ^ clearFileAttributeFlag) == 0) {
                (void) printf("Can't set file attribute. Admin privl missing\n");
                goto proceed_shortcut_desktop_loop_return;
            }

            //interact
            retval = persistFilePtr->Load(realFullFileName, STGM_READ);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't load?\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes);  //reset file attribute (if possible)
                goto proceed_shortcut_desktop_loop_return;
            }
            retval = shellLinkPtr->Resolve(NULL, 1);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't resolve?\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes);  //reset file attribute (if possible)
                goto proceed_shortcut_desktop_loop_return;
            }

            //Check to avoid double-set, hence override original icon
            retval = shellLinkPtr->GetIconLocation(targetIconPath, 5000, &targetIconIndex);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't get icon\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_desktop_loop_return;
            }
            if(wcscmp(targetIconPath, iconPath) != 0) {
                //copy backup file
                retval = CopyFileW(realFullFileName, backupFullFileName, FALSE);
                if(retval == 0) {
                    (void) printf("Fail to create backup!\n");
                    SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                    returncode = -5;
                    goto proceed_shortcut_desktop_loop_return;
                }

                //set icon
                retval = shellLinkPtr->SetIconLocation(iconPath, iconIndex);
                if(!SUCCEEDED(retval)) {
                    (void) printf("Can't set icon\n");
                    SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                    goto proceed_shortcut_desktop_loop_return;
                }

                //save back
                retval = persistFilePtr->Save(realFullFileName, TRUE);
                if(!SUCCEEDED(retval)) {
                    (void) printf("Can't save. Is administrator privileges missing?");
                    SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                    goto proceed_shortcut_desktop_loop_return;
                }
            } else {
                (void) printf("Filtered out\n");
            }

            proceed_shortcut_desktop_loop_return:
            //free mem
            free(realFullFileName);
            free(backupFullFileName);
        }
    } while(FindNextFileW(findfilehandle, &filedata) != 0);
    retval = FindClose(findfilehandle);

    proceed_shortcut_desktop_fatal_return:
    shellLinkPtr->Release();
    persistFilePtr->Release();
    free(targetLocationPath);
    free(backupLocationPath);
    return returncode;
}

int proceed_internet_shortcut(GUID targetLocationGuid, WCHAR *targetBackupPath, WCHAR *backupFolderName, WCHAR *iconPath, int iconIndex) {
    PWSTR comRetTmpStr;
    WCHAR *targetLocationPath = NULL, *backupLocationPath = NULL; 
    WCHAR *realFullFileName = NULL, *backupFullFileName = NULL, *extensionName = NULL;
    WIN32_FIND_DATAW filedata;
    HANDLE findfilehandle = NULL;
    IUniformResourceLocatorW *uniformResourceLocatorPtr;
    IPersistFile *persistFilePtr;
    IPropertySetStorage *propertySetStoragePtr;
    IPropertyStorage *propertyStoragePtr;
    const PROPSPEC propertyStorageQueryField[2] = { {PRSPEC_PROPID, PID_IS_ICONINDEX}, {PRSPEC_PROPID, PID_IS_ICONFILE} };
    PROPVARIANT propertyVariant[2];
    HRESULT retval;
    int returncode = 0;

    retval = SHGetKnownFolderPath(targetLocationGuid, 0, NULL, &comRetTmpStr);
    targetLocationPath = (WCHAR*) malloc(wcslen(comRetTmpStr)*2+MALLOC_HEADROOM); 
    wcscpy(targetLocationPath, comRetTmpStr);
    wcscat(targetLocationPath, L"\\*");     //append asterisk wildcard
    CoTaskMemFree(comRetTmpStr);

    //create innie folder
    backupLocationPath = (WCHAR*) malloc(wcslen(targetBackupPath)*2 + wcslen(backupFolderName)*2 + MALLOC_HEADROOM);
    wcscpy(backupLocationPath, targetBackupPath);
    wcscat(backupLocationPath, backupFolderName);
    wcscat(backupLocationPath, L"\\");
    if(!CreateDirectoryW(backupLocationPath, NULL)) {
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            puts("Can't create innie folder");
            returncode = -4;
            goto proceed_shortcut_internet_fatal_return;
        }
    }
    
    findfilehandle = FindFirstFileW(targetLocationPath, &filedata);
    if(findfilehandle == INVALID_HANDLE_VALUE) {
        printf("can't indexing file!\n");
        returncode = -5;
        goto proceed_shortcut_internet_fatal_return;
    }
    targetLocationPath[wcslen(targetLocationPath)-1] = (WCHAR) 0;     //pop asterisk wildcard after use
    do {
        extensionName = PathFindExtensionW(filedata.cFileName);
        if(wcscmp(extensionName, L".url") == 0) {
            //load interface
            retval = CoCreateInstance(CLSID_InternetShortcut, NULL, CLSCTX_ALL, IID_IUniformResourceLocatorW, (void**) &uniformResourceLocatorPtr);
            if(!SUCCEEDED(retval)) {
                puts("Fail to init IUniformResourceLocatorW");
                return -1;
            }
            retval = uniformResourceLocatorPtr->QueryInterface(IID_IPersistFile, (void**) &persistFilePtr);
            if(!SUCCEEDED(retval)) {
                puts("Fail to init IPersistFile");
                uniformResourceLocatorPtr->Release();
                return -2;
            }
            retval = persistFilePtr->QueryInterface(IID_IPropertySetStorage, (void**) &propertySetStoragePtr);
            if(!SUCCEEDED(retval)) {
                puts("Fail to init IPropertySetStorage");
                persistFilePtr->Release();
                uniformResourceLocatorPtr->Release();
                return -3;
            }
            retval = propertySetStoragePtr->Open(FMTID_Intshcut, STGM_READWRITE, &propertyStoragePtr);
            if(!SUCCEEDED(retval)) {
                puts("Fail to init IPropertyStorage");
                propertySetStoragePtr->Release();
                persistFilePtr->Release();
                uniformResourceLocatorPtr->Release();
                return -4;
            }

            //real target path
            realFullFileName = (WCHAR*) malloc(wcslen(targetLocationPath)*2 + wcslen(filedata.cFileName)*2 + MALLOC_HEADROOM);
            wcscpy(realFullFileName, targetLocationPath);
            wcscat(realFullFileName, filedata.cFileName);
            (void) writelog(realFullFileName, "Real path: ");
            //backup target path
            backupFullFileName = (WCHAR*) malloc(wcslen(backupLocationPath)*2 + wcslen(filedata.cFileName)*2 + MALLOC_HEADROOM);
            wcscpy(backupFullFileName, backupLocationPath);
            wcscat(backupFullFileName, filedata.cFileName);
            (void) writelog(backupFullFileName, "Backup path: ");

            //remove readonly and hidden attribute if present
            DWORD clearFileAttributeFlag = 0;
            if(filedata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
                clearFileAttributeFlag |= FILE_ATTRIBUTE_HIDDEN;
            }
            if(filedata.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                clearFileAttributeFlag |= FILE_ATTRIBUTE_READONLY;
            }
            if(SetFileAttributesW(realFullFileName, filedata.dwFileAttributes ^ clearFileAttributeFlag) == 0) {
                (void) puts("Can't set file attribute");
                goto proceed_shortcut_internet_loop_return;
            }

            //interact
            retval = persistFilePtr->Load(realFullFileName, STGM_READ);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't load?\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_internet_loop_return;
            }

            //Init PROPVARIANT
            PropVariantInit(&propertyVariant[0]);
            PropVariantInit(&propertyVariant[1]);

            //Read to check to avoid double-set, hence override original icon
            retval = propertyStoragePtr->ReadMultiple(2, propertyStorageQueryField, propertyVariant);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't get icon\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_internet_loop_return;
            }
            //check if no icon was found (prob inherited from process)
            if(propertyVariant[1].pwszVal != NULL) {
                //Check to avoid double-set, hence override original icon
                //Path returned is in forward-slash, with file:/// prepended. Need to convert
                WCHAR *convertedTargetIconPath = (WCHAR*) malloc(wcslen(propertyVariant[1].pwszVal)*2+MALLOC_HEADROOM);
                wcscpy(convertedTargetIconPath, propertyVariant[1].pwszVal);
                convertedTargetIconPath = &convertedTargetIconPath[8];
                for(WCHAR *tptr = convertedTargetIconPath; *tptr != 0; tptr++) {
                    if(*tptr == L'/') *tptr = L'\\';
                }

                if(wcscmp(convertedTargetIconPath, iconPath) == 0) {
                    (void) printf("Filtered out\n");
                    free(convertedTargetIconPath-8);    //restore original pointer before free
                    goto proceed_shortcut_internet_loop_return;
                }
                free(convertedTargetIconPath-8);    //restore original pointer before free
            }
            
            //copy backup file
            retval = CopyFileW(realFullFileName, backupFullFileName, FALSE);
            if(retval == 0) {
                (void) printf("Fail to create backup!\n");
                returncode = -5;
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_internet_loop_return;
            }

            //set icon
            propertyVariant[0].vt = VT_I4;
            propertyVariant[0].lVal = iconIndex;
            propertyVariant[1].vt = VT_LPWSTR;
            propertyVariant[1].pwszVal = iconPath;
            retval = propertyStoragePtr->WriteMultiple(2, propertyStorageQueryField, propertyVariant, 0);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't set icon\n");
                goto proceed_shortcut_internet_loop_return;
            }

            //commit
            retval = propertyStoragePtr->Commit(STGC_DEFAULT);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't commit\n");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_internet_loop_return;
            }

            //save back
            retval = persistFilePtr->Save(realFullFileName, TRUE);
            if(!SUCCEEDED(retval)) {
                (void) printf("Can't save. Is administrator privileges missing? ");
                SetFileAttributesW(realFullFileName, filedata.dwFileAttributes); 
                goto proceed_shortcut_internet_loop_return;
            }

            proceed_shortcut_internet_loop_return:
            //free mem
            retval = propertySetStoragePtr->Release();
            retval = propertyStoragePtr->Release();
            retval = persistFilePtr->Release();
            retval = uniformResourceLocatorPtr->Release();
            free(realFullFileName);
            free(backupFullFileName);
        }
    } while(FindNextFileW(findfilehandle, &filedata) != 0);
    retval = FindClose(findfilehandle);

    proceed_shortcut_internet_fatal_return:
    free(backupLocationPath);
    free(targetLocationPath);
    return returncode;
}

int recover(WCHAR *const_backupLocationPathNoWildcard, WCHAR *backupFolderName, GUID targetLocationGuid, const WCHAR *targetExtension) {
    WIN32_FIND_DATAW filedata;
    HANDLE findfilehandle = NULL;
    WCHAR *extensionName = NULL, *realFullFileName = NULL, *backupFullFileName = NULL;
    WCHAR *targetLocationPath = NULL, *backupLocationPath = NULL, *backupLocationPathWildcard = NULL;
    PWSTR comRetTmpStr;
    HRESULT retval;
    int returncode = 0;

    retval = SHGetKnownFolderPath(targetLocationGuid, 0, NULL, &comRetTmpStr);
    targetLocationPath = (WCHAR*) malloc(wcslen(comRetTmpStr)*2+MALLOC_HEADROOM);    //16 wchar headroom
    wcscpy(targetLocationPath, comRetTmpStr);
    wcscat(targetLocationPath, L"\\"); 
    CoTaskMemFree(comRetTmpStr);
    
    backupLocationPath = (WCHAR*) malloc(wcslen(const_backupLocationPathNoWildcard)*2+MALLOC_HEADROOM);  //8 wchar headroom fail here smh
    wcscpy(backupLocationPath, const_backupLocationPathNoWildcard);
    wcscat(backupLocationPath, backupFolderName);
    wcscat(backupLocationPath, L"\\*");

    /*
    backupLocationPathWildcard = (WCHAR*) malloc(wcslen(const_backupLocationPathNoWildcard)*2+16);
    wcscpy(backupLocationPathWildcard, backupLocationPath);
    wcscat(backupLocationPathWildcard, L"*");
    */

    findfilehandle = FindFirstFileW(backupLocationPath, &filedata);
    if(findfilehandle == INVALID_HANDLE_VALUE) {
        printf("can't indexing file!\n");
        returncode = -1;
        goto recover_fatal_return;
    }
    backupLocationPath[wcslen(backupLocationPath)-1] = (WCHAR) 0;
    do {
        extensionName = PathFindExtensionW(filedata.cFileName);
        if(wcscmp(extensionName, targetExtension) == 0) {
            //backup
            backupFullFileName = (WCHAR*) malloc(wcslen(backupLocationPath)*2 + wcslen(filedata.cFileName)*2 +MALLOC_HEADROOM);
            wcscpy(backupFullFileName, backupLocationPath);
            // wcscat(backupFullFileName, L"\\");
            wcscat(backupFullFileName, filedata.cFileName);
            (void) writelog(backupFullFileName, "Backup path: ");
            //real path
            realFullFileName = (WCHAR*) malloc(wcslen(targetLocationPath)*2 + wcslen(filedata.cFileName)*2 +MALLOC_HEADROOM);
            wcscpy(realFullFileName, targetLocationPath);
            // wcscat(realFullFileName, L"\\");
            wcscat(realFullFileName, filedata.cFileName);
            (void) writelog(realFullFileName, "Real path: ");
            retval = CopyFileW(backupFullFileName, realFullFileName, FALSE);
            if(retval == 0) {
                (void) printf("Fail to recover backup! Is admin privileges missing?\n");
                goto recover_loop_return;
            }

            recover_loop_return:
            free(backupFullFileName);
            free(realFullFileName);
            backupFullFileName = NULL;
            realFullFileName = NULL;
        }
    } while(FindNextFileW(findfilehandle, &filedata) != 0);
    FindClose(findfilehandle);

    recover_fatal_return:
    free(backupLocationPath);
    backupLocationPath = NULL;
    free(targetLocationPath);
    targetLocationPath = NULL;
    return returncode;
}


int encrypt(WCHAR *targetBackupPath, WCHAR *iconPath, int iconIndex) {
    HRESULT retval;
    retval = proceed_desktop_shortcut(FOLDERID_PublicDesktop, targetBackupPath, L"public_bk", iconPath, iconIndex);
    retval = proceed_desktop_shortcut(FOLDERID_Desktop, targetBackupPath, L"local_bk", iconPath, iconIndex);
    retval = proceed_internet_shortcut(FOLDERID_PublicDesktop, targetBackupPath, L"public_bk", iconPath, iconIndex);
    retval = proceed_internet_shortcut(FOLDERID_Desktop, targetBackupPath, L"local_bk", iconPath, iconIndex);
    return 0;
}


int main(int argc, char *argv[]) {
    freopen("log.txt", "w", stdout);
    HRESULT retval;
    bool useShell32Icon = false;
    bool recoverMode = false;

    HANDLE iconHandle = NULL;
    DWORD iconHandleByteWritten = 0;

    retval = CoInitialize(NULL);
    if(!SUCCEEDED(retval)) {
        puts("Fail to init COM");
        return -1;
    }

    if(argc >= 2) {
        if(strcmp(argv[1], "-r") == 0) {
            recoverMode = true;
        }
        if(strcmp(argv[1], "-s") == 0) {
            //Should not be use as double-check functionality do not work, raising safety concern
            //interfere with default option, no mitigation now 
            // useShell32Icon = true;
            useShell32Icon = false;
        }
    }

    PWSTR comRetTmpStr;
    WCHAR targetBackupPath[300];
    retval = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &comRetTmpStr);
    wcscpy(targetBackupPath, comRetTmpStr);
    wcscat(targetBackupPath, L"\\i_shell_link_shortcut_backup\\");
    CoTaskMemFree(comRetTmpStr);

    if(recoverMode) {
        retval = recover(targetBackupPath, L"local_bk", FOLDERID_Desktop, L".lnk");
        retval = recover(targetBackupPath, L"local_bk", FOLDERID_Desktop, L".url");
        retval = recover(targetBackupPath, L"public_bk", FOLDERID_PublicDesktop, L".lnk");
        retval = recover(targetBackupPath, L"public_bk", FOLDERID_PublicDesktop, L".url");

        CoUninitialize();
        return 0;
    }

    if(!CreateDirectoryW(targetBackupPath, NULL)) {
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            puts("Can't create backup folder");
            return -1;
        }
    }
    SetFileAttributesW(targetBackupPath, FILE_ATTRIBUTE_HIDDEN);

    WCHAR iconPath[300];
    int iconIndex = 0;
    if(useShell32Icon) {
        wcscpy(iconPath, L"C:\\Windows\\System32\\shell32.dll");
        unsigned int shell32IconCount = ExtractIconExW(iconPath, -1, NULL, NULL, 0);
        if(shell32IconCount == UINT_MAX) {
            puts("Can't query icon in shell32");
            return -1;
        }

        srand((unsigned int) time(NULL));
        iconIndex = (int) (rand() % shell32IconCount);
    } else {
        wcscpy(iconPath, targetBackupPath);
        wcscat(iconPath, L"goicon.ico");
        iconIndex = 0;

        
        //write icon
        iconHandle = CreateFileW(iconPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                                FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL, NULL);
        if(iconHandle == INVALID_HANDLE_VALUE) {
            puts("Can't create icon");
            return -1;
        }
        if(!WriteFile(iconHandle, goicon_dump, sizeof(goicon_dump), &iconHandleByteWritten, NULL) || iconHandleByteWritten == 0) {
            puts("Can't write icon");
            return -1;
        }
        CloseHandle(iconHandle);
        
        
        
    }
    (void) encrypt(targetBackupPath, iconPath, iconIndex);

    CoUninitialize();
    return 0;
}