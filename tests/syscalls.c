// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/signal.h>
#include "util.h"
#include "mmio.h"
#include "lnic-scheduler.h"
#include "lnic.h"

#define SYS_read  63
#define SYS_write 64
#define SYS_getmainvars 2011

// Base address for uart. Can view in chisel synthesis output
#define UART_BASE_ADDR 0x54000000

// UART registers. Defined in sifive documentation and in scala source.
#define UART_TX_FIFO   0x00
#define UART_TX_CTRL   0x08
#define UART_RX_CTRL   0x0c
#define UART_DIV       0x18

// Rocket-chip peripheral clock frequency.
#define PERIPHERAL_CLOCK_FREQ 100000000 // 100 MHz

// Baud rate expected by firesim uart bridge
#define UART_BRIDGE_BAUDRATE  115200

// Frequency/baudrate, used to set uart divisor
#define UART_DIVISOR          868

// Enable uart tx with one stop bit and dump buffer at one byte
#define UART_TX_EN            0b10000000000000011

#define MAX_ARGS_BYTES 1024
uint64_t mainvars[MAX_ARGS_BYTES / sizeof(uint64_t)];
uint64_t argc;
char ** argv;

#define NCORES 4
int core_complete[NCORES];

int thread_failed[NCORES];

#undef strcmp

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

uint32_t use_uart = 0;

bool did_init = false;
int core_global_ret = 0;
uint64_t num_exited = 0;

uint64_t num_threads_exited[NCORES];

bool __attribute__((weak)) is_single_core()
{
  return true;
}

typedef struct {
  volatile unsigned int lock;
} arch_spinlock_t;

#define arch_spin_is_locked(x) ((x)->lock != 0)

void arch_spin_unlock(arch_spinlock_t *lock) {
  asm volatile (
    "amoswap.w.rl x0, x0, %0"
    : "=A" (lock->lock)
    :: "memory"
    );
}

int arch_spin_trylock(arch_spinlock_t* lock) {
  int tmp = 1, busy;
  asm volatile (
    "amoswap.w.aq %0, %2, %1"
    : "=r"(busy), "+A" (lock->lock)
    : "r"(tmp)
    : "memory"
    );
  return !busy;
}

void arch_spin_lock(arch_spinlock_t* lock) {
  while (1) {
    // Thread is idle if locked
    write_csr(0x056, 2);
    if (arch_spin_is_locked(lock)) {
      continue;
    }
    if (arch_spin_trylock(lock)) {
      break;
    }
  }
}

arch_spinlock_t init_lock, exit_lock, print_lock;
arch_spinlock_t thread_lock[NCORES];

// Kernel thread structure
struct thread_t {
  uintptr_t regs[NUM_REGS];
  uintptr_t epc;
  uintptr_t padding;
} __attribute__((packed));

// Thread metadata storage in global state
struct thread_t threads[NCORES][MAX_THREADS + 2]; // Extra one for dummy main thread, and another because index 0 is reserved for asm scratch space
uint64_t num_threads[NCORES];

struct thread_t* new_thread() {
  uint64_t hart_id = csr_read(mhartid);
  //csr_clear(mie, TIMER_INT_ENABLE);
  if (num_threads[hart_id] == MAX_THREADS) {
    exit(ERR_THREADS_EXHAUSTED);
  }
  struct thread_t* next_thread = &threads[hart_id][num_threads[hart_id]];
  next_thread->epc = 0;
  num_threads[hart_id]++;
  //csr_set(mie, TIMER_INT_ENABLE);
  return next_thread;
}

void app_wrapper(uint64_t argc, char** argv, int cid, int nc, uint64_t context_id, uint64_t priority, int (*target)(uint64_t, char**, int, int, uint64_t, uint64_t)) {
  if (is_single_core()) {
    // Run the thread
    int retval = target(argc, argv, cid, nc, context_id, priority);
    printf("Core %d (only core) application %d exited with code %d\n", cid, context_id, retval);
    uint64_t hart_id = csr_read(mhartid);
    arch_spin_lock(&thread_lock[hart_id]);
    num_threads_exited[hart_id]++;
    if (retval != 0) {
      thread_failed[hart_id] = -1;
    }
    arch_spin_unlock(&thread_lock[hart_id]);

    // Join all threads
    while (1) {
      arch_spin_lock(&thread_lock[hart_id]);
      if (num_threads_exited[hart_id] == num_threads[hart_id] - 1) {
        exit(thread_failed[hart_id]);
      } else {
        arch_spin_unlock(&thread_lock[hart_id]);
        for (int i = 0; i < 1000; i++) {
          asm volatile("nop");
        }
      }
    }
  } else {
    // Run the thread
    int retval = target(argc, argv, cid, nc, context_id, priority);
    printf("Core %d application %d exited with code %d\n", cid, context_id, retval);
    uint64_t hart_id = csr_read(mhartid);
    arch_spin_lock(&thread_lock[hart_id]);
    num_threads_exited[hart_id]++;
    if (retval != 0) {
      thread_failed[hart_id] = -1;
    }
    arch_spin_unlock(&thread_lock[hart_id]);
    // Join all threads on this core
    while (1) {
      arch_spin_lock(&thread_lock[hart_id]);
      if (num_threads_exited[hart_id] == num_threads[hart_id] - 1) {
        arch_spin_unlock(&thread_lock[hart_id]);
        break;
      } else {
        arch_spin_unlock(&thread_lock[hart_id]);
        for (int i = 0; i < 1000; i++) {
          asm volatile("nop");
        }
      }
    }

    // Stall all threads but one
    csr_clear(mie, LNIC_INT_ENABLE | TIMER_INT_ENABLE);

    // Join all other cores
    arch_spin_lock(&exit_lock);
    core_global_ret |= (thread_failed[hart_id] & 0xFF) << (8*cid);
    num_exited++;
    arch_spin_unlock(&exit_lock);
    while (1) {
      arch_spin_lock(&exit_lock);
      if (num_exited == NCORES) {
        exit(core_global_ret);
      } else {
        arch_spin_unlock(&exit_lock);
        for (int i = 0; i < 1000; i++) {
          asm volatile("nop");
        }
      }
    }
  }
}

void start_thread(int (*target)(void), uint64_t id, uint64_t priority) {
  struct thread_t* thread = new_thread();
  thread->epc = app_wrapper;
  lnic_add_context(id, priority);
  volatile uint64_t sp, gp, tp;
  asm volatile("mv %0, sp" : "=r"(sp));
  asm volatile("mv %0, gp" : "=r"(gp));
  asm volatile("mv %0, tp" : "=r"(tp));
  thread->regs[REG_SP] = sp - STACK_SIZE_BYTES * (1 + id);
  thread->regs[REG_GP] = gp;
  thread->regs[REG_TP] = tp;
  thread->regs[REG_A0] = argc;
  thread->regs[REG_A1] = argv;
  thread->regs[REG_A2] = csr_read(mhartid);
  thread->regs[REG_A3] = NCORES;
  thread->regs[REG_A4] = id;
  thread->regs[REG_A5] = priority;
  thread->regs[REG_A6] = target;
}

void scheduler_init() {
  uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
  uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO + (read_csr(mhartid) << 3); // One eight-byte word offset per hart id
  *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
  num_threads[read_csr(mhartid)] = 1;
  csr_write(mscratch, &threads[read_csr(mhartid)][0]); // mscratch now holds thread base addr
}

void scheduler_run() {
  // Turn on the timer interrupts and wait for the scheduler to start
  csr_write(0x53, MAX_THREADS); // Set the main thread's id to an illegal value

  csr_write(0x55, MAX_THREADS); // Set the main thread's priority to a low value

  // This will keep it from being re-scheduled.
  uint64_t* mtime_ptr_lo = MTIME_PTR_LO;
  uint64_t* mtimecmp_ptr_lo = MTIMECMP_PTR_LO + (read_csr(mhartid) << 3); // One eight-byte word offset per hart id
  *mtimecmp_ptr_lo = *mtime_ptr_lo + TIME_SLICE_RTC_TICKS;
  csr_set(mie, LNIC_INT_ENABLE);
  csr_set(mie, TIMER_INT_ENABLE);
  asm volatile ("wfi");
}

void uart_init() {
  reg_write32(UART_BASE_ADDR + UART_TX_CTRL, UART_TX_EN);    
  reg_write32(UART_BASE_ADDR + UART_DIV, UART_DIVISOR);
}

void uart_write_char(char ch) {
  while ((int32_t)reg_read32(UART_BASE_ADDR + UART_TX_FIFO) < 0);
  reg_write8(UART_BASE_ADDR + UART_TX_FIFO, ch); 
}

void enable_uart_print(uint32_t uart_print) {
  use_uart = uart_print;
}

static uintptr_t syscall(uintptr_t which, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
  volatile uint64_t magic_mem[8] __attribute__((aligned(64)));
  magic_mem[0] = which;
  magic_mem[1] = arg0;
  magic_mem[2] = arg1;
  magic_mem[3] = arg2;
  __sync_synchronize();

  tohost = (uintptr_t)magic_mem;
  while (fromhost == 0)
    ;
  fromhost = 0;

  __sync_synchronize();
  return magic_mem[0];
}

#define NUM_COUNTERS 2
static uintptr_t counters[NUM_COUNTERS];
static char* counter_names[NUM_COUNTERS];

void setStats(int enable)
{
  int i = 0;
#define READ_CTR(name) do { \
    while (i >= NUM_COUNTERS) ; \
    uintptr_t csr = read_csr(name); \
    if (!enable) { csr -= counters[i]; counter_names[i] = #name; } \
    counters[i++] = csr; \
  } while (0)

  READ_CTR(mcycle);
  READ_CTR(minstret);

#undef READ_CTR
}

void __attribute__((noreturn)) tohost_exit(uintptr_t code)
{
  tohost = (code << 1) | 1;
  while (1);
}

uintptr_t __attribute__((weak)) handle_trap(uintptr_t cause, uintptr_t epc, uintptr_t regs[32])
{
  tohost_exit(1337);
}

void exit(int code)
{
  tohost_exit(code);
}

void abort()
{
  exit(128 + SIGABRT);
}

void getstr(char* buf, uint32_t buf_len) {
  // TODO: fixme. Can read from serial but no character debouncing
  // and lots of trouble at start of program.
  char ch;
  do {
    syscall(SYS_read, 0, &ch, 1);
  } while (ch == 0);
  if (ch != '~') return;
  do {
    syscall(SYS_read, 0, &ch, 1);
  } while (ch == '~');
  buf[0] = ch;
  int i = 1;
  while (ch != '\n' && i < buf_len - 1) {
    syscall(SYS_read, 0, &ch, 1);
    buf[i] = ch;
    i++;
  }
  buf[i] = '\0';
}

void getmainvars(char* buf, uint32_t limit) {
  syscall(SYS_getmainvars, buf, limit, 0);
}

void printstr(const char* s)
{
  syscall(SYS_write, 1, (uintptr_t)s, strlen(s));
}

void __attribute__((weak)) thread_entry(int cid, int nc) {
  if (nc != 4 || NCORES != 4) {
      if (cid == 0) {
          printf("This program requires 4 cores but was given %d\n", nc);
      }
      return;
  }

  int core_local_ret = 0;

  if (is_single_core()) {
    // Support programs that just define a regular main()
    if (cid == 0) {
      core_local_ret = main(argc, argv);
      printf("Core %d (only core) exited with code %d\n", cid, core_local_ret);
      exit(core_local_ret);
    } else {
      while (1);
    }
  } else {
    // Support multicore programs
    core_local_ret = core_main(argc, argv, cid, nc);
    printf("Core %d exited with code %d\n", cid, core_local_ret);
    arch_spin_lock(&exit_lock);
    core_global_ret |= (core_local_ret & 0xFF) << (8*cid);
    num_exited++;
    arch_spin_unlock(&exit_lock);

    while (1) {
      arch_spin_lock(&exit_lock);
      if (num_exited == NCORES) {
        exit(core_global_ret);
      } else {
        arch_spin_unlock(&exit_lock);
        for (int i = 0; i < 1000; i++) {
          asm volatile("nop");
        }
      }
    }
  }

  // Should never get here
  exit(-1);
}

int __attribute__((weak)) core_main(uint64_t argc, char** argv, int cid, int nc) {
    return 0;
}

void print_counters() {
  char buf[NUM_COUNTERS * 32] __attribute__((aligned(64)));
  char* pbuf = buf;
  for (int i = 0; i < NUM_COUNTERS; i++)
    if (counters[i])
      pbuf += sprintf(pbuf, "%s = %d\n", counter_names[i], counters[i]);
  if (pbuf != buf)
    printstr(buf);
}

int __attribute__((weak)) main(int argc, char** argv)
{
  // single-threaded programs override this function.
  printstr("Implement main(), foo!\n");
  return -1;
}

static void init_tls()
{
  register void* thread_pointer asm("tp");
  extern char _tls_data;
  extern __thread char _tdata_begin, _tdata_end, _tbss_end;
  size_t tdata_size = &_tdata_end - &_tdata_begin;
  memcpy(thread_pointer, &_tls_data, tdata_size);
  size_t tbss_size = &_tbss_end - &_tdata_end;
  memset(thread_pointer + tdata_size, 0, tbss_size);
}

void _init(int cid, int nc)
{
  init_tls();
  if (cid == 0) {
    uart_init();
    getmainvars(&mainvars[0], MAX_ARGS_BYTES);
    argc = mainvars[0];
    argv = mainvars + 1;
    if (argc == 0 || argv == 0) {
      printf("Unable to parse program arguments.\n");
      exit(-1);
    }
    arch_spin_lock(&init_lock);
    did_init = true;
    arch_spin_unlock(&init_lock);
  } else {
    // // A really bad fake condition variable wait
    while (1) {
      arch_spin_lock(&init_lock);
      if (did_init) {
        arch_spin_unlock(&init_lock);
        break;
      } else {
        arch_spin_unlock(&init_lock);
        for (int i = 0; i < 100; i++) {
          asm volatile("nop");
        }
      }
    }
  }

  // wait for lnicrdy CSR to be set
  while (read_csr(0x057) == 0);

  thread_entry(cid, nc);

  // We should never get here
  exit(-1);
}

#undef putchar
int putchar(int ch)
{
  if (use_uart) {
    uart_write_char(ch);
    return 0;
  }
  static __thread char buf[64] __attribute__((aligned(64)));
  static __thread int buflen = 0;

  buf[buflen++] = ch;

  if (ch == '\n' || buflen == sizeof(buf))
  {
    syscall(SYS_write, 1, (uintptr_t)buf, buflen);
    buflen = 0;
  }

  return 0;
}

void printhex(uint64_t x)
{
  char str[17];
  int i;
  for (i = 0; i < 16; i++)
  {
    str[15-i] = (x & 0xF) + ((x & 0xF) < 10 ? '0' : 'a'-10);
    x >>= 4;
  }
  str[16] = 0;

  printstr(str);
}

static inline void printnum(void (*putch)(int, void**), void **putdat,
                    unsigned long long num, unsigned base, int width, int padc)
{
  unsigned digs[sizeof(num)*CHAR_BIT];
  int pos = 0;

  while (1)
  {
    digs[pos++] = num % base;
    if (num < base)
      break;
    num /= base;
  }

  while (width-- > pos)
    putch(padc, putdat);

  while (pos-- > 0)
    putch(digs[pos] + (digs[pos] >= 10 ? 'a' - 10 : '0'), putdat);
}

static unsigned long long getuint(va_list *ap, int lflag)
{
  if (lflag >= 2)
    return va_arg(*ap, unsigned long long);
  else if (lflag)
    return va_arg(*ap, unsigned long);
  else
    return va_arg(*ap, unsigned int);
}

static long long getint(va_list *ap, int lflag)
{
  if (lflag >= 2)
    return va_arg(*ap, long long);
  else if (lflag)
    return va_arg(*ap, long);
  else
    return va_arg(*ap, int);
}

static void vprintfmt(void (*putch)(int, void**), void **putdat, const char *fmt, va_list ap)
{
  register const char* p;
  const char* last_fmt;
  register int ch, err;
  unsigned long long num;
  int base, lflag, width, precision, altflag;
  char padc;

  while (1) {
    while ((ch = *(unsigned char *) fmt) != '%') {
      if (ch == '\0')
        return;
      fmt++;
      putch(ch, putdat);
    }
    fmt++;

    // Process a %-escape sequence
    last_fmt = fmt;
    padc = ' ';
    width = -1;
    precision = -1;
    lflag = 0;
    altflag = 0;
  reswitch:
    switch (ch = *(unsigned char *) fmt++) {

    // flag to pad on the right
    case '-':
      padc = '-';
      goto reswitch;
      
    // flag to pad with 0's instead of spaces
    case '0':
      padc = '0';
      goto reswitch;

    // width field
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      for (precision = 0; ; ++fmt) {
        precision = precision * 10 + ch - '0';
        ch = *fmt;
        if (ch < '0' || ch > '9')
          break;
      }
      goto process_precision;

    case '*':
      precision = va_arg(ap, int);
      goto process_precision;

    case '.':
      if (width < 0)
        width = 0;
      goto reswitch;

    case '#':
      altflag = 1;
      goto reswitch;

    process_precision:
      if (width < 0)
        width = precision, precision = -1;
      goto reswitch;

    // long flag (doubled for long long)
    case 'l':
      lflag++;
      goto reswitch;

    // character
    case 'c':
      putch(va_arg(ap, int), putdat);
      break;

    // string
    case 's':
      if ((p = va_arg(ap, char *)) == NULL)
        p = "(null)";
      if (width > 0 && padc != '-')
        for (width -= strnlen(p, precision); width > 0; width--)
          putch(padc, putdat);
      for (; (ch = *p) != '\0' && (precision < 0 || --precision >= 0); width--) {
        putch(ch, putdat);
        p++;
      }
      for (; width > 0; width--)
        putch(' ', putdat);
      break;

    // (signed) decimal
    case 'd':
      num = getint(&ap, lflag);
      if ((long long) num < 0) {
        putch('-', putdat);
        num = -(long long) num;
      }
      base = 10;
      goto signed_number;

    // unsigned decimal
    case 'u':
      base = 10;
      goto unsigned_number;

    // (unsigned) octal
    case 'o':
      // should do something with padding so it's always 3 octits
      base = 8;
      goto unsigned_number;

    // pointer
    case 'p':
      static_assert(sizeof(long) == sizeof(void*));
      lflag = 1;
      putch('0', putdat);
      putch('x', putdat);
      /* fall through to 'x' */

    // (unsigned) hexadecimal
    case 'x':
      base = 16;
    unsigned_number:
      num = getuint(&ap, lflag);
    signed_number:
      printnum(putch, putdat, num, base, width, padc);
      break;

    // escaped '%' character
    case '%':
      putch(ch, putdat);
      break;
      
    // unrecognized escape sequence - just print it literally
    default:
      putch('%', putdat);
      fmt = last_fmt;
      break;
    }
  }
}

int printf(const char* fmt, ...)
{
  arch_spin_lock(&print_lock);
  va_list ap;
  va_start(ap, fmt);

  vprintfmt((void*)putchar, 0, fmt, ap);

  va_end(ap);
  arch_spin_unlock(&print_lock);
  return 0; // incorrect return value, but who cares, anyway?
}

int sprintf(char* str, const char* fmt, ...)
{
  va_list ap;
  char* str0 = str;
  va_start(ap, fmt);

  void sprintf_putch(int ch, void** data)
  {
    char** pstr = (char**)data;
    **pstr = ch;
    (*pstr)++;
  }

  vprintfmt(sprintf_putch, (void**)&str, fmt, ap);
  *str = 0;

  va_end(ap);
  return str - str0;
}

void* memcpy(void* dest, const void* src, size_t len)
{
  if ((((uintptr_t)dest | (uintptr_t)src | len) & (sizeof(uintptr_t)-1)) == 0) {
    const uintptr_t* s = src;
    uintptr_t *d = dest;
    while (d < (uintptr_t*)(dest + len))
      *d++ = *s++;
  } else {
    const char* s = src;
    char *d = dest;
    while (d < (char*)(dest + len))
      *d++ = *s++;
  }
  return dest;
}

void* memset(void* dest, int byte, size_t len)
{
  if ((((uintptr_t)dest | len) & (sizeof(uintptr_t)-1)) == 0) {
    uintptr_t word = byte & 0xFF;
    word |= word << 8;
    word |= word << 16;
    word |= word << 16 << 16;

    uintptr_t *d = dest;
    while (d < (uintptr_t*)(dest + len))
      *d++ = word;
  } else {
    char *d = dest;
    while (d < (char*)(dest + len))
      *d++ = byte;
  }
  return dest;
}

size_t strlen(const char *s)
{
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

size_t strnlen(const char *s, size_t n)
{
  const char *p = s;
  while (n-- && *p)
    p++;
  return p - s;
}

int strcmp(const char* s1, const char* s2)
{
  unsigned char c1, c2;

  do {
    c1 = *s1++;
    c2 = *s2++;
  } while (c1 != 0 && c1 == c2);

  return c1 - c2;
}

char* strcpy(char* dest, const char* src)
{
  char* d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

long atol(const char* str)
{
  long res = 0;
  int sign = 0;

  while (*str == ' ')
    str++;

  if (*str == '-' || *str == '+') {
    sign = *str == '-';
    str++;
  }

  while (*str) {
    res *= 10;
    res += *str++ - '0';
  }

  return sign ? -res : res;
}

// TODO: Reference linux source, borrowed from elsewhere to avoid having to link in the whole library.
int inet_pton4 (const char *src, const char *end, unsigned char *dst)
{
  int saw_digit, octets, ch;
  unsigned char tmp[4], *tp;
  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while (src < end)
    {
      ch = *src++;
      if (ch >= '0' && ch <= '9')
        {
          unsigned int new = *tp * 10 + (ch - '0');
          if (saw_digit && *tp == 0)
            return 0;
          if (new > 255)
            return 0;
          *tp = new;
          if (! saw_digit)
            {
              if (++octets > 4)
                return 0;
              saw_digit = 1;
            }
        }
      else if (ch == '.' && saw_digit)
        {
          if (octets == 4)
            return 0;
          *++tp = 0;
          saw_digit = 0;
        }
      else
        return 0;
    }
  if (octets < 4)
    return 0;
  memcpy (dst, tmp, 4);
  return 1;
}

uint32_t swap32(uint32_t in) {
  // TODO: Did we add an instruction to do this?
  return ((in >> 24) & 0x000000FF) | ((in << 24) & 0xFF000000) | ((in >> 8) & 0x0000FF00) | ((in << 8) & 0x00FF0000);
}
