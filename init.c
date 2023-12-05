#include "multiboot.h"
#include "types.h"

static unsigned short *videoram = (unsigned short *)0xB8000; // Base address of the VGA frame buffer
static int attrib = 0x0F; // Black background, white foreground
static int csr_x = 0, csr_y = 0;
#define COLS 80

typedef struct multiboot_memory_map {
    unsigned int size;
    unsigned int base_addr_low, base_addr_high;
    unsigned int length_low, length_high;
    unsigned int type;
} multiboot_memmap_t;

typedef multiboot_memmap_t mmap_entry_t;
 
 enum vga_color
{
  COLOR_BLACK = 0,
  COLOR_BLUE = 1,
  COLOR_GREEN = 2,
  COLOR_CYAN = 3,
  COLOR_RED = 4,
  COLOR_MAGENTA = 5,
  COLOR_BROWN = 6,
  COLOR_LIGHT_GREY = 7,
  COLOR_DARK_GREY = 8,
  COLOR_LIGHT_BLUE = 9,
  COLOR_LIGHT_GREEN = 10,
  COLOR_LIGHT_CYAN = 11,
  COLOR_LIGHT_RED = 12,
  COLOR_LIGHT_MAGENTA = 13,
  COLOR_LIGHT_BROWN = 14,
  COLOR_WHITE = 15,
};
 
uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 24;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void terminal_initialize()
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
  terminal_buffer = (uint16_t*) 0xB8000;
  for ( size_t y = 0; y < VGA_HEIGHT; y++ )
    {
      for ( size_t x = 0; x < VGA_WIDTH; x++ )
	{
	  const size_t index = y * VGA_WIDTH + x;
	  terminal_buffer[index] = make_vgaentry(' ', terminal_color);
	}
    }
}
 
int strlen(const char *text) {
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

// print char
void putc(unsigned char c) {
    if (c == 0x09) { // Tab (move to the next multiple of 8)
        csr_x = (csr_x + 8) & ~(8 - 1);
    } else if (c == '\r') { // CR
        csr_x = 0;
    } else if (c == '\n') { // LF (unix-like)
        csr_x = 0;
        csr_y++;
    } else if (c >= ' ') { // Printable characters
        *(videoram + (csr_y * COLS + csr_x)) = c | (attrib << 8); // Put the character with attributes
        csr_x++;
    }
    if (csr_x >= COLS) {
        csr_x = 0;
        csr_y++;
    } // Wrap around!
}

// print string
void terminal_writestring(const char *text) {
    int length = strlen(text);
    int i = 0;
    for (i = 0; i < length; i++) {
        putc(text[i]);
    }
}

/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
     
  /* If %d is specified and D is minus, put `-' in the head. */
  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;
     
  /* Divide UD by DIVISOR until UD == 0. */
  do
    {
      int remainder = ud % divisor;
     
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);
     
  /* Terminate BUF. */
  *p = 0;
     
  /* Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}

/* ===== FIFOS IMPLEMENTATION BEGIN ===== */

/* === INITIALIZE THREAD STRUCTURES/VARIABLES - BEGIN === */
#define MAX_THREADS 3
#define STACK_SIZE 1024

typedef struct {
  uint32_t *sp;    
  uint32_t tid;  
  uint32_t *bp;             
  uint32_t (*entry)();         
  uint32_t flag;         
}thread_control_block;

typedef struct {
    thread_control_block *pool[MAX_THREADS];
    int front;
    int rear;
    int current_pos;
    int size;
    int max_size;
} queue;

thread_control_block fifos_threads[MAX_THREADS];
queue myQueue = { .pool = fifos_threads, .front = -1, .rear = -1, .size = 0, .max_size = MAX_THREADS, .current_pos = -1};
static uint32_t stack[MAX_THREADS][1024];
/* === INITIALIZE THREAD STRUCTURES/VARIABLES - END === */

/* === SCHEDULER FUNCTIONS - BEGIN === */
void sleep(int milliseconds){
    volatile int i, j;
	for (i = 0; i < milliseconds; i++) {
        for (j = 0; j < 800000; j++) {
        }
    }
}

int find_next_thread(){
  int next_id = myQueue.current_pos;
  for (int j = 0; j < MAX_THREADS; j++){
    next_id = (next_id + 1) % MAX_THREADS;
    if (fifos_threads[next_id].flag== 0){
      return next_id;
    }
  }
  sleep(10000);
  return -100;
}

void switch_thread(int from, int to){
  thread_control_block *from_tcb = &fifos_threads[from];
  thread_control_block *to_tcb = &fifos_threads[to];
  if (from == -1) {
      __asm__ volatile("call switch" : : "S"(0), "D"(to_tcb));
  } 
  else {
      __asm__ volatile("call switch" : : "S"(from_tcb), "D"(to_tcb));
  } 
}

void schedule() {
  int next_id = find_next_thread();
  int temp = myQueue.current_pos;
  myQueue.current_pos = next_id;
  switch_thread(temp, next_id);
}

void thread_yield() {
  schedule();
}

void exit_thread() {
  fifos_threads[myQueue.current_pos].flag = 1;
  schedule();
}
/* === SCHEDULER FUNCTIONS - END === */

/* === QUEUE FUNCTIONS - BEGIN === */
void runqueue_add(thread_control_block *tcb) {
    // Check if the circular queue is full
    if ((myQueue.front == 0 && myQueue.rear == myQueue.max_size - 1) ||
        (myQueue.rear == (myQueue.front - 1) % (myQueue.max_size - 1))) {
        terminal_writestring("Queue is full. Cannot enqueue more pool.\n");
        return;
    }

    // If the queue is empty
    if (myQueue.front == -1) {
        myQueue.front = myQueue.rear = 0;
    } else {
        myQueue.rear = (myQueue.rear + 1) % myQueue.max_size;
    }

    // Add the thread_control_block to the circular queue
    myQueue.pool[myQueue.rear] = tcb;
    myQueue.size++;
}
/* === QUEUE FUNCTIONS - END === */

/* === THREAD FUNCTIONS - BEGIN === */
static void thread1(){
  terminal_writestring("Start <1>\n");
  int i = 0;
  while (i < 5){
    i = i + 1;
    terminal_writestring("<1>\n");
    sleep(100);    
    thread_yield();
  }
  terminal_writestring("Done <1>\n");
  exit_thread();
}

static void thread2(){
  terminal_writestring("Start <2>\n");
  int i = 0;
  while (i < 3) {
      i = i + 1;
      terminal_writestring("<2>\n");
      sleep(100);
      thread_yield();
  }
  terminal_writestring("Done <2>\n");
  exit_thread();
}

static void thread3(){
  terminal_writestring("Start <3>\n");
  int i = 0;
  while (i < 3) {
    i = i + 1;
    terminal_writestring("<3>\n");
    sleep(100);
    thread_yield();
  }
    terminal_writestring("Done <3>\n");
    exit_thread();
  }

/* === THREAD FUNCTIONS - END === */

uint32_t thread_create(void* stack, void* function) {
  uint16_t ds = 0x10, es = 0x10, fs = 0x10, gs = 0x10;
  int new_pcb = myQueue.rear + 1;

  fifos_threads[new_pcb].tid = new_pcb;
  fifos_threads[new_pcb].bp = (uint32_t *)stack;
  fifos_threads[new_pcb].entry = function;
  fifos_threads[new_pcb].flag = 0;

  /* Fake initial context for new thread */
  fifos_threads[new_pcb].sp =(uint32_t *) (((uint16_t *)stack) - 22);

  /* EIP */ *(((uint32_t *)stack) - 0) = fifos_threads[new_pcb].entry;
  /* FLG */ *(((uint32_t *)stack) - 1) = 0;
  /* EAX */ *(((uint32_t *)stack) - 2) = 0;
  /* ECX */ *(((uint32_t *)stack) - 3) = 0;
  /* EDX */ *(((uint32_t *)stack) - 4) = 0;
  /* EBX */ *(((uint32_t *)stack) - 5) = 0;
  /* ESP */ *(((uint32_t *)stack) - 6) = 0;
  /* EBP */ *(((uint32_t *)stack) - 7) = 0;
  /* ESI */ *(((uint32_t *)stack) - 8) = 0;
  /* EDI */ *(((uint32_t *)stack) - 9) = 0;

  /* DS */ *(((uint16_t *)stack) - 19) = ds;
  /* ES */ *(((uint16_t *)stack) - 20) = es;
  /* FS */ *(((uint16_t *)stack) - 21) = fs;
  /* GS */ *(((uint16_t *)stack) - 22) = gs;

  runqueue_add(&fifos_threads[new_pcb]);
  return 1;
}


void init(multiboot* pmb) {
  terminal_initialize();
  terminal_writestring("FIFOS-1:\n");
  sleep(100);
  thread_create(&stack[0][1023], thread1);
  thread_create(&stack[1][1023], thread2);
  thread_create(&stack[2][1023], thread3);
  schedule();
  return;
}