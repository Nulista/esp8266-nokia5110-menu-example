/*
 Testprogramma voor het werken met menu op 5110 en een rotary encoder : 20-02-2022
 Programma maakt gebruik van een i2C aansturing van het 5110 LCD
 
-- Omschrijving van programma Nulista Menu --
-----------------------------------------------
||	menuVal 		bevat het uit te voeren loop programma dat na de standaard loop-functie wordt uitgevoerd
||	menuTxtSrt  	bevat de start positie van text in array menuTxt[]
||	num_Menus 	bevat aantal te lezen menu items vanaf menuTxtSrt
||	Menu_Titel 	bevat de titel van het uit te voeren menu 
||	rotaryEncoder.setBoundaries(0, 19, true); //how many pieces 1 to 10; also go from 10 to 1
||	rotaryEncoder.setEncoderValue(0);  
||  	slider       als deze bit is gezet wordt de slider rechts weergegeven aut aangepast aan aantal weer tegeven menus	
De voorgaande variabelen bepalen bepalen welke menu Items er worden weergegeven

De schets gebruikt 285369 bytes (27%)  programma-opslagruimte. Maximum is 1044464 bytes.
Globale variabelen gebruiken 31720 bytes (38%) van het dynamisch geheugen. Resteren 50200 bytes voor lokale variabelen. Maximum is 81920 bytes.

*/
//-----------------------------------------------------------------------------------------------------------------------

#include "AiEsp32RotaryEncoder.h"   // https://github.com/igorantolic/ai-esp32-rotary-encoder
#if defined(ESP8266)
#define ROTARY_ENCODER_A_PIN D5
#define ROTARY_ENCODER_B_PIN D6
#define ROTARY_ENCODER_BUTTON_PIN D7
#else
#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 21
#define ROTARY_ENCODER_BUTTON_PIN 25
#endif
#define ROTARY_ENCODER_STEPS 2  // 4 kan ook
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);

//------------------
#include <Adafruit_GFX.h>
#include <PCF8574_PCD8544.h>          // https://github.com/maxint-rd/I2C-PCF8574-PCD8544-Nokia-5110-LCD
#include <U8g2_for_Adafruit_GFX.h>    // https://github.com/olikraus/U8g2_for_Adafruit_GFX

PCF8574_PCD8544 display = PCF8574_PCD8544(0x20, 4, 3, 2, 1, 0); // Nieuwe ESP8266 print
#define Backlight 5
U8G2_FOR_ADAFRUIT_GFX u8g2;
//-----------------------------------------------------------------------------------------------------------------------

char *menuTxt[] = {"Temperatuur","Energie zon","Teller","Battery","Backlight","Contrast","Optie menu","Graph","Help","Reset Teller","Optie 1", "Slider", "Optie 3"};

uint32_t Prgr_Back = 0; 		// terug naar hoofd programma
uint32_t Sec_Tel = 0;           // Sec_Teller

String  Menu_Titel = "Menu";
int 	teller 		= 0;
int 	menuTxtSrt	= 0;
int 	num_Menus	=10;
int 	menuLine;
int 	menuStart;
int 	numScrLines = 4;     	// aantal menu regels op scherm (ifv gekozen font en indeling)
int     Sec_Aan 	= 5;        // standaard backlight 5 sec aan
volatile int encoderPos = 0;  	// volatile = direct zichtbaar in loop bij of na interrupt

uint8_t menuVal;       			// bepaald in welk menu we zitten
uint8_t Old_menuVal;   			// Voor test doeleinden in voorbeeld routines
uint8_t VoorKeur_menuVal = 1; 	// standaard menu screen 1; hier keren we steeds naar terug

bool 	refreshAll = false;     // clear screen en ververst alle gegevens
bool 	BackL = false;          // als is gezet backlight on 
bool    BL_Altijd_aan= false;	// als deze bit is gezet zal backlight altijd aan blijven
bool    Set_sec_BL = false;
bool 	slider = true;			// als slider is gezet wordt er een rechts van menu een slider weergegeven

#define begin_menu 14           // op deze Y-loc start de menu
//-----------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}
//---------------------------------------------------
void setup()
{
	Serial.begin(115200);            
	Serial.println(F("\n\n"));
	Serial.println(F("5110 Menu test"));
	//--- Encoder setup
	rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);
	rotaryEncoder.setAcceleration(0);
	rotaryEncoder.setBoundaries(0, 19, true); //how many pieces 1 to 10; also go from 10 to 1
	rotaryEncoder.setEncoderValue(0);  // we starten op de hoogste waarde-1, maar trekken er in menuafhandeling 19 af, om zo op nul te kunnen starten
	//--- Display setup
	display.begin();    
	u8g2.begin(display);                 // connect u8g2 procedures to Adafruit GFX
	
	display.digitalWrite(Backlight, HIGH); 		// zet backlight aan
	display.setContrast(58);
	display.setRotation(2);
	display.clearDisplay(); 
	//--- Introductie scherm
	display.setTextSize(1);
	display.setTextColor(BLACK);
	display.setCursor(0,20);
	display.println("Nulista Menu.");
	display.display();
	delay(2000);
	display.clearDisplay(); 
	menuVal = 2;       	// ga naar menu 2 (vb. standaard scherm)
	menuTxtSrt = 0; //start positie van text in menuTxt[]
	num_Menus = 10; // aantal te lezen menu items vanaf menuTxtSrt
	Menu_Titel = "-- Menu --";
	slider = true;
	refreshAll = true;	// eerste keer dat we een menu aanroepen
}
//----------------------------------------------------------------------------------------------------
//  springtabel voor menu programmas
// this is a typedef to the menu functions
typedef void (* MenuFuncPtr) ();
MenuFuncPtr menu_func[] = {
	menu,         //menu 0     dit is een routine waar we niets doen, wordt aangeroepen bij het afhandelen van het menu
	menu_1,       //menu 1
	menu_2,       //menu 2 = standaard menu
	menu_1,       //menu 3
	setBacklight, //menu 4 = backlight 
	setContrast,  //menu 5 = Set contrast
	optie_1,      //menu 6
	menu_1,       //menu 7
	menu_1,       //menu 8
	reset_Tel,    //menu 9
	menu_1, 	  //menu 10      // submenu
	Change_slider,//menu 11      // submenu
	menu_1,       //menu 12      // submenu
	menu_1,       //menu 13
	menu          //menu 14
	
};
//---------------------------------------------------------------------------------------------------
void loop() {
	menuStart = 0;
	//numMenus = 10;	
	//--- Spring nu naar het geslecteerde programma via menuVal ----
	menu_func[menuVal]();    // roep het bijbehorende programma van menu function from array
	
	if (rotaryEncoder.encoderChanged())
	{
		BackL = true;
		Prgr_Back = millis();
		//--------
		switch (menuVal)
		{
		case 4:       	// bij dit programma geen afhandeling van de menu
			break;
		case 5:    		// bij dit programma geen afhandeling van de menu
			break;
		default:
			//  handleMenu(byte menuItems, byte Start_menu_Txt)
			handleMenu(num_Menus,menuTxtSrt,Menu_Titel,slider);   // bij alde andere programmas gaan we de menu afhandelen als er aan de knop gedraaid wordt
			break;
		}
	}
	
	if (rotaryEncoder.isEncoderButtonClicked())
	{
		BackL = true;
		Prgr_Back = millis();
		rotary_onButtonClick();
	} 

	// Backlight wordt aangezet of uitgeet ifv bit BackL
	if (BackL) display.digitalWrite(Backlight, HIGH); // zet backlight aan
	else display.digitalWrite(Backlight, LOW); // zet backlight uit
	//--------
	if (Sec_Aan == 30) BackL = true;
	else if (millis() - Prgr_Back > Sec_Aan*1000) BackL = false;
	
	if (millis() - Prgr_Back > 10000)    //back to main screen after 10 sec
	{	
		if (menuVal != 2) refreshAll = true;
		menuVal = 2;
		rotaryEncoder.setBoundaries(0, 19, true); //we zetten de encoder klaar voor menu program
		rotaryEncoder.setEncoderValue(0);    // altijd 1 minder dan max in boundaries
		Prgr_Back = millis();		
	}
	if (millis() - Sec_Tel > 1000)    //als 1 sec voorbij, verhoog teller
	{	
		Sec_Tel = millis();
		teller = (teller + 1) % 5000; // Teller gaat max naar 5000
	}
}
//-----------------------------------------------------------------------------------------------------------------------
// handleMenu geeft menu op scherm en laat ons toe een keuze te maken.
//-----------------------------------------------------------------------------------------------------------------------
void handleMenu(byte menuItems, byte Start_menu_Txt, String MenuTitel, bool Slider)
{
	menuVal = 0;
	rotaryEncoder.setBoundaries(0, (menuItems*2)-1, true);
	//	encoderPos = 19-rotaryEncoder.readEncoder(); // om de logica van dalen in de menu-tabel, trekken we af van 19
	encoderPos = (menuItems*2)-1-rotaryEncoder.readEncoder(); // om de logica van dalen in de menu-tabel, trekken we af van 19
	display.clearDisplay(); 
	display.setCursor(8,0);  // hier start de titel
	display.println(MenuTitel);
	display.drawLine(0, 10, 80, 10, BLACK); //draws a line from (x1,x2) to (y1,y2)
	// Verander hier de font indien gewenst, maar pas ook de regels aan per scherm etc. 
	u8g2.setFont(u8g2_font_6x10_tr); //u8g2_font_profont10_tr; mooi:u8g2_font_mozart_nbp_tr;  u8g2_font_4x6_t_cyrillic); //u8g2_font_5x7_tf); //u8g2_font_squeezed_b7_tr); u8g2_font_t0_11_tf); //u8g2_font_profont11_mf; u8g2_font_nokiafc22_tf

	if(encoderPos<0) encoderPos=0;
	menuLine = encoderPos/2;
	if(menuLine>=menuItems) { menuLine=menuItems-1; rotaryEncoder.setEncoderValue(menuLine*2); }
	if(menuLine>=menuStart+numScrLines) menuStart=menuLine-numScrLines+1;
	if(menuLine<menuStart) menuStart=menuLine;
	
	for(int i=0;i<numScrLines;i++) {
		//int markerY = (i*(48/numScrLines) );
		if(i+menuStart<menuItems) {
			String item = menuTxt[i+menuStart+Start_menu_Txt];
			int x = 5;
			if (menuLine == i+ menuStart) {
				item = ">" + item + "<";
				x=0; // regels start op 0; anders op 5 , voor '>'
				//--- indien invert text, gebruik dan volgende regels
				//display.setTextColor(WHITE, BLACK); // 'inverted' text
				//display.fillRect(0, markerY, display.width()-10, 48/numScrLines, BLACK);
			}
			u8g2.setCursor(x,i*((48-begin_menu)/numScrLines)+8+begin_menu);
			u8g2.print(item); //(menuTxt[i+menuStart]);
			//display.setTextColor(BLACK,WHITE);
		}
	}
	if (Slider) drawMenuSlider(menuItems);   // tekent de slider ifv waar we zitten ifv het totale aantal menus
	display.display();
}
//-----------------------------------------------------------------------------------------------------------------------
// drawMenuSlider: tekent de menuslider ifv aantal menu's en schuift mee met geselecteerde regel/menu
//-----------------------------------------------------------------------------------------------------------------------
void drawMenuSlider(byte numMenus)
{
	int y, n = (8*numScrLines-2-5-2)*menuLine/(numMenus-1);
	//Serial.println(n);
	for(y=0; y<numScrLines*8; y++) display.drawPixel(1+81,y+begin_menu,BLACK); // disp lange lijn
	for(y=0; y<5; y++) {
		display.drawPixel(0+81,y+n+2+begin_menu,BLACK);
		display.drawPixel(2+81,y+n+2+begin_menu,BLACK); 
	}
	display.display();
}
//-----------------------------------------------------------------------------------------------------------------------
// rotary_onButtonClick: komt hier als de knop is gedrukt
//-----------------------------------------------------------------------------------------------------------------------
void rotary_onButtonClick()
{
	static unsigned long lastTimePressed = 0;
	if (millis() - lastTimePressed < 200)     // anti dender
	return;
	lastTimePressed = millis();
	refreshAll = true;
	//Serial.println(menuVal);
	switch (menuVal)
	{
	case 2:
		break;
	case 4:       	// set backligt
		menuVal = 2;
		rotaryEncoder.setBoundaries(0, 19, true); //menu 0 to 19; also go from 19 to 0
		rotaryEncoder.setEncoderValue(0);
		break;
	case 5:    		// bij dit programma geen afhandeling van de menu
		menuVal = 2;
		rotaryEncoder.setBoundaries(0, 19, true); //menu 0 to 19; also go from 19 to 0
		rotaryEncoder.setEncoderValue(0);
		break;
		
	default:
		menuVal = menuLine+menuTxtSrt;         // menuVal = aan geselecteerde lijn
		break;
	}
}
//-----------------------------------------------------------------------------------------------------------------------
// Hier volgende voorbeeld subroutines waar naar toe kan gesprongen worden vanuit main loop
//-----------------------------------------------------------------------------------------------------------------------
void optie_1(){
	//menuVal = 2;       	// ga naar menu 2 (vb. standaard scherm)
	menuTxtSrt = 10; //start positie van text in menuTxt[]
	num_Menus = 3; // aantal te lezen menu items vanaf menuTxtSrt
	Menu_Titel = "-- Optie --";
	slider = false; // geen weergave van de slider 
	handleMenu(num_Menus,menuTxtSrt,Menu_Titel,slider);   // bij alde andere programmas gaan we de menu afhandelen als er aan de knop gedraaid wordt
}

void menu() {  //  bij handleMenu gaan we naar deze routine springen en enkel het menu afhandelen....
}
//--------------------------------------------------------
void menu_1() {
	if (refreshAll){
		display.clearDisplay(); 
		display.clearDisplay(); 
		display.setTextSize(1);
		display.setTextColor(BLACK);
		display.setCursor(0,20);
		display.printf ("Menu: %d \n", menuVal);
		display.display();	
		menuTxtSrt = 0; //start positie van text in menuTxt[]
		num_Menus = 10; // aantal te lezen menu items vanaf menuTxtSrt
		Menu_Titel = "-- Menu --";		
		slider = true;
	}
}
//--------------------------------------------------------
void menu_2() {
	
	if (refreshAll){
		refreshAll = false;	
		display.clearDisplay(); 
		display.setTextSize(1);
		display.setTextColor(BLACK, WHITE); // nu worden de karakters gewist en overschreven
		display.setCursor(5,0);
		display.print("- Nulista -");
		display.writeFastHLine(10,9,55,BLACK);
		display.setCursor(0,20);
		display.printf ("Menu Item:%d \n", menuVal);
	}
	display.setCursor(0,40);
	display.printf ("teller:%d \n", teller);
	display.display();
}
//--------------------------------------------------------
void Change_slider(){
	slider = !slider;
	handleMenu(num_Menus,menuTxtSrt,Menu_Titel,slider);   // bij alde andere programmas gaan we de menu afhandelen als er aan de knop gedraaid wordt
}
//--------------------------------------------------------
void setContrast()
{
	if ( refreshAll) {         //draw static elements
		display.clearDisplay(); 
		refreshAll = false;
		rotaryEncoder.setBoundaries(90, 150, false); //how many pieces 60 to 150; do not go from 150 to 90(waarde wordt door 2 gedeeld...)
		rotaryEncoder.setEncoderValue(115);	
		display.setTextSize(1);
		display.setTextColor(BLACK, WHITE); // nu worden de karakters gewist en overschreven
		display.setCursor(2,0);
		display.print("-- Nulista --");
	}	
	encoderPos = rotaryEncoder.readEncoder();
	display.setCursor(0,20);
	display.printf ("Contrast:%d \n", encoderPos/2);
	display.setContrast(encoderPos/2);
	display.display();
}
//--------------------------------------------------------
void reset_Tel() {
	teller = 0;
	menuVal = 2;
	rotaryEncoder.setBoundaries(0, 19, true); //how many pieces 0 to 19; also go from 19 to 0
	rotaryEncoder.setEncoderValue(0);
	refreshAll = true;
	if (teller > 100) teller = 0;   // reste teller 
}
//---------------------------------------------------------
void setBacklight()
{
	if ( refreshAll) { 
		display.clearDisplay(); 
		refreshAll = false;
		rotaryEncoder.setBoundaries(0, 30, false); //how many pieces 0 to 30; not go from 30 to 0
		rotaryEncoder.setEncoderValue(Sec_Aan);
		
		display.setTextSize(1);
		display.setTextColor(BLACK, WHITE); // nu worden de karakters gewist en overschreven
		display.setCursor(5,0);
		display.print("- Nulista -");
		display.writeFastHLine(10,9,55,BLACK);
		display.setCursor(0,20);
		display.print("Backlight:");
	}
	encoderPos = rotaryEncoder.readEncoder();
	//
	display.setCursor(0,40);
	display.setTextColor(BLACK,WHITE); // 'normal' text
	if (encoderPos ==0)   display.print("Altijd uit !   ");
	if (encoderPos == 30) display.print("Altijd aan !   ");
	else                 display.printf("uit na: %2d    ",Sec_Aan);
	Sec_Aan = encoderPos;

	display.display();
}