
 /* Created 09/2019
 * By Sebastian Gonzalez Dixon 
 */


/* Es esta versión 0.1 crea para controlas las bobinas por grupos.
 *  De un grupo de N bobinas solo enciende 1 a la vez.
 *  Pueden haber varios grupos de N bobinas independientes.
 */

//// VARIABLES MODIFICABLES ///////

// El siguiente array determina los pines que se pueden utilizar, para controlar el circuito impreso.

const int hallSensPin[50] = {2,  3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51};

// Los siguientes Arrays determinan los segmentos de las probabilidades sobre el 100% (percentCut)y los tiempos (holdTimeOne) de cada segmento.
// La variable percentOne determina la primera probabilidad, que es el porcentaje de chance que se ejecute el tiempo holdTimeOne
// La variable percentTwo determina la segunda probabilidad, que es el porcentaje de chance que se ejecute el tiempo holdTimeTwo.
// La tercera probabilidad es la diferencia para completar el 100% y se calcula como 100 - (percentOne + percentTwo). Es el chance que se ejecute el tiempo holdTimeThree.

int percentOne[50] =       {60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60,  60};
int percentTwo[50] =       {30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30,  30};

// Los tiempos que permanecen encendida cada bobina o grupo se establecen a continuación, y estan definidos en segundos enteros.

int holdTimeOne[50]   =    {6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6};
int holdTimeTwo[50]   =    {12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12};
int holdTimeThree[50] =    {18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18};

// //Los tiempos que permanecen apagada cada bobina o grupo se establecen a continuación, y estan definidos en segundos enteros.


int holdTimeOffOne[50]   =  {6,  6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6};
int holdTimeOffTwo[50]   =  {12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12};
int holdTimeOffThree[50] =  {18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,  18};


// La multiplicación coilsPerGroup x numberOfGroups determina el numero de salidas del arduino que serán utilizadas y su resultado debe ser menor a 50.

int coilsPerGroup = 1;
int numberOfGroups = 10;

int outputNumber = coilsPerGroup * numberOfGroups;

////////  VARIABLES DE USO INTERNO

long delayValue[50];
bool state[50];

//////// INICIALIZACION DE VARIABLES

void setup() {

  for(int i = 0; i < outputNumber; i++){

    pinMode(hallSensPin[i], OUTPUT);
    delay(10);

    delayValue[i] = 1000; //  Valor inicial, tiempo muerto al encender
    state[i] = false;
    
  }
  
}

//////// CODIGO PRINCIPAL DE EJECUCIÓN

void loop() {

////// PENDULUMS

  for(int i = 0; i < numberOfGroups; i++){
    
    if(millis() >= delayValue[i]){
      //Serial.println("P1");
      int randomValue = random(100);
      if(randomValue < percentOne[i]) {
        if(state[i] == false){
          delayValue[i] = millis()+holdTimeOne[i]*1000;
        }
        else{
          delayValue[i] = millis()+holdTimeOffOne[i]*1000;
        }
      }
      else if(randomValue >percentOne[i] && randomValue < (percentOne[i]+percentTwo[i])){
        if(state[i] == false){
          delayValue[i] = millis()+holdTimeTwo[i]*1000;
        }
        else{
          delayValue[i] = millis()+holdTimeOffTwo[i]*1000;
        }
      }
      else if(randomValue > (percentOne[i]+percentTwo[i])){
        if(state[i] == false){
          delayValue[i] = millis()+holdTimeThree[i]*1000;
        }
        else{
          delayValue[i] = millis()+holdTimeOffThree[i]*1000;
        }
      }

      int activeCoil = random(coilsPerGroup);
      
      
      if(state[i] == true){
        for(int j = 0; j < coilsPerGroup; j++){
          digitalWrite(hallSensPin[i*coilsPerGroup + j], LOW); 
          delay(5); // Pausa de 5 milisegundos para esperar que el pin se apague
          state[i] = false;
        }
      }
      else if(state[i] == false){
          digitalWrite(hallSensPin[i*coilsPerGroup + activeCoil], HIGH);
          delay(5); // Pausa de 5 milisegundos para esperar que el pin se encienda
          state[i] = true;
      }
    }
  }

}


////// FIN
