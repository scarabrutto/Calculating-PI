/*
 * Calculating-PI
 *
 * Created: 20.03.2018 18:32:07
 * Author : Scarabelli
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"
#include "math.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"

//Task Variabeln aufstellen
void controllerTask(void *pvParameters);
void leibniztask(void *pvParameters);
void KetteTask(void *pvParameters);
void ButtonTask(void *pvParameters);

//Variable um PI auszugeben
float pi_calc;
uint8_t mode;


//EventgroupButtons
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01 // Start Calculate
#define BUTTON2_SHORT	0x02 // Stop Calculate
#define BUTTON3_SHORT	0x04 // Reset 
#define BUTTON4_SHORT	0x08 // Switch Algorithm
#define Start_Leibniz	0x10 // Start Leibniz Calc 
#define Start_Kette		0x20 // Start Kette Calc 
#define PI_Ready		0x40 // PI Task beendet 
#define BUTTON_ALL		0xFF // Reset all





int main(void)
{
	vInitClock();
	vInitDisplay();
	xEventGroupClearBits(egButtonEvents, BUTTON_ALL); // Buttons zurücksetzen
	
	// Tasks definieren
	xTaskCreate( controllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, NULL);
	xTaskCreate( leibniztask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);
	xTaskCreate( KetteTask, (const char*) "Kette_tsk", configMINIMAL_STACK_SIZE, NULL, 1, NULL); 
	xTaskCreate( ButtonTask, (const char *) "Button_tsk", configMINIMAL_STACK_SIZE, NULL, 3, NULL); 
	
	vDisplayClear(); //Display zurücksetzen
	vDisplayWriteStringAtPos (0,0, "CALCULATE PI");
	vDisplayWriteStringAtPos(3,0,"cal|stp|rst|sw"); // Startbildschirm definieren

	vTaskStartScheduler();
	return 0;
}

//Modes for Finite State Machine
#define MODE_IDLE 0
#define MODE_PI 1
#define MODE_Display 2

void controllerTask(void *pvParameters) {
	float Pi_Ausgabe = 0;
	char Pi_String[20];
	
	mode = MODE_IDLE;
	uint16_t counter = 0;
	uint8_t Zustand;
	for(;;) {
		switch(mode){
			case MODE_IDLE: 
//				xEventGroupSetBits(egButtonEvents, Start_Leibniz); // blockiert Ablauf
				if (xEventGroupGetBits(egButtonEvents) & BUTTON1_SHORT){
					xEventGroupSetBits(egButtonEvents, Start_Leibniz);
					xEventGroupClearBits(egButtonEvents, BUTTON1_SHORT);
				}
				if (xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT){
					xEventGroupClearBits(egButtonEvents, Start_Leibniz);
					xEventGroupClearBits(egButtonEvents, BUTTON2_SHORT);
				}
				if (counter == 20 || counter == 40 || counter == 60){
					mode = MODE_Display;
					if (counter >= 60){ // Rückesetzen des Timers
						counter = 0;
					}
				}
				if (counter == 50){ // Bei 500ms PI ausgeben
					mode = MODE_PI;
				}
				counter = counter+1; // Timer raufzählen
			
				break;
			
			case MODE_PI: 
				Zustand = 0;
				if (xEventGroupGetBits(egButtonEvents) & Start_Leibniz){
					xEventGroupClearBits(egButtonEvents, Start_Leibniz);
					xEventGroupClearBits(egButtonEvents, Start_Kette);
					Zustand = 0;
				}
				else {
					xEventGroupClearBits(egButtonEvents, Start_Leibniz);
					xEventGroupClearBits(egButtonEvents, Start_Kette);
					Zustand = 1;
				}
				xEventGroupWaitBits(egButtonEvents, PI_Ready, false, true, portMAX_DELAY);
				Pi_Ausgabe = pi_calc;
				
				if (Zustand == 0){
					xEventGroupSetBits(egButtonEvents, Start_Leibniz);
					xEventGroupClearBits(egButtonEvents, PI_Ready);
				}
				else{
					xEventGroupSetBits(egButtonEvents, Start_Kette);
					xEventGroupClearBits(egButtonEvents, PI_Ready);
				}
				mode = MODE_IDLE;
				break;
			
			case MODE_Display:
				if(xEventGroupGetBits(egButtonEvents) & Start_Leibniz){ // Bildschirmausgabe für die Berechnung mit Leibniz
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Calculate with Leibniz");
					sprintf(Pi_String, "%1.5f", Pi_Ausgabe);
					vDisplayWriteStringAtPos(1,0,"Value: %s", Pi_String);
					vDisplayWriteStringAtPos(2,0,"Time:");
					vDisplayWriteStringAtPos(3,0,"cal|stp|rst|sw");
				}
				if (xEventGroupGetBits(egButtonEvents) & Start_Kette){ // Bildschirmausgabe für die Berechnung mit Wallis
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0,"Calculate with Wallis");
					//sprintf(Pi_String, "%1.5f", Pi_Ausgabe);
					vDisplayWriteStringAtPos(1,0,"Value: %s", Pi_Ausgabe);
					vDisplayWriteStringAtPos(2,0,"Time:");
					vDisplayWriteStringAtPos(3,0,"cal|stp|rst|sw");
				}
				else {
					vDisplayClear();
					vDisplayWriteStringAtPos(0,0, "Press Start to calc");
					//vDisplayWriteStringAtPos(1,0, "1");
					//vDisplayWriteStringAtPos(2,0, "1");
					vDisplayWriteStringAtPos(3,0, "cal|stp|rst|sw");
				}
				mode = MODE_IDLE;
				break;
				default: 
				mode = MODE_IDLE;
				break;
		}
		vTaskDelay(100);
	}
}
	void leibniztask(void *pvParameters) {
		float piviertel = 1.0;
			for(;;){
				if ((xEventGroupGetBits(egButtonEvents) & Start_Leibniz)) {
				uint32_t n = 3;
				piviertel = piviertel -(1.0/n) + (1.0/(n+2));
				n = n+4;
				pi_calc = piviertel * 4;
				
			
		}
		else{
//			xEventGroupSetBits(egButtonEvents, PI_Ready); // blockiert Ablauf
			xEventGroupWaitBits(egButtonEvents, Start_Leibniz, false, true, portMAX_DELAY);
		}
			}
	}	
	
	void KetteTask(void *pvParameters){
		float piKette = 2.0;
		float piRechnen = 1.0;
		
			for(;;){
				if (xEventGroupGetBits(egButtonEvents) & Start_Kette){
				piRechnen = piRechnen * (piKette/(piKette - 1.0));
				piRechnen = piRechnen * (piKette/(piRechnen + 1.0));
				piKette = piKette + 2;
				pi_calc = piRechnen * 2;
			
		}
		else{
//			xEventGroupSetBits(egButtonEvents, PI_Ready); // blockiert Ablauf
			xEventGroupWaitBits(egButtonEvents, Start_Kette, false, true, portMAX_DELAY);
		}
	}
	}

	void ButtonTask(void* pvParameters) {
		egButtonEvents = xEventGroupCreate(); // Eventgroup definieren
		initButtons();
		for(;;) {
			updateButtons();
			if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
				xEventGroupSetBits(egButtonEvents, BUTTON1_SHORT);
		
			}
			if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
				xEventGroupSetBits(egButtonEvents, BUTTON2_SHORT);
			}
			if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
				xEventGroupSetBits(egButtonEvents, BUTTON3_SHORT);
			}
			if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
				xEventGroupSetBits(egButtonEvents, BUTTON4_SHORT);
			}
			
			vTaskDelay(10);
		}
	}
	