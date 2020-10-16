// peParse.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include<Windows.h>
#include <iostream>
#include <string>
using namespace std;

void pause() {
    char c;
    std::cin >> c;
}


char* loadFile(string fileName) {
    FILE* file;
    if (0 != fopen_s(&file, fileName.c_str(), "rb") || file ==0) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long lSize = ftell(file);

    char* buf = new char[lSize];
    memset(buf, 0, lSize);

    fseek(file, 0, SEEK_SET);
    if (0 >= fread_s(buf, lSize, lSize, 1, file)) {
        fclose(file);
        return 0;
    }

    return buf;
}


IMAGE_DOS_HEADER getDosHead(char* buf) {
    IMAGE_DOS_HEADER head;
    memcpy_s(&head, sizeof(head), buf, sizeof(head));
    return head;
}

IMAGE_NT_HEADERS32 getNtHead32(char *buf) {
    IMAGE_NT_HEADERS32 head;
    memcpy_s(&head, sizeof(head), buf, sizeof(head));
    return head;
}

IMAGE_NT_HEADERS64 getNtHead64(char* buf) {
    IMAGE_NT_HEADERS64 head;
    memcpy_s(&head, sizeof(head), buf, sizeof(head));
    return head;
}

void getSectionHeader(char *bufTmp, IMAGE_SECTION_HEADER sectionHeaders[16], int n)
{
    memcpy_s(sectionHeaders, sizeof(sectionHeaders[0]) * 16, bufTmp, sizeof(sectionHeaders[0]) * 16);
}


int findSectionHeader(DWORD rva, IMAGE_SECTION_HEADER sectionHeaders[16], int n) {
    for (int i = 0; i < n; i++) {
        if (rva >= sectionHeaders[i].VirtualAddress && rva < (sectionHeaders[i].VirtualAddress + sectionHeaders[i].Misc.VirtualSize)) {
            return i;
        }
    }
    return -1;
}

bool checkX64(IMAGE_NT_HEADERS32 &ntHead32) {
    /*switch (ntHead32.FileHeader.Machine) {
    case IMAGE_FILE_MACHINE_I386:
        printf("x86\n");
        return false;
    case IMAGE_FILE_MACHINE_AMD64:
        printf("x64\n");
        return true;
    default:
        printf("error\n");
        return false;
    }*/

 /*   ntHead32.OptionalHeader.Magic == 0x010B;
    ntHead32.OptionalHeader.Magic == 0x020B;*/

    if (0 != (ntHead32.FileHeader.Characteristics & IMAGE_FILE_32BIT_MACHINE)) {
        printf("x86\n");
        return false;
    }
    else {
        printf("x64\n");
        return true;
    }
}

DWORD offsetPhyToVa(IMAGE_SECTION_HEADER& section) {
    section.PointerToRawData;
    section.VirtualAddress;
    return section.PointerToRawData - section.VirtualAddress;
}

wstring getResEntryName(char*buf) {
    IMAGE_RESOURCE_DIRECTORY_STRING name;
    
    memcpy_s(&name, sizeof(name), buf, sizeof(name));

    wchar_t* p = new wchar_t[name.Length+1];
    p[name.Length] = 0;
    memcpy_s(p, name.Length * sizeof(wchar_t), buf + sizeof(name.Length), name.Length * sizeof(wchar_t));
    
    wstring s =p ;
    delete[]p;
    p = NULL;
    return s;
}

bool g_bVersion = false;

struct VS_VERSIONINFO {
    USHORT len;
    USHORT valueLen;
    USHORT type;
    WCHAR key[17];
    VS_FIXEDFILEINFO value;
};
void parseVersion(char* buf) {
    VS_VERSIONINFO version;
    memcpy_s(&version, sizeof(version), buf, sizeof(version));
    if (0 != (version.value.dwFileFlags & VS_FF_DEBUG)) {
        cout << "Debug\n";
    }
    else {
        cout << "Release\n";
    }
}

bool parseResourse(char*buf, DWORD resourceRVA, IMAGE_SECTION_HEADER& section, int level) {
    //1.rec directory
    IMAGE_RESOURCE_DIRECTORY direcotry;
    DWORD d = offsetPhyToVa(section);
    char *bufPhy = buf + (resourceRVA + d);
    memcpy_s(&direcotry, sizeof(direcotry), bufPhy, sizeof(direcotry));

    char* bufPhySectionBase = buf + (section.VirtualAddress + d);

    //2.多个 directory entry
    char * bufPhyEntyrBase = bufPhy + sizeof(direcotry);
    int count = direcotry.NumberOfIdEntries + direcotry.NumberOfNamedEntries;
    //cout << "count: "<< count << endl;
    for (int i = 0; i < count; i++) {
        IMAGE_RESOURCE_DIRECTORY_ENTRY entry;
        char *bufPhyEntry = bufPhyEntyrBase + i*sizeof(entry);
        memcpy_s(&entry, sizeof(entry), bufPhyEntry, sizeof(entry));

        //打印
        for (int i = 0; i <= level; i++) {
            //cout << "--";
        }
        if (entry.NameIsString == 1) {
            //wcout << L"name:"<< getResEntryName(bufPhySectionBase + entry.NameOffset) << endl;
        }
        else {
            //cout << "id: " << entry.Id << endl;
            
        }

        if (1 == entry.DataIsDirectory) {
            if (level == 0 && entry.Id == 16) {
                g_bVersion = true;
            }
            parseResourse(buf, section.VirtualAddress + entry.OffsetToDirectory, section, level + 1);
            if (level == 0 && entry.Id == 16) {
                g_bVersion = false;
            }
        }
        else {
            IMAGE_RESOURCE_DATA_ENTRY dataEntry;
            memcpy_s(&dataEntry, sizeof(dataEntry), bufPhySectionBase + entry.OffsetToData, sizeof(dataEntry));
            

            if (g_bVersion == true) {
                parseVersion(buf + entry.OffsetToData);
            }
        }
    }
    return true;
}

bool checkDebug(char* buf, DWORD resourceRVA, IMAGE_SECTION_HEADER &section) {

    DWORD offset = offsetPhyToVa(section);

    parseResourse(buf,resourceRVA, section, 0);

    return true;
}

void parseFile(unique_ptr<char[]> fileBuf) {
    char* bufTmp = fileBuf.get();
    IMAGE_DOS_HEADER dosHead = getDosHead(bufTmp);
    
    bufTmp += dosHead.e_lfanew;

    IMAGE_NT_HEADERS32 ntHead32 = getNtHead32(bufTmp);
    IMAGE_NT_HEADERS64 ntHead64 = getNtHead64(bufTmp);

    int numberOfSections = ntHead32.FileHeader.NumberOfSections;
    DWORD resourceRVA = 0;

    bool bX64 = checkX64(ntHead32);
    if (false == bX64) {
        bufTmp += sizeof(ntHead32);
        resourceRVA = ntHead32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
    }
    else { 
        bufTmp += sizeof(ntHead64);
        resourceRVA = ntHead64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
    }



    IMAGE_SECTION_HEADER sectionHeaders[16];
    getSectionHeader(bufTmp, sectionHeaders, numberOfSections);

    //解析resource
    int resSectionIndex = findSectionHeader(resourceRVA, sectionHeaders, numberOfSections);
    if (resSectionIndex < 0) {
        return;
    }

    checkDebug(fileBuf.get(), resourceRVA, sectionHeaders[resSectionIndex] );
}

int main(int n, char **argv)
{
    std::cout << "Hello World!\n";
    string sIn;
    if (n >= 2) {
        sIn = argv[1];
    }
    else {
       // std::cin >> sIn;
        getline(cin, sIn, '\n');
    }

    std::cout << sIn << std::endl;

    char* buf = loadFile(sIn);
    if (buf == 0) {
        cout << "load file failed" << endl;
        return 0;
    }

    unique_ptr<char[]> fileBuf(buf);

    parseFile(move(fileBuf));

    pause();

    return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
