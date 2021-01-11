/*-
 * Copyright (c) 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/elf32.h,v 1.8.14.2 2007/12/03 21:30:36 marius Exp $
 */

#ifndef _SYS_ELF32_H_
#define _SYS_ELF32_H_ 1

#include <stdint.h>
#include "elf_common.h"

// - .got & .got.plt分析
// .text中变化的部分放到.data中去，这样.text中剩下的就是不变的内容，就可以装载到虚拟内存的任何位置，
// 那代码段中变化的内容是什么？主要包括了对外部变量和函数的引用：
// .got     外部变量的地址是放在 .got 中的
// .got.plt 外部函数的地址是放在 .got.plt 中的
// 
// 
// - 延时绑定（PLT）
// 

/*
 * ELF definitions common to all 32-bit architectures.
 */
typedef uint32_t	Elf32_Addr;
typedef uint16_t	Elf32_Half;
typedef uint32_t	Elf32_Off;
typedef int32_t		Elf32_Sword;
typedef uint32_t	Elf32_Word;
typedef uint64_t	Elf32_Lword;

typedef Elf32_Word	Elf32_Hashelt;

/* Non-standard class-dependent datatype used for abstraction. */
typedef Elf32_Word	Elf32_Size;
typedef Elf32_Sword	Elf32_Ssize;

#define EI_NIDENT 16

/**
 * // 非常棒的ELF文件总结：
 * https://blog.csdn.net/feglass/article/details/51469511
 * https://blog.csdn.net/mergerly/article/details/94585901
 * 
 * ELF header.
 * 命令：readelf -h xxx.so，如图所示：""
 * - ELF header: 描述整个文件的组织
 * - Program Header Table: 描述文件中的各种segments，用来告诉系统如何创建进程映像的，包括：系统创建进程内存映像
 *   所需要的信息.
 * - sections(节) 或者 segments(段): segments是从运行的角度来描述elf文件，sections是从链接的角度来描述elf文件，
 *   也就是说，在链接阶段，我们可以忽略program header table来处理此文件，在运行阶段可以忽略section header table
 *   来处理此程序（所以很多加固手段删除了section header table.   从图中我们也可以看出，segments与sections是包含
 *   的关系，一个segment(段)包含若干个section(节).
 *   section包含了代码、数据、符号、字符串等各种各样必不可少的信息.
 * - Section Header Table: 包含了文件各个section的属性信息，用于描述section的表，每个section都在这里占一个表项，
 *   一般section header table是位于section后边的。如果文件用来链接的话，section header table必须存在.
 * 
 * - 首先需要知道的是，一个程序从源码到被执行，当中经历了3个过程:
 * 编译: 将.c文件编译成.o文件，不关心.o文件之间的联系
 * 静态链接: 将所有.o文件合并成一个.so或a.out文件，处理所有.o文件节区(section)在目标文件中的布局
 * 动态链接：将.so或a.out文件加载到内存，处理加载文件在的内存中的布局
 * 
 * - ELF文件有两个视图: "链接视图" 和 "执行视图"，一开始学习ELF文件格式的时候对没有整体的概念，对这两个视图很是纠结，
 *   了解了整个文件结构以后再看这两个视图就容易理解了。初步了解阶段就先简单粗暴的记住：ELF文件执行可以没有
 *   Section Header Table，但是必须有Program Header Table，链接时（包括obj链接和动态库链接）必须有
 *   Section Header Table.
 * 
 * 通常将Section称为节，将Program Header称为Segment（段）.
 * Elf执行不需要section header table但必须要program header table，我尝试将所有的section header设置为0，
 * 程序依然可以执行.
 * Elf链接不需要Program Header Table，但必须要section header table，因为可重定位文件（obj文件）中没有
 * program header table，但是有section header table，并且一个动态库so文件如果将section header table
 * 全部设置为0的话，同样链接不成功，因为链接过程中查找符号是依赖section查找的.
 * 这也就是为什么elf文件图中，链接视图中program header table是optional（可选的），而执行视图中
 * section header table是optional（可选的）.
 * 
 * 程序头表（Program Header Table），如果存在的话，告诉系统如何创建进程映像。
 * 节区头表（Section Header Table）包含了描述文件节区的信息，比如大小、偏移等。
 * 
 * 通过执行命令 "readelf -S android_server" 来查看该可执行文件中有哪些section
 * 执行命令 "readelf –segments android_server"，可以查看该文件的执行视图
 * 
 * - 那么为什么需要区分两种不同视图？
 * 当ELF文件被加载到内存中后，系统会将多个具有相同权限（flg值）section合并一个segment。操作系统往往以页为基本单位
 * 来管理内存分配，一般页的大小为 4KB 大小。同时，内存的权限管理的粒度也是以页为单位，页内的内存是具有同样的权限等属
 * 性，并且操作系统对内存的管理往往追求高效和高利用率这样的目标。ELF文件在被映射时，是以系统的页长度为单位的，那么每
 * 个section在映射时的长度都是系统页长度的整数倍，如果section的长度不是其整数倍，则导致多余部分也将占用一个页。而
 * 我们从上面的例子中知道，一个ELF文件具有很多的section，那么会导致内存浪费严重。这样可以减少页面内部的碎片，节省了
 * 空间，显著提高内存利用率。
 * 
 * - 在ELF Header中我们需要重点关注以下几个字段:
 * 1. e_entry 表示程序入口地址
 * 2. e_ehsize ELF Header结构大小
 * 3. e_phoff、e_phentsize、e_phnum：描述Program Header Table的偏移、大小、结构
 * 4. e_shoff、e_shentsize、e_shnum：描述Section Header Table的偏移、大小、结构
 * 5. e_shstrndx 这一项描述的是字符串表在Section Header Table中的索引，值25表示的是
 *    Section Header Table中第25项是字符串表（String Table）
 * 6. 编译后比较固定的字段: e_ident、e_machine、e_version、e_entry、e_flags、e_ehsize
 * 7. 目前e_ehsize=52字节，e_shentsize=40字节，e_phentsize=32字节，这些值都是固定值，某些加固
 *    会修改这些值造成静态解析失败，可以修改回这些固定值.
 * 8. 整个so的大小 = e_shoff + e_shnum * sizeof(e_shentsize) + 1，因为节头表位于Elf文件的末尾一项.
 * 9. e_shstrndx一般等于e_shnum - 1
 * 10. e_phoff = ELF头的大小，因为程序头表一栏位于ELF头的后面一项

 * 我们关注的一些重要的Section(节)是：符号表(.dynsym)、重定位表(rel.got/rel.plt等)、GOT表(.got)等
 * 1. .dynsym 符号表
 * 记录了文件中所有符号，所谓符号就是经过修饰了得函数名或者变量名，不同编译器有不同的规则，例如：
 * _ZL15global_static_a，就是global_static_a变量经过修饰而来。
 * 2. 字符串表 。dynstr
 * 存放着所有符号的名称字符串
 * 3. 重定位表
 * 首先先理解 “重定位” 的概念，程序从代码到可执行文件的这个过程中，要经历编译器、汇编器和链接器(Linker)对代码的处理，
 * 然而编译器和汇编器通常为每个文件创建程序地址是从0开始的目标代码，但是几乎没有计算机会允许从地址0加载你的程序. 如果
 * 一个程序是由多个子程序组成的，那么所有的子程序必须加载到不同互不重叠的地址上。 “重定位” 就是为程序不同部分分配加载
 * 地址，调整程序中数据和代码以反映所分配地址的过程，简而言之：把程序中的各个部分映射到合理的地址上来。再换句话说，重定
 * 位就是把符号引用和符号定义进行连接的过程.
 */
typedef struct {
	// ELF 标识，是一个 16 字节大小的数组，其各个索引位置的字节数据有固定的含义
	//【16字节】ident ”身份“的意思，elf文件的最开始16字节：
	// 前4个字节(e_ident[EI_MAG0] ~ e_ident[EI_MAG3]) 的内容固定为 0x7f、’E’、’L’、’F’，标识这是一个ELF文件，
	// 魔数，可以直接将文件缓冲区指针强转为 unsigned int 与宏定义 #define ELFMAG "\177ELF" 判断是否相等
    // 第5字节(e_ident[EI_CLASS]) 指明文件类别：0（无效目标文件）；1（32 位目标文件）；2（64 位目标文件）
    // 第6字节(e_ident[EI_DATA]) 指明字节序，规定该文件是大端还是小端：0（无效编码格式）；1（小端）；2（大端）
    // 第7字节(e_ident[EI_VERSION]) 指明 ELF 文件头的版本。
    // 第8字节~第16字节(从 e_ident[EI_PAD] 到 e_ident[EI_NIDENT-1]) 之间的 9 个字节保留。
	unsigned char e_ident[EI_NIDENT];	/* File identification. */ 

  //【2字节】文件类型：常见的:
	// 1（可重定位（Relocatable）文件：“.o 文件”），不需要执行，因此e_entry字段为0，且没有Program Header Table等执行视图
	// 2（可执行文件）；
	// 3（共享库文件：“.so 文件”）
	// 不同类型的ELF文件的Section也有较大区别，比如只有Relocatable File有.strtab节
	Elf32_Half	e_type;		/* File type: 可定位文件.o；so库；可执行文件 */
	/* Machine architecture：elf文件适用的处理器体系结构，例如：0(未知体系结构)、3(EM_386，Intel体系结构) */
	Elf32_Half	e_machine;	
	Elf32_Word	e_version;	/* ELF format version. */

	// 程序入口点的虚拟地址，也就是OEP，（此处需要注意，当elf文件的e_type为ET_EXEC时，这里的e_entry存放的是
	// 虚拟地址VA，如果e_type是ET_DYN时（不仅仅动态库，可执行文件也会是ET_DYN，即开启了随机基址的可执行程序），
	// 那这里存放的就是RVA，加载到内存中后还要加上imagebase; .o文件(重定向文件)为0x0，因为.o文件是面向链接视图的，
	// 无需加载到进程中，因此为0x0；
	// 所谓程序进入点是指当程序真正执行起来的时候，其第1条要运行的指令的运行时地址.
	Elf32_Addr	e_entry;	/* Entry point: 指明程序入口的虚拟地址，是指该elf文件被加载到进程空间后，入口程序
							   在进程地址空间里的地址，对于可执行程序文件来说，当ELF文件完成加载之后，程序将从这
							   里开始运行;而对于其它文件来说，这个值应该是 0 */
							   
	// 此字段指明程序头表(program header table)开始处在文件中的偏移量。如果没有程序头表，该值应设为0
	Elf32_Off	e_phoff;	/* Program header file offset. 程序头表偏移量*/
	// 此字段指明节头表(section header table)开始处在文件中的偏移量。如果没有节头表，该值应设为0
	Elf32_Off	e_shoff;	/* Section header file offset. 节头表偏移量*/ // section header table偏移量
	
	// 此字段含有处理器特定的标志位。标志的名字符合”EF_machine_flag”的格式。对于Intel架构的处理器来说，
	// 它没有定义任何标志位，所以 e_flags 应该为 0。
	Elf32_Word	e_flags;	/* Architecture-specific flags. */

	// 此字段表明 ELF Header 的大小，以字节为单位
	Elf32_Half	e_ehsize;	/* Size of ELF header in bytes. elf文件头大小*/
	// 此字段表明在程序头表中每一个表项的大小，以字节为单位
	Elf32_Half	e_phentsize;	/* Size of program header entry. 程序头表中item大小*/
	// 此字段表明程序头表中总共有多少个表项。如果一个目标文件中没有程序头表，该值应设为0
	Elf32_Half	e_phnum;	/* Number of program header entries. 程序头表大小*/
	// 此字段表明在节头表中每一个表项的大小，以字节为单位
	/* Size of section header entry. 节头表大小*/ // section header table中item字节大小
	Elf32_Half	e_shentsize;	
	// 此字段表明节头表中总共有多少个表项。如果一个目标文件中没有节头表， 该值应设为0
	/* Number of section header entries. 节头表中item个数*/ // section header table数组大小
	Elf32_Half	e_shnum;	

    // 很特殊的一个字段：节头字符串表在节表中的索引位置
	// 节头名字符串表(实际上也是一个section)在 section header table 中的索引，用于定位每一个section名称
	// 的字符串表，获取section名字符串表的section之后，再遍历section header table，根据 
	// Elf32_Ehdr->e_shentsize大小读取到Elf32_Shdr中，so库基地址 + Elf32_Shdr->sh_addr 即是该section
	// 的偏移量，从这个偏移量开始，每4字节步进值的去遍历。节头表中与节名字表相对应的表项的索引。如果文件没有节名
	// 字表，此值应设置为 SHN_UNDEF
	Elf32_Half	e_shstrndx;	/* Section name strings section：节头表中所有节名字符串表 的索引(index) */
} Elf32_Ehdr; // elf文件头

/**
 * Section header.
 * 对应的命令是："readelf -S xxx.so"，如图："so中有哪些section.jpg"
 * 一般位于整个elf文件的末尾.
 * 一个Segment包含了>=1个Section.
 */
typedef struct {
	// 节区名，是节区头部字符串表节区（Section Header String Table Section）的索引。
	// 名字是一个 NULL 结尾的字符串.
	// section的名字，这里其实是一个索引(index/key)，指出section的名字存储在 .shstrtab(节头名符串
	// 表)的什么位置，.shstrtab 是一个存储所有section名、在ELF文件中的偏移量等信息的字符串表.
	// 对应(readelf -S x.so)第一列的 Name 字段，有：.got、.rel.plt、plt等
	Elf32_Word	sh_name; /*Section name (index into the section header string table)*/
	// 对应第2列：Type字段，有：PROGBITS、HASH、REL等
	Elf32_Word	sh_type;	/* Section type. */
	// 对应最后一列: eg: R/RW/W
	Elf32_Word	sh_flags;	/* Section flags. */

	// 如果节将出现在进程的内存映像中，此成员是该节的偏移量，实际地址为:
	// 基地址(/proc/pid/maps)+sh_addr，否则，此字段为0.
	// 加载到内存后的虚拟内存相对地址，相对于该so库文件加载到进程中的基地址而言的.
	// 如果此section需要映射(加载)到进程空间(例如：.got/.plt)，此成员是指定映射的起始地址。
	// 如不需映射，此值为0，这个字段是面向进程地址空间的，即该节的真实地址是: base_addr+sh_addr
	Elf32_Addr	sh_addr; /* Address in memory image. */ 
	// 此sectionn在文件中的偏移，这个字段是相对elf文件本身的，即面向文件的。
	Elf32_Off	sh_offset;	/* Offset in file. */
	// 此 section 的字节大小。如果 section 类型为 SHT_NOBITS，就不用管 sh_size 了。
	Elf32_Word	sh_size;	/* Size in bytes. */ 
	Elf32_Word	sh_link;	/* Index of a related section. */
	Elf32_Word	sh_info;	/* Depends on section type. */
	Elf32_Word	sh_addralign;	/* Alignment in bytes. */
	Elf32_Word	sh_entsize;	/* Size of each entry in section. */
} Elf32_Shdr; // section header table中的一个item: 节头

/**
 * Program header.
 * Elf文件被加载映射后的运行时视图，每个程序头(item)其实都是segment(段).
 * 该结构体中的字段对应，执行命令 "readelf –segments xxx.so"，可以查看该文件的执行视图
 * 中的字段，例如：“so的执行视图-Segments.jpg”，生成的信息，除了包括如下struct中的字段外，
 * 还包括了 Section to Segment mapping信息(Segment可能包括>=1个section)，可以看出：
 * segment是section的一个集合，sections按照一定规则映射到segment.
 * 一个Segment包含了>=1个Section.
 * 
 * “程序头部” 描述与程序执行直接相关的目标文件结构信息。用来在文件中定位各个Segment(段)的映像，
 * 同时包含其他一些用来为程序创建映像所必须的信息.
 * 
 * 只对 “可执行文件” 和 “so文件” 有意义，每个结构描述了一个段或者系统准备程序执行所必须的其他信息，
 * so文件的Segment包含一个或多个Section。
 */
typedef struct {
	// 对应第1个字段：segment的类型，有：PHDR、INTERP、LOAD等
	Elf32_Word	p_type;		/* Entry type. */
	Elf32_Off	p_offset;	/* File offset of contents. */
	Elf32_Addr	p_vaddr;	/* Virtual address in memory image. */
	Elf32_Addr	p_paddr;	/* Physical address (not used). */
	Elf32_Word	p_filesz;	/* Size of contents in file. */
	Elf32_Word	p_memsz;	/* Size of contents in memory. */
	Elf32_Word	p_flags;	/* Access permission flags. eg: R/RW/W */
	Elf32_Word	p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

/**
 * 动态连接器(段segment)，保存了动态链接器所需要的基本信息
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 * 属于运行视图，包含了elf中其他各个section的内存位置等信息
 */
typedef struct {
	// d_tag=DT_SYMTAB 动态链接符号表.dynsym的地址,    由d_ptr指向
	// d_tag=DT_STRTAB 动态链接字符串.dynstr的地址,    由d_ptr指向
	// d_tag=DT_STRSZ  动态链接字符串大小.dynsym的地址  由d_val表示
	// d_tag=DT_HASH   动态链接hash表地址             由d_ptr指向
	// ......
	Elf32_Sword	d_tag;		/* Entry type. */
	union {
		Elf32_Word	d_val;	/* Integer value. */
		Elf32_Addr	d_ptr;	/* Address value. */
	} d_un;
} Elf32_Dyn;

/**
 * 重定位段
 * Relocation entries.
 * Relocations that don't need an addend field.
 * .rel.data 数据段的重定位表
 * .rel.text 代码段的重定位表
 * .rel.plt  对函数引用的修正，它所修正的位置位于.got.plt中
 * .rel.dyn  对数据引用的修正，它所修正的位置位于.got及数据段
 */
typedef struct {
	// 指向全局符号表中的某个项，也就是全局符号表中某项的地址，地址中的值是真实的函数地址，
	// 由动态链接器(linker)给出.
	Elf32_Addr	r_offset;	/* Location to be relocated. */
	// 要重定位的符号表引用，以及实施重定位类型(哪些bit需要修改，以及如何计算他们的取值)，其中，.rel.dyn
	// 重定位类型一般是 R_386_GLOB_DAT 和 R_386_COPY；.rel.plt 为 R_386_JUMP_SLOT.
	// 对 r_info 字段使用 ELF32_R_TYPE 宏运算可得到重定位类型，使用 ELF32_R_SYM 可得到符号在符号表中
	// 的索引值.
	Elf32_Word	r_info;		/* Relocation type and symbol index. */
} Elf32_Rel;

/**
 * Relocations that need an addend(附录、加数) field.
 */
typedef struct {
	Elf32_Addr	r_offset;	/* Location to be relocated. */
	Elf32_Word	r_info;		/* Relocation type and symbol index. */
	Elf32_Sword	r_addend;	/* Addend. */
} Elf32_Rela;

/** 
 * Macros for accessing the fields of r_info. 
 */
#define ELF32_R_SYM(info)	((info) >> 8)
#define ELF32_R_TYPE(info)	((unsigned char)(info))

/**
 * Macro for constructing r_info from field values. 
 */
#define ELF32_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

/**
 *	Note entry header
 */
typedef Elf_Note Elf32_Nhdr;

/**
 *	Move entry
 */
typedef struct {
	Elf32_Lword	m_value;	/* symbol value */
	Elf32_Word 	m_info;		/* size + index */
	Elf32_Word	m_poffset;	/* symbol offset */
	Elf32_Half	m_repeat;	/* repeat count */
	Elf32_Half	m_stride;	/* stride info */
} Elf32_Move;

/**
 *	The macros compose and decompose values for Move.r_info
 *
 *	sym = ELF32_M_SYM(M.m_info)
 *	size = ELF32_M_SIZE(M.m_info)
 *	M.m_info = ELF32_M_INFO(sym, size)
 */
#define	ELF32_M_SYM(info)	((info)>>8)
#define	ELF32_M_SIZE(info)	((unsigned char)(info))
#define	ELF32_M_INFO(sym, size)	(((sym)<<8)+(unsigned char)(size))

/**
 * Hardware/Software capabilities entry
 * 软硬件能力入口
 */
typedef struct {
	Elf32_Word	c_tag;		/* how to interpret value */
	union {
		Elf32_Word c_val;
		Elf32_Addr c_ptr;
	} c_un;
} Elf32_Cap;

/**
 * 符号表(Section)
 * Symbol table entries.
 * 对应于elf文件中的.dynsym，命令：readelf -p 28 a.out
 */
typedef struct {
	// 符号的名字，但并不是一个字符串，而是一个指向字符串表的索引值(index)，
	// 在字符串表(Section)中对应位置上的字符串就是该符号名字的实际文本.
	Elf32_Word st_name;	/* String table index of name. */
	
	// 符号的值。这个值其实没有固定的类型，它可能代表一个数值，也可以是一个地址，具体是什么要看上下文:
	// 对于不同的目标文件类型，符号表项的 st_value 的含义略有不同: 
	// • 在重定位文件中，如果一个符号对应的节的索引值是SHN_COMMON，st_value 值是这个节内容的字节对齐数。
	// • 在重定位文件中，如果一个符号是已定义的，那么它的st_value值 是该符号的起始地址在其所在节中的偏移量，
	//   而其所在的节的索引由 st_shndx 给出。
	// • 在可执行文件和共享库文件(so)中，st_value不再是一个节内的偏移量，而是一个虚拟地址，直接指向符号所
	//   在的内存位置。这种情况下，st_shndx 就不再需要了.
	Elf32_Addr st_value;	/* Symbol value. */
	// 符号的大小。各种符号的大小各不相同，比如一个对象的大小就是它实际占用的字节数。如果一个符号的大小为 0 或
	// 者大小未知，则这个值为 0
	Elf32_Word st_size;	/* Size of associated object. */
	unsigned char st_info;	/* Type and binding information. */
	unsigned char st_other;	/* Reserved (not used). */
	Elf32_Half	st_shndx;	/* Section index of symbol. */
} Elf32_Sym; // 符号表

/**
 * Macros for accessing the fields of st_info. 
 */
#define ELF32_ST_BIND(info)		((info) >> 4)
#define ELF32_ST_TYPE(info)		((info) & 0xf)

/**
 * Macro for constructing st_info from field values. 
 */
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/**
 * Macro for accessing the fields of st_other. 
 */
#define ELF32_ST_VISIBILITY(oth)	((oth) & 0x3)

/**
 * Structures used by Sun & GNU symbol versioning. 
 */
typedef struct {
	Elf32_Half	vd_version;
	Elf32_Half	vd_flags;
	Elf32_Half	vd_ndx;
	Elf32_Half	vd_cnt;
	Elf32_Word	vd_hash;
	Elf32_Word	vd_aux;
	Elf32_Word	vd_next;
} Elf32_Verdef;

typedef struct {
	Elf32_Word	vda_name;
	Elf32_Word	vda_next;
} Elf32_Verdaux;

typedef struct {
	Elf32_Half	vn_version;
	Elf32_Half	vn_cnt;
	Elf32_Word	vn_file;
	Elf32_Word	vn_aux;
	Elf32_Word	vn_next;
} Elf32_Verneed;

typedef struct {
	Elf32_Word	vna_hash;
	Elf32_Half	vna_flags;
	Elf32_Half	vna_other;
	Elf32_Word	vna_name;
	Elf32_Word	vna_next;
} Elf32_Vernaux;

typedef Elf32_Half Elf32_Versym;

typedef struct {
	Elf32_Half	si_boundto;	/* direct bindings - symbol bound to */
	Elf32_Half	si_flags;	/* per symbol flags */
} Elf32_Syminfo;

#endif /* !_SYS_ELF32_H_ */
