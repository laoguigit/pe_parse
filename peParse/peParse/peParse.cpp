// peParse.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
using namespace std;

void pause() {
    char c;
    std::cin >> c;
}

char* g_buf;

char* loadFile(string fileName) {
    FILE* file;
    if (0 != fopen_s(&file, fileName.c_str(), "rb")) {
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

#include<Windows.h>

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


void parseFile(unique_ptr<char[]> fileBuf) {
    char* bufTmp = fileBuf.get();
    IMAGE_DOS_HEADER dosHead = getDosHead(bufTmp);
    
    bufTmp += dosHead.e_lfanew;

    IMAGE_NT_HEADERS32 ntHead32 = getNtHead32(bufTmp);
    IMAGE_NT_HEADERS64 ntHead64 = getNtHead64(bufTmp);

    int numberOfSections = ntHead32.FileHeader.NumberOfSections;
    DWORD resourceRVA = 0;

    switch (ntHead32.FileHeader.Machine) {
    case IMAGE_FILE_MACHINE_I386:
        printf("x86\n");
        bufTmp += sizeof(ntHead32);
        resourceRVA = ntHead32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
        break;
    case IMAGE_FILE_MACHINE_AMD64:
        printf("x64\n");
        bufTmp += sizeof(ntHead64);
        resourceRVA = ntHead64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
        break;
    default:
        printf("error\n");
        return;
    }

    IMAGE_SECTION_HEADER sectionHeaders[16];
    getSectionHeader(bufTmp, sectionHeaders, numberOfSections);

    //解析resource
    int resSectionIndex = findSectionHeader(resourceRVA, sectionHeaders, numberOfSections);
    if (resSectionIndex < 0) {
        return;
    }


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
