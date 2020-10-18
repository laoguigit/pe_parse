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


    cout << endl<< "[head]:" << endl;
    cout << "head.e_magic:" << head.e_magic<<endl;
    cout << "head.e_lfanew:" << head.e_lfanew << endl;
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

    cout << endl << "[section Header]:" << endl;
    for (int i = 0; i < n; i++) {
        cout << "sectionHeaders["<<i<<"].Name:" << sectionHeaders[i].Name << "  |  ";
        cout << "sectionHeaders["<<i<<"].VirtualAddress:"<<sectionHeaders[i].VirtualAddress << endl;
    }
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

bool parseResourse(char* buf, DWORD resourceRVA, IMAGE_SECTION_HEADER& section, int level) {
    //1.rec directory
    IMAGE_RESOURCE_DIRECTORY direcotry;
    DWORD d = offsetPhyToVa(section);
    char* bufPhy = buf + (resourceRVA + d);
    memcpy_s(&direcotry, sizeof(direcotry), bufPhy, sizeof(direcotry));

    char* bufPhySectionBase = buf + (section.VirtualAddress + d);

    //2.多个 directory entry
    char* bufPhyEntyrBase = bufPhy + sizeof(direcotry);
    int count = direcotry.NumberOfIdEntries + direcotry.NumberOfNamedEntries;
    //cout << "count: "<< count << endl;
    for (int i = 0; i < count; i++) {
        IMAGE_RESOURCE_DIRECTORY_ENTRY entry;
        char* bufPhyEntry = bufPhyEntyrBase + i * sizeof(entry);
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

bool checkDebug(char* bufPhyFile, DWORD resourceRVA, IMAGE_SECTION_HEADER* sections, int index) {
    if (index < 0) {
        return false;
    }
    cout << endl;

    //DWORD offset = offsetPhyToVa(section);
    IMAGE_SECTION_HEADER section = sections[index];
    parseResourse(bufPhyFile, resourceRVA, section, 0);

    return true;
}

bool checkExport(char* bufPhyFile, DWORD exportSectionRVA, IMAGE_SECTION_HEADER* sections, int index) {
    if (index < 0) {
        return false;
    }
    cout << endl;

    IMAGE_SECTION_HEADER section = sections[index];

    cout << endl;
    DWORD offset = offsetPhyToVa(section);

    char* bufPhyExportDirectory = bufPhyFile + (exportSectionRVA + offset);
    _IMAGE_EXPORT_DIRECTORY *pExportDirectory = (_IMAGE_EXPORT_DIRECTORY*)bufPhyExportDirectory;
    cout << "[Export DLL: " << (bufPhyFile + (pExportDirectory->Name + offset)) << "]" <<endl;
    
    // rva, 指向func 的地址 的数组
    cout << "func count: " << pExportDirectory->NumberOfFunctions << endl;
    pExportDirectory->AddressOfFunctions;
    DWORD* bufFuncAddrS = (DWORD*)(bufPhyFile + (pExportDirectory->AddressOfFunctions + offset));
    for (int i = 0; i < pExportDirectory->NumberOfFunctions; i++, bufFuncAddrS++) {
        cout << "func addr: " << *bufFuncAddrS << endl;
    }

    // rva，指向名字的地址 的数组
    cout << "func name count: " << pExportDirectory->NumberOfNames << endl;
    pExportDirectory->AddressOfNames;
    DWORD* bufNameAddrS = (DWORD*)(bufPhyFile + (pExportDirectory->AddressOfNames + offset));
    WORD* bufOrdinalS = (WORD*)(bufPhyFile + (pExportDirectory->AddressOfNameOrdinals + offset));
    for(int i = 0;i< pExportDirectory->NumberOfNames;i++, bufNameAddrS++, bufOrdinalS++) {
        char* buf = bufPhyFile + (*bufNameAddrS + offset);
        cout << "func name: " << buf <<", ordinal: "<< *bufOrdinalS << endl;;
    }
    
    // rva，指向函数名 对应序号 的地址 的数组
    pExportDirectory->AddressOfNameOrdinals;
    
    return true;
}

bool checkImport(char* bufPhyFile, DWORD importSectionRVA, IMAGE_SECTION_HEADER * sections, int index) {
    if (index < 0) {
        return false;
    }
    cout << endl;

    IMAGE_SECTION_HEADER section = sections[index];
    DWORD offset = offsetPhyToVa(section);
    char* bufPhyImportDescriptor = bufPhyFile + importSectionRVA + offset;

    IMAGE_IMPORT_DESCRIPTOR desc;

    while (true) {
        
        //每个dll文件，有一个IMAGE_IMPORT_DESCRIPTOR。 characteristics为0的时候结束
        memcpy_s(&desc, sizeof(desc), bufPhyImportDescriptor, sizeof(desc));
        if (desc.Characteristics == 0) {
            break;
        }

        char * bufName = bufPhyFile + (desc.Name + offset);

        cout << endl<< "[import DLL name: "<< bufName << "]" << endl;
        //desc.OriginalFirstThunk 指向 导入表数组
        IMAGE_THUNK_DATA thunk;
        char* bufPhyThunk = bufPhyFile + (desc.OriginalFirstThunk + offset);

        
        //加载前, desc.OriginalFirstThunk 和 desc.FirstThunk 都指向 函数名表，
        //加载后，desc.FirstThunk 指向 函数地址表

        while (true) {
            //每个 导入函数，有一个thunk。
            memcpy_s(&thunk, sizeof(thunk), bufPhyThunk, sizeof(thunk));
            
            //IMAGE_SNAP_BY_ORDINAL判断是否按序号导入，IMAGE_ORDINAL用来获取导入序号。
            if (thunk.u1.Ordinal == 0) {
                break;
            }
            else if((thunk.u1.Ordinal & 0x80000000) != 0) {
                thunk.u1.Ordinal & 0xffff;
            }
            else {
                char* bufPhyName = bufPhyFile + (thunk.u1.AddressOfData + offset);
                IMAGE_IMPORT_BY_NAME name;
                //memcpy_s(&name, sizeof(name), bufPhyName, sizeof(name));

                //char* buf = new char[name.Hint + 1];
                //buf[name.Hint] = 0;
                //memcpy_s(buf, name.Hint, bufPhyName+2, name.Hint);
                cout << "import func: " << (bufPhyName + 2) << endl;
                
            }

            bufPhyThunk += sizeof(thunk);
        }
        bufPhyImportDescriptor += sizeof(desc);
    };// while (desc.Characteristics != 0);
    
    return true;
}
//_IMAGE_EXPORT_DIRECTORY; 

/*
#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
//      IMAGE_DIRECTORY_ENTRY_COPYRIGHT       7   // (X86 usage)
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor
*/
const char* IMAGE_DATA_ENTRY_NAME[16] = {
    "export",
    "import",
    "resource",
    "exception",
    "security",
    "basereloc",
    "debug",
    "copyright",
    "architecture",
    "globalptr",
    "tls",
    "load_config",
    "boud_import",
    "iat",
    "delay_import",
    "com_descriptor",
};



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

    cout << endl << "[IMAGE_DIRECTORY_ENTRY]" << endl;
    for (int i = 0; i < 16; i++) {
        cout << i << ": "<< IMAGE_DATA_ENTRY_NAME[i]<< ": " << ntHead32.OptionalHeader.DataDirectory[i].VirtualAddress << endl;
    }

    cout << "file_align: " << ntHead32.OptionalHeader.FileAlignment << endl;
    cout << "section align: " << ntHead32.OptionalHeader.SectionAlignment << endl;

    //节表头
    IMAGE_SECTION_HEADER sectionHeaders[16];
    getSectionHeader(bufTmp, sectionHeaders, numberOfSections);

    //解析resource
    int resSectionIndex = findSectionHeader(resourceRVA, sectionHeaders, numberOfSections);
    if (resSectionIndex < 0) {
        return;
    }

    checkDebug(fileBuf.get(), resourceRVA, sectionHeaders, resSectionIndex );
   ///
    DWORD exportSectionRVA = ntHead32.OptionalHeader.DataDirectory[0].VirtualAddress;
    int exportSectionIndex = findSectionHeader(exportSectionRVA, sectionHeaders, numberOfSections);

    checkExport(fileBuf.get(), exportSectionRVA, sectionHeaders, exportSectionIndex);

    //
    DWORD importSectionRVA = ntHead32.OptionalHeader.DataDirectory[1].VirtualAddress;
    int importSectionIndex = findSectionHeader(importSectionRVA, sectionHeaders, numberOfSections);

    checkImport(fileBuf.get(), importSectionRVA, sectionHeaders, importSectionIndex);

 
}

int main(int n, char **argv)
{
    cout << "Hello World!"<<endl;
    string sIn;
    if (n >= 2) {
        sIn = argv[1];
    }
    else {
        sIn = "C:\\Users\\ff\\source\\repos\\Dll1\\Debug\\Dll1.dll";
       //sIn = "C:\\fx\\code\\my\\pe_parse\\pe_parse\\peParse\\Debug\\peParse1.exe";
       // getline(cin, sIn, '\n');
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
