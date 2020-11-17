/**
 * elf_parser.cpp
 * 负责解析elf文件
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
// #include <android/log.h>
// #include <EGL/egl.h>
// #include <GLES/gl.h>
#include "elf32.h" // <elf.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

// #define LOG_TAG "INJECT"
// #define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
 
// FILE *fopen(const char *filename, const char *modes)
FILE* (*old_fopen) (const char* filename, const char* modes);

FILE* new_fopen(const char *filename, const char *modes) {
  printf("[+] New call fopen.\n");
  if (*(uint32_t *) old_fopen == -1) {
    printf("error.\n");
  }
  return old_fopen(filename, modes);
}

void* get_module_base(pid_t pid, const char* module_name) {
  FILE* fp;
  long addr = 0;
  char* pch;
  char filename[32];
  char line[1024];
 
  // 格式化字符串得到 "/proc/pid/maps"
  if (pid < 0) {
    snprintf(filename, sizeof(filename), "/proc/self/maps");
  } else {
    snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
  }
 
  // 打开文件/proc/pid/maps，获取指定pid进程加载的内存模块信息，每一行格式是：
  // 78f6898000-78f6899000 r--p 00000000 fd:00 335  /apex/com.android.runtime/lib64/bionic/libdl.so
  fp = fopen(filename, "r");
  if (fp != NULL) {
      // 每次一行，读取文件 /proc/pid/maps中内容
      while (fgets(line, sizeof(line), fp)) {
          // 查找指定的so模块：/data/app-lib/com.bbk.appstore-2/libvivosgmain.so
          if (strstr(line, module_name)) {
              pch = strtok(line, "-"); // 分割字符串
              addr = strtoul(pch, NULL, 16); // 字符串转长整形

              // 特殊内存地址的处理
              if (addr == 0x8000) {
                  addr = 0;
              }
              break;
          }
      }
  }
  fclose(fp);
  return (void*) addr;
}

#define LIB_PATH "/data/app-lib/com.bbk.appstore-2/libvivosgmain.so"
int hook_fopen() {

  // 【第1步】获取so库加载到进程后的基地址
  // 获取目标pid进程中"/data/app-lib/com.bbk.appstore-2/libvivosgmain.so"模块的加载地址
  void* base_addr = get_module_base(getpid(), LIB_PATH);
  printf("[+] libvivosgmain.so address = %p \n", base_addr);
  
  old_fopen = fopen; // 保存被Hook的目标函数的原始调用地址
  printf("[+] Orig fopen = %p\n", old_fopen);
  
  int fd;
  // 打开内存模块文件"/data/app-lib/com.bbk.appstore-2/libvivosgmain.so"
  fd = open(LIB_PATH, O_RDONLY);
  if (fd == -1) {
      // LOGD("error.\n");
      printf("error.\n");
      return -1;
  }
 
  //【第2步】获取elf头，并从elf头中获取SHT的偏移量、SHT中每个item大小、SHT中item个数；
  // SHT中section名称字符串表(.shstrtab)在SHT中的索引(index).
  // elf32文件的文件头结构体Elf32_Ehdr：
  // ELF目标文件格式最前部ELF文件头（ELF Header），它包含了描述了整个文件的基本属性，比如: 
  // ELF文件版本、目标机器型号、程序入口地址等。其中ELF文件与节有关的重要结构就是节表（Section 
  // Header Table），我们可以使用readelf命令来详细查看elf文件.
  Elf32_Ehdr ehdr;
  // 读取elf32格式的文件"/data/app-lib/com.bbk.appstore-2/libvivosgmain.so"的文件头信息
  read(fd, &ehdr, sizeof(Elf32_Ehdr));
 
  // elf32文件中 节区表信息结构的文件偏移
  unsigned long shdr_addr = ehdr.e_shoff; // section header table在elf文件中的偏移地址
  // elf32文件中节区表信息结构的数量，section headler table数组大小
  int shnum = ehdr.e_shnum;
  // elf32文件中每个节区表信息结构中的单个信息结构的大小（描述每个节区的信息的结构体的大小）
  int shent_size = ehdr.e_shentsize; // section header table数组中item大小

  // elf32文件"节区表"中"每个节区的名称"存放的"节区名称字符串表"，在节区表中的序号index，我的理解是：在elf文件的
  // Section Header Table部分，是一个数组，且该数组保存了各种类型的section，例如：got类型的section，同样也包
  // 括每个section名称的section，elf是通过elf头来记录section名称的section所在的索引(index)是哪个。
  unsigned long stridx = ehdr.e_shstrndx; // 读取 "节头表字符串" 所在的索引

  //【第3步】计算得到.shstrtab 这个section的内容，并遍历SHT，判断为.got.plt/.got名称的section，并获取目标函
  // 数地址，替换之，elf32文件中节区表的每个单元信息结构体（描述每个节区的信息的结构体）
  Elf32_Shdr shdr; // 相当于 section header table
  // elf32文件中定位到存放每个节区名称的字符串表的信息结构体位置.shstrtab
  lseek(fd, shdr_addr + stridx * shent_size, SEEK_SET); // 定位到节表(section header table)中字符串
  // section的地址偏移处，读取elf32文件中的描述每个节区的信息的结构体（这里是保存elf32文件的每个节区的名称字符串的）
  read(fd, &shdr, shent_size); // 读取字符串section到Elf32_Shdr中
  // 41159, size is 254
  printf("[+] String table offset is %u, size is %u", shdr.sh_offset, shdr.sh_size);

  // 为保存ELF32文件的所有的节区的名称字符串申请内存空间: .shstrtab 类型的section
  char* string_table = (char *) malloc(shdr.sh_size);
  // 定位到具体存放elf32文件的所有的"节区"的名称字符串的文件偏移处
  lseek(fd, shdr.sh_offset, SEEK_SET);
  // 从elf32内存文件中读取所有的节区的名称字符串到申请的内存空间中
  // 得到字符串表(实际上是session header table中的一个section)中存储的所有字符串，其中就包括section名
  read(fd, string_table, shdr.sh_size); 

  // 重新设置elf32文件的文件偏移为节区信息结构的起始文件偏移处，为了遍历section header table做准备
  lseek(fd, shdr_addr, SEEK_SET);

  int i;
  uint32_t out_addr = 0;
  uint32_t out_size = 0;
  uint32_t got_item = 0;
  int32_t got_found = 0;
    
  // 循环遍历 ELF32 文件的节区表（描述每个节区的信息的结构体）
  for (i = 0; i < shnum; i++) {
    // 依次读取节区表中每个描述节区的信息的结构体
    read(fd, &shdr, shent_size);
    // 判断当前节区描述结构体描述的节区是否是 SHT_PROGBITS 类型
    // 类型为SHT_PROGBITS的.got节区包含全局偏移表
    if (shdr.sh_type == SHT_PROGBITS) { // 程序自定义节头表类型
      // 获取节区(section header table)的名称字符串在保存所有节区的名称字符串段 .shstrtab 中的序号
      // section 的名字。这里其实是一个索引，指出 section 的名字存储在 .shstrtab 的什么位置。
      // .shstrtab 是一个存储所有 section 名字的字符串表。
      int name_idx = shdr.sh_name; 
      // 判断节区的名称是否为 ".got.plt" 或者 ".got" ，其中 .plt 是函数链接表；.got 是全局偏移表
      if (strcmp(&(string_table[name_idx]), ".got.plt") == 0 
          || strcmp(&(string_table[name_idx]), ".got") == 0) {

        // 获取节区 ".got" 或者 ".got.plt" 在so内存中实际数据存放地址
        out_addr = *((uint32_t*) base_addr) + shdr.sh_addr;
        // 获取节区 ".got" 或者 ".got.plt" 的大小
        out_size = shdr.sh_size;
        printf("[+] out_addr = %x, out_size = %x\n", out_addr, out_size);
        int j = 0;
        // 遍历节区 ".got" 或者 ".got.plt" 获取保存的全局的函数调用地址
        for (j = 0; j < out_size; j += 4) {
          // 获取节区".got"或者".got.plt"中的单个函数的调用地址
          got_item = out_addr + j;
          // 判断节区 ".got" 或 ".got.plt" 中函数调用地址是否为将要被Hook的目标函数地址
          if (got_item == *(uint32_t*) old_fopen) {
            // LOGD("[+] Found fopen in got.\n");
            printf("[+] Found fopen in got.\n");
            got_found = 1;
              
            uint32_t page_size = getpagesize(); // 获取当前内存分页的大小(32bit系统一般是4k)
            // 获取内存分页的起始地址（需要内存对齐）
            uint32_t entry_page_start = (out_addr + j) & (~(page_size - 1));
            printf("[+] entry_page_start = %x, page size = %x\n", entry_page_start, page_size);

            // 修改内存属性为可读可写可执行
            if (mprotect((uint32_t*) entry_page_start, page_size, 
                PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {

              printf("mprotect false.\n");
              return -1;
            }

            printf("[+] %s, old_fopen = %x, new_fopen = %x\n", "before hook function", 
                got_item, new_fopen);

            // Hook函数为我们自己定义的函数：函数地址替换
            got_item = *(uint32_t*) new_fopen;
            printf("[+] %s, old_fopen = %x, new_fopen = %x\n", "after hook function", 
                got_item, new_fopen);

            // 恢复内存属性为可读可执行
            if (mprotect((uint32_t *) entry_page_start, page_size, PROT_READ | PROT_EXEC) == -1) {
              printf("mprotect false.\n");
              return -1;
            }
                      
            break;
            // 此时，目标函数的调用地址已经被Hook了
          } else if (got_item == *(uint32_t *) new_fopen) {
            printf("[+] Already hooked.\n");
            break;
          }
        }
                
        // Hook目标函数成功，跳出循环
        if (got_found) {
          break;
        }
      }
    }
  }
  
  free(string_table);
  close(fd);
}

int hook_entry(char* a) {
  // LOGD("[+] Start hooking.\n");
  printf("Start hooking.\n");
  hook_fopen();
  return 0;
}
