/* Simple command-line kernel monitor useful for
 * controlling the kernel and exploring the system interactively. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define WHITESPACE "\t\r\n "
#define MAXARGS    16

/* Functions implementing monitor commands */
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);

struct Command {
    const char *name;
    const char *desc;
    /* return -1 to force monitor to exit */
    int (*func)(int argc, char **argv, struct Trapframe *tf);
};

static struct Command commands[] = {
        {"help", "Display this list of commands", mon_help},
        {"kerninfo", "Display information about the kernel", mon_kerninfo},
        {"backtrace", "Print stack backtrace", mon_backtrace},
};
#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

/* Implementations of basic kernel monitor commands */

int
mon_help(int argc, char **argv, struct Trapframe *tf) {
    for (size_t i = 0; i < NCOMMANDS; i++)
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf) {
    extern char _head64[], entry[], etext[], edata[], end[];

    cprintf("Special kernel symbols:\n");
    cprintf("  _head64 %16lx (virt)  %16lx (phys)\n", (unsigned long)_head64, (unsigned long)_head64);
    cprintf("  entry   %16lx (virt)  %16lx (phys)\n", (unsigned long)entry, (unsigned long)entry - KERN_BASE_ADDR);
    cprintf("  etext   %16lx (virt)  %16lx (phys)\n", (unsigned long)etext, (unsigned long)etext - KERN_BASE_ADDR);
    cprintf("  edata   %16lx (virt)  %16lx (phys)\n", (unsigned long)edata, (unsigned long)edata - KERN_BASE_ADDR);
    cprintf("  end     %16lx (virt)  %16lx (phys)\n", (unsigned long)end, (unsigned long)end - KERN_BASE_ADDR);
    cprintf("Kernel executable memory footprint: %luKB\n", (unsigned long)ROUNDUP(end - entry, 1024) / 1024);
    return 0;
}

uint64_t 
get_bp_v(uint8_t* rip, uint64_t current_sp) {
    uint64_t current_bp_v = 0;

    for (uint64_t i = 0; i <= (uint64_t)rip; i++) {
        uint8_t* val = rip - i;
        if (*(uint32_t*)val == 0xfa1e0ff3) { // on prologue
            current_bp_v = current_sp + 0x10;
            break;
        }
        if (val[0] == 0x48 && val[2] == 0xec) { // on sub rbp
            uint64_t offset = 0;
            if (val[1] == 0x81) { // sub 32-bit val
                offset = *(uint32_t*)(val + 3) + 0x10;
            }
            else if (val[1] == 0x83) { // sub 8-bit val
                offset = *(uint8_t*)(val + 3) + 0x10;
            }
            else continue;

            //cprintf("offset // 16: %ld\n", offset % 16);

            offset = offset + 8; // stack is always alligned

            current_bp_v = current_sp + offset;
            break;
        }
    }

    return current_bp_v;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf) {
    // LAB 2: Your code here

    uint8_t* rip = (uint8_t*)read_rip();
    uint64_t current_sp = read_rsp();
    uint64_t current_bp_v = get_bp_v(rip, current_sp) + 0x10;

    uint64_t *prev_bp_p = (uint64_t *) current_bp_v;
    extern void bootstacktop();
	uint64_t final_bp_v = (uint64_t)bootstacktop;
    int time_to_stop = 0;
    while(1) 
	{	
		uint64_t rip_v = *(prev_bp_p + 1);
		uint64_t *rip_p = (uint64_t *) rip_v;
		
		struct Ripdebuginfo info;
		debuginfo_rip( rip_v, &info);
		
		cprintf("rbp %016lx  rip %016lx\n", 
			current_bp_v, 
			rip_v);
			
		int offset = (uint64_t)rip_p - info.rip_fn_addr; 
			
		cprintf("\t %s:%d: %.*s+%d \n", info.rip_file, info.rip_line, info.rip_fn_namelen, info.rip_fn_name, offset);

		current_bp_v = get_bp_v((uint8_t*)rip_v, current_bp_v);
		prev_bp_p = (uint64_t *) current_bp_v;

        if (time_to_stop)
            break;

        if (current_bp_v + 0x10 == final_bp_v)
            time_to_stop = 1;
	}
    /*
    cprintf("Old ver:\n");
    // Read_ebp returns the current value of the ebp register. This first value will not be on the stack. 
	current_bp_v = read_rbp();
	// The value of current_bp_v turned into an address is a pointer to the previous bp
	prev_bp_p = (uint64_t *) current_bp_v;
	
	// Print all relevant items within current_bp_p stack frame. 
	while(current_bp_v + 0x10 != final_bp_v) 
	{	
		uint64_t rip_v = *(prev_bp_p + 1);
		uint64_t *rip_p = (uint64_t *) rip_v;
		
		struct Ripdebuginfo info;
		debuginfo_rip( rip_v, &info);
		
		cprintf("rbp %016lx  rip %016lx final %016lx\n", 
			current_bp_v, 
			rip_v,
            final_bp_v);
			
		int offset = (uint64_t)rip_p - info.rip_fn_addr; 
			
		cprintf("\t %s:%d: %.*s+%d \n", info.rip_file, info.rip_line, info.rip_fn_namelen, info.rip_fn_name, offset);

		current_bp_v = *prev_bp_p;
		prev_bp_p = (uint64_t *) current_bp_v;
	}
    */
    return 0;
}

int
mon_rand_text(int argc, char **argv, struct Trapframe *tf) {
    cprintf("bruh\n");
    return 0;
}

/* Kernel monitor command interpreter */

static int
runcmd(char *buf, struct Trapframe *tf) {
    int argc = 0;
    char *argv[MAXARGS];

    argv[0] = NULL;

    /* Parse the command buffer into whitespace-separated arguments */
    for (;;) {
        /* gobble whitespace */
        while (*buf && strchr(WHITESPACE, *buf)) *buf++ = 0;
        if (!*buf) break;

        /* save and scan past next arg */
        if (argc == MAXARGS - 1) {
            cprintf("Too many arguments (max %d)\n", MAXARGS);
            return 0;
        }
        argv[argc++] = buf;
        while (*buf && !strchr(WHITESPACE, *buf)) buf++;
    }
    argv[argc] = NULL;

    /* Lookup and invoke the command */
    if (!argc) return 0;
    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0)
            return commands[i].func(argc, argv, tf);
    }

    cprintf("Unknown command '%s'\n", argv[0]);
    return 0;
}

void
monitor(struct Trapframe *tf) {

    cprintf("Welcome to the JOS kernel monitor!\n");
    cprintf("Type 'help' for a list of commands.\n");

    char *buf;
    do buf = readline("K> ");
    while (!buf || runcmd(buf, tf) >= 0);
}
