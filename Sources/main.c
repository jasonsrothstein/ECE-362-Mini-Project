/*******************************************************************
ECE 362 MINI PROJECT CODE
JASON ROTHSTEIN, JORDAN WARNE, DAVID PIMLEY
"SIMON SAYS"

PWM CH0 for Speaker
ATD CH0 for Potentiometer (difficulty)
RTI @ 2.048 ms (sample pushbuttons)
TIM CH7 @ 1 ms (with interrupts) for game timing
Port T pins 1-6 for LED outputs
Port AD pins 1-6 for pushbutton inputs
*******************************************************************/

#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include <mc9s12c32.h>
#include <stdlib.h>

/* Declare all functions called from/after main here */
void shiftout(char);
void lcdwait(void);
void send_byte(char);
void send_i(char);
void chgline(char);
void print_c(char);
void pmsglcd(char[]);

void win(void);
void lose(void);
void generateOrder(void);
void dispround(void);
void lightup(char);
void waitlevel(void);
void selectDiff(void);

/* Variable Declarations */
char button1; //button flags
char button2; //1-6 in left-to-right order
char button3;
char button4;
char button5;
char button6;

char startbutton;  //indicates button press for START

char round;        //round #. increments by 1 every time you correctly guess a complete sequence
char guessround;   //index variable to increment through the sequence array. indicates the current guess, from 0->round#
char startflg;     //start flag 
char playflg;
char sequence[250];

char prev1;
char prev2;
char prev3;
char prev4;
char prev5;
char prev6;
char prevstart;

unsigned char difficulty;
unsigned char prev_difficulty = 0;

char tenthflg = 0;
char secflg = 0;
char milli = 0;
char tenth = 0;
char tenthadder = 0;

//globals //
char hundreds1 = 0;
char tens1 = 0;
char ones1 = 0;    
char hundreds2 = 0;
char tens2 = 0;
char ones2 = 0;
char ind1 = 0;
char ind2 = 0;
char ind3 = 0;


//#define DIF1 0x00
#define DIF2 0x33    //difficulty thresholds (fractions of 0xFF)
#define DIF3 0x66
#define DIF4 0x99
#define DIF5 0xCC

#define BUTTON_1 0   //lookup value for button sequence
#define BUTTON_2 1
#define BUTTON_3 2
#define BUTTON_4 3
#define BUTTON_5 4
#define BUTTON_6 5

#define LED1 PTT_PTT1   //route LEDs and PBs to hardware on Port T and ATD
#define LED2 PTT_PTT2
#define LED3 PTT_PTT3
#define LED4 PTT_PTT4
#define LED5 PTT_PTT5
#define LED6 PTT_PTT6

#define PB1 PTAD_PTAD1
#define PB2 PTAD_PTAD2
#define PB3 PTAD_PTAD3
#define PB4 PTAD_PTAD4
#define PB5 PTAD_PTAD5
#define PB6 PTAD_PTAD6
#define PBSTART PTAD_PTAD7

#define LED_ON 0
#define LED_OFF 1


/* Other Configurations */ 

/* ASCII character definitions */
#define CR 0x0D	// ASCII return character   

/* LCD COMMUNICATION BIT MASKS */
#define RS 0x04		// RS pin mask (PTT[2])
#define RW 0x08		// R/W pin mask (PTT[3])
#define LCDCLK 0x10	// LCD EN/CLK pin mask (PTT[4])

/* LCD INSTRUCTION CHARACTERS */
#define LCDON 0x0F	// LCD initialization command
#define LCDCLR 0x01	// LCD clear display command
#define TWOLINE 0x38	// LCD 2-line enable command
#define CURMOV 0xFE	// LCD cursor move instruction
#define LINE1 0x80	// LCD line 1 cursor position
#define LINE2 0xC0	// LCD line 2 cursor position


/*	 	   		
***********************************************************************
 Initializations
***********************************************************************
*/

void  initializations(void) {

/* Set the PLL speed (bus clock = 24 MHz) */
  CLKSEL = CLKSEL & 0x80; // disengage PLL from system
  PLLCTL = PLLCTL | 0x40; // turn on PLL
  SYNR = 0x02;            // set PLL multiplier
  REFDV = 0;              // set PLL divider
  while (!(CRGFLG & 0x08)){  }
  CLKSEL = CLKSEL | 0x80; // engage PLL

/* Disable watchdog timer (COPCTL register) */
  COPCTL = 0x40   ; // COP off; RTI and COP stopped in BDM-mode

/* Initialize asynchronous serial port (SCI) for 9600 baud, interrupts off initially */
  SCIBDH =  0x00; //set baud rate to 9600
  SCIBDL =  0x9C; //24,000,000 / 16 / 156 = 9600 (approx)  
  SCICR1 =  0x00; //$9C = 156
  SCICR2 =  0x0C; //initialize SCI for program-driven operation
  DDRB   =  0x10; //set PB4 for output mode
  PORTB  =  0x10; //assert DTR pin on COM port
 
/* Add additional port pin initializations here */
   DDRM = 0xFF; //Set Port M pins 0 and 1 to Outputs.

/* Initialize SPI for baud rate of 6 Mbs */
   SPIBR = 0x01; //Set SPT Baud Rate to 6.25 Mbs 
   SPICR1 = 0x50;
   SPICR2 = 0x00;
   SPISR = 0x00;
   
/* Initialize digital I/O port pins */
  DDRT = 0x7E; //Port T pins 1-6 are outputs (for LED's)
  ATDDIEN = 0xFE; //Port AD pins 1-7 are inputs (for Pushbuttons);         
         
/* Initialize TIM Ch 7 (TC7) for periodic interrupts every 1.000 ms
     - enable timer subsystem
     - set channel 7 for output compare
     - set appropriate pre-scale factor and enable counter reset after OC7
     - set up channel 7 to generate 1 ms interrupt rate
     - initially disable TIM Ch 7 interrupts      
*/
  TSCR1 = 0x80; //Set Timer Enable Bit
  TIOS  = 0x80; //Set Channel 7 to Output Compare
  TSCR2 = 0x0C; //Reset Counter and Prescaler = 16.
  TC7   = 1500; //Interrupt every 1500 ticks. 
  TIE   = 0x00; //Initially disable TIM Ch 7 Interrupts  


/* Initialize PWM Channel 0*/
  PWME = 0x00; //Initially dissable PWM channel 0;
  PWMPOL = 0x01; //Set PWM channel 0 to positive polarity.
  PWMCLK = 0x01; //Set clock for PWM channel 0 to Scaled Clock A.
  PWMPRCLK = 0x07; //Clock A = 187.5kHz.
  PWMSCLA = 0x20; //Scaled Clock A = 2900Hz: ///By adjusting period: Frequency Range: 11.5-1460 Hz.///
  PWMPER0 = 0; //initially not set
  PWMDTY0 = 0; //initially off
  MODRR = 0x01; //Set PWM channel 0 output on PTT0.

  
/* Initialize ATD Channel 0*/
  ATDCTL2 = 0x80; //Power Up ATD
  ATDCTL3 = 0x08; //1 conversion per sequence.   (non-FIFO mode)
  ATDCTL4 = 0x85; //2Mhz ATD clock, 8-bit mode.


/*
  Initialize the RTI for an 2.048 ms interrupt rate
*/
  CRGINT = 0x80;
  RTICTL = 0x1F;  


/* 
   Initialize the LCD
     - pull LCDCLK high (idle)
     - pull R/W' low (write state)
     - turn on LCD (LCDON instruction)
     - enable two-line mode (TWOLINE instruction)
     - clear LCD (LCDCLR instruction)
     - wait for 2ms so that the LCD can wake up     
*/ 
  PTM = PTM | LCDCLK; //pull LCD clk high
 	send_i(LCDON);
  send_i(TWOLINE);
  send_i(LCDCLR);
  lcdwait();	
  
  
  //LEDs initially off
  LED1 = LED_OFF;
  LED2 = LED_OFF;
  LED3 = LED_OFF;
  LED4 = LED_OFF;
  LED5 = LED_OFF;
  LED6 = LED_OFF;
  
  //set flags
  startflg = 1;
  playflg = 0;
  button1 = 0;
  button2 = 0;
  button3 = 0;
  button4 = 0;
  button5 = 0;
  button6 = 0;
  startbutton = 0;
  round = 0;
  guessround = 0;	 		  			 		  		
	      
}



/*	 		  			 		  		
***********************************************************************
Main                                  
***********************************************************************
*/


void main(void) {
  /* put your own code here */
  DisableInterrupts;
  initializations();
	EnableInterrupts;

  for(;;) {
  
 
  /* BUTTON 1 PRESSED WHILE ENABLED */  
    if (button1 && playflg) {      //button push only counts when playflg on (like an enable)
      button1 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_1);     //light & sound response
      if (BUTTON_1 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
          
  /* BUTTON 2 PRESSED WHILE ENABLED */ 
    if (button2 && playflg) {      //button push only counts when playflg on (like an enable)
      button2 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_2);     //light & sound response
      if (BUTTON_2 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
           
  /* BUTTON 3 PRESSED WHILE ENABLED */ 
    if (button3 && playflg) {      //button push only counts when playflg on (like an enable)
      button3 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_3);     //light & sound response
      if (BUTTON_3 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
           
  /* BUTTON 4 PRESSED WHILE ENABLED */ 
    if (button4 && playflg) {      //button push only counts when playflg on (like an enable)
      button4 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_4);     //light & sound response
      if (BUTTON_4 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
        
  /* BUTTON 5 PRESSED WHILE ENABLED */ 
    if (button5 && playflg) {      //button push only counts when playflg on (like an enable)
      button5 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_5);     //light & sound response
      if (BUTTON_5 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
         
  /* BUTTON 6 PRESSED WHILE ENABLED */ 
    if (button6 && playflg) {      //button push only counts when playflg on (like an enable)
      button6 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_6);     //light & sound response
      if (BUTTON_6 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;       //enable input when still guessing
      } else {
        lose();
      }
    }
                
    
  } /* loop forever */
}  /* never leave main */

/*
***********************************************************************
 RTI interrupt service routine: RTI_ISR
  Initialized for 2.048 ms interrupt rate
  Samples state of pushbuttons
***********************************************************************
*/
interrupt 7 void RTI_ISR(void)
{
  	// clear RTI interrupt flag
  	CRGFLG = CRGFLG | 0x80; 
  	if (PB1 < prev1) {     //sample each pushbutton (not RESET)
  	  button1 = 1;         //set button flag
  	}
  	prev1 = PB1;
  	
  	if (PB2 < prev2) {
  	  button2= 1;
  	}
  	prev2 = PB2;
  	
  	if (PB3 < prev3) {
  	  button3= 1;
  	}
  	prev3 = PB3;
  	
  	if (PB4 < prev4) {
  	  button4= 1;
  	}
  	prev4 = PB4;
  	
  	if (PB5 < prev5) {
  	  button5= 1;
  	}
  	prev5 = PB5;
  	
  	if (PB6 < prev6) {
  	  button6= 1;
  	}
  	prev6 = PB6;
  	
  	if (PBSTART < prevstart) {
  	  startbutton= 1;
  	}
  	prevstart = PBSTART;
  
}

/*
*********************************************************************** 
  TIM Channel 7 interrupt service routine
  Initialized for 1.00 ms interrupt rate
  Increment (3-digit) BCD variable "react" by one
***********************************************************************
*/

interrupt 15 void TIM_ISR(void)
{
	// clear TIM CH 7 interrupt flag
 	TFLG1 = TFLG1 | 0x80; 
 		
 	milli++;              //coutns each ms
 	if(milli == 100) {
 	  tenth++;            //counts each 0.1 s
 	  milli = 0;
 	  tenthflg = 1;
 	}
 	if (tenth == 10) {
 	  secflg = 1;
 	  tenth = 0;
 	  tenthadder++;       //counts up intervals of 0.1 sec - used such that its only reset in wait function (or when it hits 255)
 	}
 	
 	if (startflg) {    //startflg indicates phase before game starts
    selectDiff();            //select difficulty (continuous)
    if (startbutton) {
      startflg = 0;         //exit loop when start button is pressed, and start game
      round = 0;
      generateOrder();   //come up with random sequence to match
      dispround();        //display first round
      playflg = 1;     //enable input
      PWME = 0x01;     //enable PWM
    }
  }
 	
 	
 	/*
 	if((PTT & 0x7E) == 0x7E) {
 	  PTT = PTT & ~0x7E;
 	} else {
 	  PTT = PTT | 0x7E;
 	}
 	*/
}

/***************************************************************************** 
User Defined/Helper Functions called in Main 
*****************************************************************************/

/*  SELECT DIFFICULTY FUNCTION
    SAMPLES POTENTIOMETER AND COMPARES TO THRESHOLDS
    PRINTS DIFFICULTY TO LCD
    CALLED CONTINUOUSLY BEFORE START BUTTON PRESS, NEVER AFTER
*/
void selectDiff() {          //uses potentiometer to select difficulty
  ATDCTL5 = 0x00;
  while (ATDSTAT0 == 0x00){
  }
  difficulty = ATDDR0H;
  if ((prev_difficulty) / 40 != (difficulty / 40)){
      send_i(LCDCLR);
      chgline(LINE1);
      pmsglcd("Difficulty:");
      chgline(LINE2);
      if (difficulty > 200) {          //print difficulty as it is being selected
        pmsglcd("WTF");
      } else if (difficulty > 150) {
        pmsglcd("Pro");
      } else if (difficulty > 100) {
        pmsglcd("Shmedium");
      } else if (difficulty > 50) {
        pmsglcd("Amateur");
      } else {
        pmsglcd("Chump");
      }
  }
  prev_difficulty = difficulty;
}

/*  DISPLAY ROUND FUNCTION
    DISPLAYS ROUND NUMBER ON LINE 2
    OUTPUTS TIMED SEQUENCE OF LIGHTS UP TO ROUND NUMBER ON LEDS
    TELLS USER TO START INPUT WITH "Go!" ON LINE 1
*/
void dispround() {
  /* display right round on LCD */
  send_i(LCDCLR);
  chgline(LINE2);
  pmsglcd("Round: ");
  hundreds1 = ((round + 1) / 100) % 10; //digits of round score
  tens1 = ((round + 1) / 10) % 10; //round + 1 because it is an index (starts at 0)
  ones1 = (round + 1) / 10;
  if (hundreds1 > 0) {  //controls where digits are displayed based on magnitude of round
    print_c(hundreds1 + 48);
  }
  if (tens1 > 0) {
    print_c(tens1 + 48);
  }
  if (ones1 > 0) {
    print_c(ones1 + 48);
  }
  chgline(LINE1);
  pmsglcd("Simon Says...");
  /* display round output on the lights */
  for (ind1; ind1 <= round; ind1++) {
    lightup(sequence[ind1]);
    waitlevel();
  }
  
  /* Tell user to start input */
  chgline(LINE1);
  pmsglcd("                ");
  chgline(LINE1);
  pmsglcd("Go!");
}
  
  
/*  GENERATE ORDER FUNCTION
    GENERATES NEW RANDOM SEQUENCE OF LENGTH 250
    EACH INDEX IS RANDOM 0-5 NUMBER
*/ 
void generateOrder() {  //generates random game sequence
  for (ind2; ind2 < 250; ind2++) {
    sequence[ind2] = rand() % 6; //random number 0-5
  }
}


/*  LIGHT UP LEDS FUNCTION
    LIGHTS UP CORRESPONDING LED WHENEVER BUTTON IS PRESSED OR SEQUENCE DISPLAYED
    PLAYS SOUND WITH EACH LIGHT
    AMOUNT OF TIME DETERMINED BY LEVEL FUNCTION
*/
void lightup(char button) { //momentary lights & sounds whenever button is pressed (our round sequence output)
  if (button == BUTTON_1) {
    LED1 = LED_ON;
    //play frequency 1
    PWMPER0 = 28; //105 Hz
    PWMDTY0 = 14;
  } else if (button == BUTTON_2) {
    LED2 = LED_ON;
    //play frequency 2
    PWMPER0 = 14; //209 Hz
    PWMDTY0 = 7;
  } else if (button == BUTTON_3) {
    LED3 = LED_ON;
    //play frequency 3
    PWMPER0 = 10;  // 293 Hz
    PWMDTY0 = 5;
  } else if (button == BUTTON_4) {
    LED4 = LED_ON;
    //play frequency 4
    PWMPER0 = 8;  //366 Hz
    PWMDTY0 = 4;
  } else if (button == BUTTON_5) {
    LED5 = LED_ON;
    //play frequency 5
    PWMPER0 = 6;  //488 Hz
    PWMPER0 = 3;
  } else if (button == BUTTON_6) {
    LED6 = LED_ON;
    //play frequency 6
    PWMPER0 = 4;  //732 Hz
    PWMDTY0 = 2;
  }
  
  waitlevel();
  
  PWMDTY0 = 0;  //change duty cycle to 0% (pwm off)
  
  LED1 = LED_OFF; //reset lights to off
  LED2 = LED_OFF;
  LED3 = LED_OFF;
  LED4 = LED_OFF;
  LED5 = LED_OFF;
  LED6 = LED_OFF;
}


/*  WIN ROUND FUNCTION
    FLASHES ALL LIGHTS ON DURING WIN JINGLE OF A FEW TONES
    INCREASES ROUND NUMBER AND DISPLAYS THE NEXT ROUND
*/
void win() {
  chgline(LINE1);  //success message
  pmsglcd("                ");
  chgline(LINE1);
  pmsglcd("Correct!");
  
  LED1 = LED_ON; //all lights flash
  LED2 = LED_ON;
  LED3 = LED_ON;
  LED4 = LED_ON;
  LED5 = LED_ON;
  LED6 = LED_ON;
  //play 2-3 tone jingle
  LED1 = LED_OFF;
  LED2 = LED_OFF;
  LED3 = LED_OFF;
  LED4 = LED_OFF;
  LED5 = LED_OFF;
  LED6 = LED_OFF;
  
  round++; //increment round
  guessround = 0;  //change guess index (within sequence) to zero
  dispround();     //display next round LEDs
}


/*  LOSE - INCORRECT INPUT FUNCTION
    LIGHTS UP RED LEDS AND PLAYS LOSING JINGLE OF A FEW TONES
    DISPLAYS FINAL SCORE ON LINE 2
    DOES NOT SET PLAY FLAG AGAIN - USER CAN ONLY RESTART GAME WITH RESET BUTTON AT THIS POINT
*/
void lose() {
  send_i(LCDCLR);   //failure message
  chgline(LINE1);
  pmsglcd("Game Over!");  
  LED2 = LED_ON;  //CHANGE HERE - only want RED lights to light up when you lose
  LED5 = LED_ON;
  //play error jingle
  LED2 = LED_OFF;
  LED5 = LED_OFF;
  chgline(LINE2);
  pmsglcd("Final Score: ");
  //same display round code as dispround function
  hundreds2 = (round / 100) % 10; //digits of round score
  tens2 = (round / 10) % 10; //round because need last COMPLETED round
  ones2 = round / 10;
  if (hundreds2 > 0) {  //controls where digits are displayed based on magnitude of round
    print_c(hundreds2 + 48);
  }
  if (tens2 > 0) {
    print_c(tens2 + 48);
  }
  if (ones2 > 0) {
    print_c(ones2 + 48);
  }
}
 
 
/*  WAIT FUNCTION
    WAIT TIME DEPENDS ON LEVEL - HARDER LEVELS WAIT LESS
    WAIT DETERMINES LENGTH OF LED FLASH AND SOUND PULSE, AND SPEED AT WHICH PATTERN IS DISPLAYED
*/ 
void waitlevel() {       
  if (difficulty > 200) {          //determine difficulty level given thresholds
    ind3 = 1;
  } else if (difficulty > 150) {
    ind3 = 3;                          //i values are arbitrary - (i.e. i=5 waits 0.2*5= 1 second)
  } else if (difficulty > 100) {
    ind3 = 5;
  } else if (difficulty > 50) {
    ind3 = 7;
  } else {
    ind3 = 9;
  } 
    
  for (ind3; ind3 > 0; ind3--) {       ///difficulty determines how many times this runs
    tenthadder = 0;
    while (tenthadder < 2) {   ///waits 0.2 sec
    }
  }
}
  


/* CODE TAKEN FROM LAB ASSIGNMENTS - LCD MESSAGE CONFIGURATION */
/*
***********************************************************************
  shiftout: Transmits the character x to external shift 
            register using the SPI.  It should shift MSB first.  
            MISO = PM[4]
            SCK  = PM[5]
***********************************************************************
*/
 
void shiftout(char x)

{
 
  // test the SPTEF bit: wait if 0; else, continue
  // write data x to SPI data register
  // wait for 30 cycles for SPI data to shift out 

  int ind;
  while (SPISR_SPTEF == 0) {
  }
  SPIDR = x;
  for (ind = 0; ind < 30; ind++) {
  }
}

/*
***********************************************************************
  lcdwait: Delay for approx 2 ms
***********************************************************************
*/

void lcdwait()
{
  int ind;
  for (ind = 0; ind < 5000; ind++) {
  }
}

/*
*********************************************************************** 
  send_byte: writes character x to the LCD
***********************************************************************
*/

void send_byte(char x)
{
     // shift out character
     // pulse LCD clock line low->high->low
     // wait 2 ms for LCD to process data
  
  shiftout(x);
  PTT_PTT6 = 0;
  PTT_PTT6 = 1;
  PTT_PTT6 = 0;
  lcdwait();
}

/*
***********************************************************************
  send_i: Sends instruction byte x to LCD  
***********************************************************************
*/

void send_i(char x)
{
        // set the register select line low (instruction data)
        // send byte
  PTT_PTT4 = 0;
  send_byte(x);
}

/*
***********************************************************************
  chgline: Move LCD cursor to position x
  NOTE: Cursor positions are encoded in the LINE1/LINE2 variables
***********************************************************************
*/

void chgline(char x)
{
  send_i(CURMOV);
  send_i(x);
}

/*
***********************************************************************
  print_c: Print (single) character x on LCD            
***********************************************************************
*/
 
void print_c(char x)
{
  PTT_PTT4 = 1;
  send_byte(x);
}

/*
***********************************************************************
  pmsglcd: print character string str[] on LCD
***********************************************************************
*/

void pmsglcd(char str[])
{
  int ind;
  ind = 0;
  while (str[ind] != 0) {
    print_c(str[ind]);
    ind++;
  }
}