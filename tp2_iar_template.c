#include <msp430G2553.h>
#include <stdio.h>

#define uint32_t unsigned long
#define uint16_t unsigned int
#define uint8_t unsigned short


#define BACKUP_REGS 12 /* r4 to r15*/

#define STACK_SIZE  50
#define STACK_TOP   STACK_SIZE - 1   
#define TOTAL_TASKS 3

/*Enable GIE in SR so that the WDT never stops when we go to user task*/
/*Enable SCG0 for 25MHZ CPU execution*/
#define DEFAULT_SR  ((uint16_t)0x0048) 

#define SAVE_CONTEXT()           \
  asm ( "push r4  \n\t" \
        "push r5  \n\t" \
        "push r6  \n\t" \
        "push r7  \n\t" \
        "push r8  \n\t" \
        "push r9  \n\t" \
        "push r10  \n\t" \
        "push r11  \n\t" \
        "push r12  \n\t" \
        "push r13  \n\t" \
        "push r14  \n\t" \
        "push r15  \n\t" \
      );

#define RESTORE_CONTEXT()       \
  asm ( "pop r15  \n\t" \
        "pop r14  \n\t" \
        "pop r13  \n\t" \
        "pop r12  \n\t" \
        "pop r11  \n\t" \
        "pop r10  \n\t" \
        "pop r9  \n\t" \
        "pop r8  \n\t" \
        "pop r7  \n\t" \
        "pop r6  \n\t" \
        "pop r5  \n\t" \
        "pop r4  \n\t" \
        "reti    \n\t" \
      );

/*stack for each task*/
uint16_t task1ram[STACK_SIZE];
uint16_t task2ram[STACK_SIZE];
uint16_t task3ram[STACK_SIZE];

volatile uint8_t  task_id; /*has the current running task*/
volatile uint16_t *stack_pointer[TOTAL_TASKS]; /*address of stack pointer for each task*/

/*****************************************************/
volatile uint8_t button1 = 0x1, button2=0x1; /*volatile since its a shared resource between tasks*/
/*****************************************************/

void task1(void)
{ 
  volatile unsigned int delay;
  P1DIR |= BIT0;
  while(1) {
    if( button1 ){
      for( delay = 0; delay < 65535; delay++ );
      for( delay = 0; delay < 65535; delay++ );
      for( delay = 0; delay < 65535; delay++ );
      P1OUT ^= BIT0;
    }
  }
}

void task2(void)
{  
  volatile unsigned int delay;
  P4DIR |= BIT7;
  while(1) {
    if( button2 ){
      for( delay = 0; delay < 65535; delay++ );
      P4OUT ^= BIT7;
    }
  }
}

void task3(void)
{
  /*TODO*/
}


/*****************************************************/

/*
 * This function will initialise stack for each task. Following are filled into the stack
 * 1) Store the PC first since it has to be poped back and loaded at end 
 * 2) then store SR register. This determines the CPU condition at which your task should execute
 * 3) leave the space for the registers r4 to r15 since the scheduler will pop these when a task switching occurs
 * 3) return the address of stack which will contain all informations for the task to execute after task switching
 * 4) TODO: fill the end of stack with known values to detect buffer overflows.
 */
uint16_t *initialise_stack(void (* func)(void), uint16_t *stack_location)
{
  uint8_t i;
  
  /*MSP430F5529 has a 20bit PC register*/
  *stack_location = (uint16_t)func; //last 16 bits will only stored. Pending 4bits will be stored with SR
  stack_location--;
  /*refer datasheet to see how 20bit PC is stored along with SR*/
  *stack_location = (((uint16_t)((uint32_t)(0xf0000 & (uint32_t)func) >> 4))| DEFAULT_SR); 
  
  /*leave space in stack for r4 to r15*/
  for(i= 0; i< BACKUP_REGS; i++)
  {
    stack_location--;
  }
  
  return stack_location;
}


  
volatile uint16_t *temp;
void main(void)
{
  //Stop the watchdog timer until we configure our scheduler
  WDTCTL = WDTPW + WDTHOLD;
  
  /*initialise stack for each task*/
  stack_pointer[0] = initialise_stack(task1, &task1ram[STACK_TOP]);
  stack_pointer[1] = initialise_stack(task2, &task2ram[STACK_TOP]);
  stack_pointer[2] = initialise_stack(task3, &task3ram[STACK_TOP]);

  CCTL0 = CCIE;               // Habilita interrupção de comparação do timer A           
  TACTL = TASSEL_2+MC_3+ID_3; // SMCLK = 1 MHz, SMCLK/8 = 125 KHz (8 us)      
  CCR0 =  62500;              // Modo up/down: chega no valor e depois volta
                              // para zero, portanto cada interrupção acontece
                              // 2 * 62500 * 8 us = 1 segundo 

  /*initialise to first task*/
  task_id = 0;
  temp = stack_pointer[task_id];
  asm ("mov.w  temp, r1 \n\t");
  _BIS_SR(GIE);  
  // lets pop our first task out!
  RESTORE_CONTEXT();
}



#pragma vector=TIMER0_A0_VECTOR
// Scheduler
__interrupt void Timer_A (void)
{
  //0 -Save Context 
  SAVE_CONTEXT();
  //1-Load Stack Pointer
  asm volatile ("mov.w r1, temp \n\t");
  stack_pointer[task_id] = temp;
  //2-update the task id
  task_id = (task_id+1) % TOTAL_TASKS;
  //3-Save Stack pointer
  temp = stack_pointer[task_id];
  asm volatile ("mov.w temp, r1 \n\t");
  //4 Load Context 
  RESTORE_CONTEXT():
  
}
