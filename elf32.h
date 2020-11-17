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

/*
 * ELF header.
 * readelf -h xxx.so
 */
typedef struct {
	// ELF 标识，是一个 16 字节大小的数组，其各个索引位置的字节数据有固定的含义
	//【16字节】ident ”身份“的意思，elf文件的最开始16字节：
	// 前4个字节(e_ident[EI_MAG0] ~ e_ident[EI_MAG3]) 的内容固定为 0x7f、’E’、’L’、’F’，标识这是一个ELF文件
    // 第5字节(e_ident[EI_CLASS]) 指明文件类别：0（无效目标文件）；1（32 位目标文件）；2（64 位目标文件）
    // 第6字节(e_ident[EI_DATA]) 指明字节序，规定该文件是大端还是小端：0（无效编码格式）；1（小端）；2（大端）
    // 第7字节(e_ident[EI_VERSION]) 指明 ELF 文件头的版本。
    // 第8字节~第16字节(从 e_ident[EI_PAD] 到 e_ident[EI_NIDENT-1]) 之间的 9 个字节保留。
	unsigned char e_ident[EI_NIDENT];	/* File identification. */ 

   	//【2字节】文件类型：常见的：1（可重定位文件：“.o 文件”）；2（可执行文件）；3（共享库文件：“.so 文件”）
	Elf32_Half	e_type;		/* File type: 可定位文件.o；so库；可执行文件 */
	/* Machine architecture：elf文件适用的处理器体系结构，例如：0(未知体系结构)、3(EM_386，Intel体系结构) */
	Elf32_Half	e_machine;	
	Elf32_Word	e_version;	/* ELF format version. */

	Elf32_Addr	e_entry;	/* Entry point: 指明程序入口的虚拟地址，是指该elf文件被加载到进程控件后，入口程序
							   在进程地址空间里的地址，对于可执行程序文件来说，当ELF文件完成加载之后，程序将从这
							   里开始运行;而对于其它文件来说，这个值应该是 0 */
							   
	// 此字段指明程序头表(program header table)开始处在文件中的偏移量。如果没有程序头表，该值应设为0
	Elf32_Off	e_phoff;	/* Program header file offset. 程序头表偏移量*/
	// 此字段指明节头表(section header table)开始处在文件中的偏移量。如果没有节头表，该值应设为0
	Elf32_Off	e_shoff;	/* Section header file offset. 节头表偏移量*/ // section header table偏移量
	
	// 此字段含有处理器特定的标志位。标志的名字符合”EF_machine_flag”的格式。对于Intel架构的处理器来说，
	// 它没有定义任何标志位，所以 e_flags 应该为 0。
	Elf32_Word	e_flags;	/* Architecture-specific flags. */

	// 此字段表明 ELF 文件头的大小，以字节为单位
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

	// 节头名字符串表(实际上也是一个section)在 section header table 中的索引，用于定位每一个section名称
	// 的字符串表，获取section名字符串表的section之后，再遍历section header table，根据 
	// Elf32_Ehdr->e_shentsize大小读取到Elf32_Shdr中，so库基地址 + Elf32_Shdr->sh_addr 即是该section
	// 的偏移量，从这个偏移量开始，每4字节步进值的去遍历。节头表中与节名字表相对应的表项的索引。如果文件没有节名
	// 字表，此值应设置为 SHN_UNDEF
	Elf32_Half	e_shstrndx;	/* Section name strings section：节头表中所有节名字符串表 的索引(index) */
} Elf32_Ehdr; // elf文件头

/*
 * Section header.
 * readelf -S xxx.so
 */
typedef struct {
	// section的名字，这里其实是一个索引(index)，指出section的名字存储在 .shstrtab(节头名符串表) 的什么位置.
	// .shstrtab是一个存储所有section名、在ELF文件中的偏移量等信息的字符串表
	Elf32_Word	sh_name;	/* Section name (index into the section header string table). */
	Elf32_Word	sh_type;	/* Section type. */
	Elf32_Word	sh_flags;	/* Section flags. */

	// 加载到内存后的虚拟内存相对地址，相对于该so库文件加载到进程中的基地址而言的
	// 如果此section需要映射(加载)到进程空间(例如：.got/.plt)，此成员指定映射的起始地址。如不需映射，此值为0
	Elf32_Addr	sh_addr;	/* Address in memory image. */ 
	Elf32_Off	sh_offset;	/* Offset in file. */
	// 此 section 的字节大小。如果 section 类型为 SHT_NOBITS，就不用管 sh_size 了。
	Elf32_Word	sh_size;	/* Size in bytes. */ 
	Elf32_Word	sh_link;	/* Index of a related section. */
	Elf32_Word	sh_info;	/* Depends on section type. */
	Elf32_Word	sh_addralign;	/* Alignment in bytes. */
	Elf32_Word	sh_entsize;	/* Size of each entry in section. */
} Elf32_Shdr; // section header table中的一个item: 节头

/*
 * Program header.
 * elf文件被加载映射后的运行时视图
 */
typedef struct {
	Elf32_Word	p_type;		/* Entry type. */
	Elf32_Off	p_offset;	/* File offset of contents. */
	Elf32_Addr	p_vaddr;	/* Virtual address in memory image. */
	Elf32_Addr	p_paddr;	/* Physical address (not used). */
	Elf32_Word	p_filesz;	/* Size of contents in file. */
	Elf32_Word	p_memsz;	/* Size of contents in memory. */
	Elf32_Word	p_flags;	/* Access permission flags. */
	Elf32_Word	p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 * 属于运行视图，包含了elf中其他各个section的内存位置等信息
 */
typedef struct {
	Elf32_Sword	d_tag;		/* Entry type. */
	union {
		Elf32_Word	d_val;	/* Integer value. */
		Elf32_Addr	d_ptr;	/* Address value. */
	} d_un;
} Elf32_Dyn;

/*
 * Relocation entries.
 * Relocations that don't need an addend field.
 * 重定位入口
 */
typedef struct {
	Elf32_Addr	r_offset;	/* Location to be relocated. */
	Elf32_Word	r_info;		/* Relocation type and symbol index. */
} Elf32_Rel;

/**
 * Relocations that need an addend field. 
 */
typedef struct {
	Elf32_Addr	r_offset;	/* Location to be relocated. */
	Elf32_Word	r_info;		/* Relocation type and symbol index. */
	Elf32_Sword	r_addend;	/* Addend. */
} Elf32_Rela;

/* Macros for accessing the fields of r_info. */
#define ELF32_R_SYM(info)	((info) >> 8)
#define ELF32_R_TYPE(info)	((unsigned char)(info))

/* Macro for constructing r_info from field values. */
#define ELF32_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

/*
 *	Note entry header
 */
typedef Elf_Note Elf32_Nhdr;

/*
 *	Move entry
 */
typedef struct {
	Elf32_Lword	m_value;	/* symbol value */
	Elf32_Word 	m_info;		/* size + index */
	Elf32_Word	m_poffset;	/* symbol offset */
	Elf32_Half	m_repeat;	/* repeat count */
	Elf32_Half	m_stride;	/* stride info */
} Elf32_Move;

/*
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
		Elf32_Word	c_val;
		Elf32_Addr	c_ptr;
	} c_un;
} Elf32_Cap;

/*
 * Symbol table entries.
 * 对应于elf文件中的.dynsym
 */
typedef struct {
	// 符号的名字，但并不是一个字符串，而是一个指向字符串表的索引值(index)，在字符串表中对应位置上的字符串就是
	// 该符号名字的实际文本
	Elf32_Word	st_name;	/* String table index of name. */
	// 符号的值。这个值其实没有固定的类型，它可能代表一个数值，也可以是一个地址，具体是什么要看上下文:
	// 对于不同的目标文件类型，符号表项的 st_value 的含义略有不同: 
	// • 在重定位文件中，如果一个符号对应的节的索引值是SHN_COMMON，st_value 值是这个节内容的字节对齐数。
	// • 在重定位文件中，如果一个符号是已定义的，那么它的st_value值 是该符号的起始地址在其所在节中的偏移量，
	//   而其所在的节的索引由 st_shndx 给出。
	// • 在可执行文件和共享库文件(so)中，st_value不再是一个节内的偏移量，而是一个虚拟地址，直接指向符号所
	//   在的内存位置。这种情况下，st_shndx 就不再需要了.
	Elf32_Addr	st_value;	/* Symbol value. */
	// 符号的大小。各种符号的大小各不相同，比如一个对象的大小就是它实际占用的字节数。如果一个符号的大小为 0 或
	// 者大小未知，则这个值为 0
	Elf32_Word	st_size;	/* Size of associated object. */
	unsigned char	st_info;	/* Type and binding information. */
	unsigned char	st_other;	/* Reserved (not used). */
	Elf32_Half	st_shndx;	/* Section index of symbol. */
} Elf32_Sym; // 符号表

/* Macros for accessing the fields of st_info. */
#define ELF32_ST_BIND(info)		((info) >> 4)
#define ELF32_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* Macro for accessing the fields of st_other. */
#define ELF32_ST_VISIBILITY(oth)	((oth) & 0x3)

/* Structures used by Sun & GNU symbol versioning. */
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
