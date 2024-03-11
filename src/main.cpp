#include <Wire.h> // Libreria para manejar el i2c
#include <DS3231.h> // Libreria para manejar la pantallita
#include <SolarCalculator.h> // libreria para calcular el azimut y la levaciòn
#include <TimeLib.h> // para manejar las fechas y horas
#include <Adafruit_GFX.h> //Liberia para gráficos
#include <Adafruit_SSD1306.h> //Liberia para Oleds monocromáticos basados ​​en controladores SSD1306
#include <AsyncTaskLib.h>

#define GMT_OFFSET -6  // Offset from UTC time  Ex Mexico = UTC-6

#define OLED_RESET 13 
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 32)
  #error("Wrong High, cambie en la librería de Adafruit_SSD1306.h!");
#endif


//Pinout  for 28BYJ-48 stepper motors
//Azimut Motor
#define M1_P1  6
#define M1_P2  7
#define M1_P3  8
#define M1_P4  9

//Elevation Motor
#define M2_P1  0
#define M2_P2  1
#define M2_P3  4
#define M2_P4  5

#define PUSH_BUTTON  10
#define LASER  11




// Funciones
void clearDisplay();
void getSolarPosition();
void stepperDoOneStep(int pinM, int *Steps,boolean *direction, int *currentPosition);
void executeUpdate();

AsyncTask executeUpdateTask(5000, executeUpdate );

DS3231 Clock;

double az, el;
double latitude = 19.244465; // CHoza
double longitude = -99.09375; // CHoza

int currentAngleAzimut = 0;
int currentAngleElevation = 0;

bool Century   = false; // ??
bool h12 = false;
bool PM = false;

//left steps in every solar position step
int stepsleftAzimut =0;
int stepsleftElevation = 0;  

//Matrix Stepper Counter
int stepsAzimutCounter = 0;  
int stepsElevationCounter = 0;

//Motor direction
boolean directionAzimut = 0;
boolean directionElevation = 0;

//Targets to reflect sun light in MP (Motor position)
int azimutTarget;
int elevationTarget;


//"GUI" variables
byte setuppStage = 0;
byte lastButtonStage = 0;
boolean setupp = true;
bool swapAzHour = true; //Visualization

int pasoDerecha [ 8 ][ 4 ] =
   {  {1, 0, 0, 0},
      {1, 1, 0, 0},
      {0, 1, 0, 0},
      {0, 1, 1, 0},
      {0, 0, 1, 0},
      {0, 0, 1, 1},
      {0, 0, 0, 1},
      {1, 0, 0, 1}
   };

int pasoIzquierda [ 8 ][ 4 ] =
   {  {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 1, 0},
      {0, 1, 1, 0},
      {0, 1, 0, 0},
      {1, 1, 0, 0},
      {1, 0, 0, 0},
      {1, 0, 0, 1}
   };


void setup()
{ 
  pinMode(M1_P1, OUTPUT);
  pinMode(M1_P2, OUTPUT);
  pinMode(M1_P3, OUTPUT);
  pinMode(M1_P4, OUTPUT);
  pinMode(M2_P1, OUTPUT);
  pinMode(M2_P2, OUTPUT);
  pinMode(M2_P3, OUTPUT);
  pinMode(M2_P4, OUTPUT);
  pinMode(LASER, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT);
   
  Serial.begin(9600);
  Wire.begin();
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  clearDisplay();  
  display.setCursor(30,0); display.print("ELIOSTAT");
  display.setCursor(10,15);display.print("Push Joy to Setup");
  display.display();

  executeUpdateTask.Start();

}

void clearDisplay(){
  display.clearDisplay(); //Borra el buffer
  display.setTextSize(1); //Establece el tamaño de fuente, admite tamaños de 1 a 8
  display.setTextColor(WHITE); //Establece el color 
}

void getSolarPosition(){ 

  byte year,month,date,hour,minute,second;

  year  = Clock.getYear();
  month = Clock.getMonth(Century);
  date = Clock.getDate();
  hour  = Clock.getHour(h12, PM);
  minute = Clock.getMinute();
  second = Clock.getSecond();

  setTime((hour -(GMT_OFFSET) ) % 24, minute, second, date, month, 2000 + year); // Mas 6 horas para la zona horaria GMT-6 para Mexico
  time_t utc = now();
  // Calculate the solar position, in degrees
  calcHorizontalCoordinates(utc, latitude, longitude, az, el);
  az = az > 180 ? az-360: az ;
  //Read Real Time Clock
  Serial.print(year, DEC);
  Serial.print("-");
  Serial.print(month, DEC);
  Serial.print("-");
  Serial.print(date, DEC);
  Serial.print(" ");
  Serial.print(hour, DEC); //24-hr
  Serial.print(":");
  Serial.print(minute, DEC);
  Serial.print(":");
  Serial.println(second, DEC);
  // Print results
  Serial.print(F("Az: "));
  Serial.print(az);
  Serial.print(F("°  El: "));
  Serial.print(el);
  Serial.println(F("°"));

}

void stepperDoOneStep(int pinM, int *Steps,boolean *direction, int *currentPosition){  
  
  if(*direction){
    (*Steps)++;
    (*currentPosition)++;
  }else{
      (*Steps)--;
      (*currentPosition)--;
  }
  *Steps = ( (*Steps) + 8 ) % 8 ;

  
  /*//if(pinM == M2_P1){
     // Azimut giro positivo Izquierda
    digitalWrite( pinM+0, pasoIzquierda[*Steps][0] );
    digitalWrite( pinM+1, pasoIzquierda[*Steps][1] );
    digitalWrite( pinM+2 == 2 ? 4: pinM+2, pasoIzquierda[*Steps][2] );
    digitalWrite( pinM+3 == 3 ? 5: pinM+3, pasoIzquierda[*Steps][3] );
  }else*/{
    // Azimut giro positivo derecha
    digitalWrite( pinM+0, pasoDerecha[*Steps][0] );
    digitalWrite( pinM+1, pasoDerecha[*Steps][1] );
    digitalWrite( pinM+2 == 2 ? 4: pinM+2, pasoDerecha[*Steps][2] );
    digitalWrite( pinM+3 == 3 ? 5: pinM+3, pasoDerecha[*Steps][3] );
  }
    
  
}

//Correcta
void  getStepsNDirectionByAngles(float angle1,float angle2,int *currentAngle, boolean *Direction, int *pasos ){
  float  angleA = 0;
  float  angleB = 0;
  float  target = 0;
  int targetInt = 0;
  
  // Identificar el menor
  if(angle1 < angle2){
     angleA = angle1;
     angleB = angle2;   
  }else{
     angleA = angle2;
     angleB = angle1;
  }

  target = angleA / 2 + angleB / 2 - 90;  // Calculate Mirror angle target
  //target  = target < 0 ? 360 + target : target; // Only positive values. 0 - 360 

  Serial.print("Angle A: ");Serial.println(angleA);
  Serial.print("Angle B: ");Serial.println(angleB);
  Serial.print("Target: ");Serial.println(target);

  /*if( ( angleB - angleA ) >= 180){ //Validation for reflective side  points to target
    target += 180;
    Serial.print("Target reflective side fix  +180.   Updated Target: ");Serial.println(target);
  }*/
 
  targetInt = (int)(target *( 4096 / 360)); // target in motor position

  Serial.print("Target in motor position(MP): ");Serial.println(targetInt);

  Serial.print("Current MP: ");Serial.println(*currentAngle);
  targetInt = targetInt - *currentAngle;

  Serial.print("Steps from current MP: ");Serial.println(targetInt);

  if(targetInt > 0){ //giro a la derecha
    *Direction = true;
    Serial.println("Positive turn ");
  }
  else{
    Serial.println("Negative turn");
    *Direction = false;
    targetInt = -targetInt;
   }

  *pasos = targetInt;
  
}

void pantalla(){
  clearDisplay();
  
  display.setCursor(0,0);
  display.print(Clock.getYear());
  display.print("/");
  display.print(Clock.getMonth(Century), DEC);
  display.print("/");
  display.print(Clock.getDate(), DEC);
  display.setCursor(70,0);
  display.print(Clock.getHour(h12, PM), DEC); //24-hr
  display.print(":");
  display.print(Clock.getMinute(), DEC);
  display.print(":");
  display.println(Clock.getSecond(), DEC);
  

  display.setCursor(0,10);
  display.print("Az: ");
  display.print(az); 
  display.print(" El: ");
  display.print(el);

  display.setCursor(0,20);
  display.print("T-Az: ");
  display.print(azimutTarget); 
  display.print(" T-El: ");
  display.print(elevationTarget);

  display.display();

}





void loop(){ 
  if(digitalRead(PUSH_BUTTON) && lastButtonStage == 0){
    setuppStage++; 

    if(setuppStage == 1){
      clearDisplay();  
      display.setTextSize(2);
      display.setCursor(20,0);  
      display.print("Azimut 0");
      display.setCursor(25,15);  
      display.print("...");
      display.display();
      display.setTextSize(1);
      digitalWrite(LASER,1);
      setupp =true;
    }else if(setuppStage == 2){
      currentAngleAzimut = -1024; // -90 grados  para que el laser apunte correctamente
      clearDisplay();  
      display.setTextSize(2);
      display.setCursor(10,0);  
      display.print("Elevation 0");
      display.setCursor(25,15);  
      display.print("...");
      display.display();
      display.setTextSize(1);
      digitalWrite(LASER,1); 
    }else if(setuppStage == 3){
      currentAngleElevation = -1024;
      clearDisplay();  
      //display.setTextSize(2);
      display.setCursor(10,0);  
      display.print("Azimut ");
      display.setCursor(0,15);  
      display.print("Set point");
      display.display();
      display.setTextSize(1);
      digitalWrite(LASER,1); 
    }else if(setuppStage == 4){
      //currentAngleAzimut -= 1024;
       //azimutTarget = currentAngleAzimut < 0 ? 4096 + currentAngleAzimut: currentAngleAzimut;
      azimutTarget = currentAngleAzimut + 1024;
       clearDisplay();  
       display.setTextSize(2);
       display.setCursor(10,0);  
       display.print("Elevation");
       display.setCursor(0,15);  
       display.print("Set point");
       display.display();
       display.setTextSize(1);
       digitalWrite(LASER,1); 
    }else if(setuppStage == 5){
      //currentAngleElevation+= 1024;
      //elevationTarget = currentAngleElevation < 0 ? 4096 + currentAngleElevation: currentAngleElevation;
      Serial.println("////////////////////////");
      Serial.println("1 "); Serial.println(elevationTarget);
      elevationTarget = currentAngleElevation + 1024;
      Serial.println("2 "); Serial.println(elevationTarget);
    /*  int y;
      if(currentAngleAzimut < 0){
        if(elevationTarget > 1024 || elevationTarget < -1024){
          y = -1024;
          Serial.println("3 "); Serial.println(y);
        }else if(elevationTarget > -1024 || elevationTarget < 1024){
          y = 1024;
          Serial.println("4 "); Serial.println(y);
        }
      }else{
        if(elevationTarget > 1024 || elevationTarget < -1024){
          y = 1024;
          Serial.println("5 "); Serial.println(y);
        }else if(elevationTarget > -1024 || elevationTarget < 1024){
          y = -1024;
          Serial.println("6 "); Serial.println(y);
        }
      }*/
      Serial.println("7 "); Serial.println(azimutTarget);
      //azimutTarget = currentAngleAzimut + y;
      Serial.println("8 "); Serial.println(azimutTarget);
      //azimutTarget = azimutTarget < 0 ? 4096 + azimutTarget: azimutTarget;
      Serial.println("9 "); Serial.println(azimutTarget);
      Serial.println("/////////////////// ");
      
      
      elevationTarget *= (0.0878); //(360 /4096); To degrees, hard coded because type limits
      azimutTarget *= (0.0878); //(360 /4096); To degrees, hard coded because type limits
      clearDisplay();
      setupp = false ;  
      display.setCursor(10,0);  
      display.print("Normal");
      display.setCursor(25,15);  
      display.print("operation");
      display.display();
      display.setTextSize(1);
      digitalWrite(LASER,0); 
    }else if(setuppStage > 5){
      clearDisplay();  
      display.setCursor(30,0);  
      display.print("ELIOSTAT");
      display.setCursor(10,15);  
      display.print("Push Joy to Setup");
      display.display();
      setuppStage = 0  ; 
    }   
    lastButtonStage = 1;
  }

  if(digitalRead(PUSH_BUTTON)){
    lastButtonStage = 1;
  }else{
   lastButtonStage = 0; 
   }
  

  
  if(setupp)
  {
    int valA = analogRead(A0); 
    int valE = analogRead(A1); 
    int velAzimut = 0;
    int velElevation = 0;

    // A1 = eje Y  atras 1023 enfrente 0
    // A0 = eje X derecha 0 izquierda 1024 
    boolean direccionJoyAzminut = valA >557 ? false: true; 
    boolean azimutJoyActive = valA >557 || valA < 467  ? true: false; // Izquierda | Derecha

    boolean direccionJoyElevation = valE >557 ? false: true; 
    boolean elevationJoyActive = valE >557 || valE < 467  ? true: false; // Izquierda | Derecha
    
    if(azimutJoyActive && (setuppStage == 1 || setuppStage == 3 ) ){    
      if(direccionJoyAzminut){
        velAzimut = 467-valA;
      }else
      {
        velAzimut = valA -557;
        }
      stepperDoOneStep(M1_P1,&stepsAzimutCounter,&direccionJoyAzminut,&currentAngleAzimut) ;     // Avanza un paso
      delay (46- (velAzimut)/10 ) ;
    }

    if(elevationJoyActive && ( setuppStage == 2 || setuppStage == 4 )  ){    
      if(direccionJoyElevation){
        velElevation = 467-valE;
      }else
      {
        velElevation = valE -557;
        }
      stepperDoOneStep(M2_P1,&stepsElevationCounter,&direccionJoyElevation,&currentAngleElevation) ;     // Avanza un paso
      delay (46- (velElevation)/10 ) ;
    }
  
  }
  else
  {
    executeUpdateTask.Update(executeUpdateTask);
  }

  delay(1);
}




void executeUpdate(){
  Serial.println("Init process : ");
  digitalWrite(13,1);
  getSolarPosition();
  pantalla();

  Serial.println("-----Azimut Calc-----");    
  getStepsNDirectionByAngles( az, azimutTarget, &currentAngleAzimut, &directionAzimut, &stepsleftAzimut );
  Serial.println("-----Elevation Calc -----");
  getStepsNDirectionByAngles( el, elevationTarget, &currentAngleElevation, &directionElevation, &stepsleftElevation );

  Serial.println("Executing Azimut movement... ");

  while(stepsleftAzimut>0){ 
     stepperDoOneStep(M1_P1,&stepsAzimutCounter,&directionAzimut,&currentAngleAzimut);
     stepsleftAzimut--;
     delay (5) ;
  }
  
  Serial.println("Executing  Elevation movement... ");
  while(stepsleftElevation>0){ 
     stepperDoOneStep(M2_P1,&stepsElevationCounter, &directionElevation, &currentAngleElevation);
     stepsleftElevation--;
     delay (5) ;
  }
  
  Serial.println("End Process ... ");
  digitalWrite(13,0);  
}

   