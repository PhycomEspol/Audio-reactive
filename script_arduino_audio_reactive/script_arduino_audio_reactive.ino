#include <FastLED.h>
//La cantidad de LEDs en la configuración
#define NUM_LEDS 60
//El pin que controla los LEDs
#define LED_PIN 6
//El pin en el que leemos los valores del sensor
#define ANALOG_READ 0
// Valor bajo y valor máximo del micrófono confirmado
#define MIC_LOW 0.0
#define MIC_HIGH 200.0
//¿Cuántos valores anteriores del sensor afectan el promedio operativo?
#define AVGLEN 5
//Cuántos valores anteriores del sensor deciden si estamos en un pico/ALTO (por ejemplo, en una canción)
#define LONG_SECTOR 20
#define HIGH 3
#define NORMAL 2
//¿Cuánto tiempo mantenemos el sonido de "promedio actual", antes de reiniciar la medición?
#define MSECS 30 * 1000
#define CYCLES MSECS / DELAY
/*A veces las lecturas son incorrectas o extrañas. ¿Cuánto se permite una lectura?
desviarse del promedio para no ser descartado? **/
#define DEV_THRESH 0.8
//Retraso del bucle de Arduino
#define DELAY 1
float fscale( float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve);
void insert(int val, int *avgs, int len);
int compute_average(int *avgs, int len);
void visualize_music();
//Cuantos LEDs mostramos
int curshow = NUM_LEDS;
/*Todavía no se usa realmente. Pensado para poder cambiar entre sonido reactivo
modo, y pulsación de degradado general/color estático*/
int mode = 0;
//Mostrando diferentes colores según el modo.
int songmode = NORMAL;
//Medida de sonido promedio últimos CICLOS
unsigned long song_avg;
//La cantidad de iteraciones desde que se restableció el song_avg
int iter = 0;
// La velocidad a la que los LED se vuelven negros si no se vuelven a encender
float fade_scale = 1.2;
// Matriz de leds
CRGB leds[NUM_LEDS];
/* Promedio de sonido corto utilizado para "normalizar" los valores de entrada.
Usamos el promedio corto en lugar de usar la entrada del sensor directamente */
int avgs[AVGLEN] = {-1};
// Promedio de sonido más largo
int long_avg[LONG_SECTOR] = {-1};
//Hacer un seguimiento de la frecuencia y el tiempo que llegamos a un modo determinado
struct time_keeping {
  unsigned long times_start;
  short times;
};
//Cuánto aumentar o disminuir cada color en cada ciclo
struct color {
  int r;
  int g;
  int b;
};
struct time_keeping high;
struct color Color; 
void setup() {
  Serial.begin(9600);
  // Configure todas las luces para asegurarse de que todas funcionen como se esperaba
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++) 
    leds[i] = CRGB(0, 0, 255);
  FastLED.show(); 
  delay(1000);  
  //promedio de arranque con algunos valores bajos
  for (int i = 0; i < AVGLEN; i++) {  
    insert(250, avgs, AVGLEN);
  }
  //Valores iniciales
  high.times = 0;
  high.times_start = millis();
  Color.r = 0;  
  Color.g = 0;
  Color.b = 1;
}
/*Con esto podemos cambiar el modo si queremos implementar un general
característica de la lámpara, con, por ejemplo, pulsaciones generales. tal vez si el
el sonido es bajo por un tiempo? */
void loop() {
  switch(mode) {
    case 0:
      visualize_music();
      break;
    default:
      break;
  }
    delay(DELAY);       
    // retraso entre lecturas para la estabilidad
}
/**Función para comprobar si la lámpara debe entrar en modo ALTO,
o volver a NORMAL si ya está en ALTO. Si los sensores reportan valores
que son superiores a 1,1 veces los valores medios, y esto ha ocurrido
más de 30 veces los últimos milisegundos, entrará en modo ALTO.
TODO: No está muy bien escrito, elimine los valores codificados y hágalo más
reutilizable y configurable. */

void check_high(int avg) {
  if (avg > (song_avg/iter * 1.1))  {
    if (high.times != 0) {
      if (millis() - high.times_start > 200.0) {
        high.times = 0;
        songmode = NORMAL;
      } else {
        high.times_start = millis();  
        high.times++; 
      }
    } else {
      high.times++;
      high.times_start = millis();

    }
  }
  if (high.times > 30 && millis() - high.times_start < 50.0)
    songmode = HIGH;
  else if (millis() - high.times_start > 200) {
    high.times = 0;
    songmode = NORMAL;
  }
}
//Función principal para visualizar los sonidos en la lámpara
void visualize_music() {
  int sensor_value, mapped, avg, longavg;
  //Valor real del sensor
  sensor_value = analogRead(ANALOG_READ);
  //Si es 0, descartar inmediatamente. Probablemente no esté bien y ahorre CPU.
  if (sensor_value == 0)
    return;
  // Descartar las lecturas que se desvían demasiado del promedio anterior.
  mapped = (float)fscale(MIC_LOW, MIC_HIGH, MIC_LOW, (float)MIC_HIGH, (float)sensor_value, 2.0);
  avg = compute_average(avgs, AVGLEN);
  if (((avg - mapped) > avg*DEV_THRESH)) //|| ((avg - mapped) < -avg*DEV_THRESH))
    return;
  //Insertar nuevo promedio valores
  insert(mapped, avgs, AVGLEN); 
  insert(avg, long_avg, LONG_SECTOR); 
  // Calcular el valor del sensor "promedio de la canción"
  song_avg += avg;
  iter++;
  if (iter > CYCLES) {  
    song_avg = song_avg / iter;
    iter = 1;
  }
  longavg = compute_average(long_avg, LONG_SECTOR);
  //Comprobamos si entramos en modo ALTO
  check_high(longavg);
  if (songmode == HIGH) {
    fade_scale = 3;
    Color.r = 5;
    Color.g = 3;
    Color.b = -1;
  }
  else if (songmode == NORMAL) {
    fade_scale = 2;
    Color.r = -1;
    Color.b = 2;
    Color.g = 1;
  }
  //Decide cuántos de los LED se encenderán
  curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)avg, -1);
  /*Configurar los diferentes leds. Control de valores demasiado altos y demasiado bajos.
           Algo divertido para probar: no tenga en cuenta el desbordamiento en una dirección,
     ¡aparecen algunos efectos de luz interesantes! */
  for (int i = 0; i < NUM_LEDS; i++) 
    //The leds we want to show
    if (i < curshow) {
      if (leds[i].r + Color.r > 255)
        leds[i].r = 255;
      else if (leds[i].r + Color.r < 0)
        leds[i].r = 0;
      else
        leds[i].r = leds[i].r + Color.r;
          
      if (leds[i].g + Color.g > 255)
        leds[i].g = 255;
      else if (leds[i].g + Color.g < 0)
        leds[i].g = 0;
      else 
        leds[i].g = leds[i].g + Color.g;

      if (leds[i].b + Color.b > 255)
        leds[i].b = 255;
      else if (leds[i].b + Color.b < 0)
        leds[i].b = 0;
      else 
        leds[i].b = leds[i].b + Color.b;  
      
    //Todos los demás LED comienzan su viaje de desvanecimiento hacia la eventual oscuridad total
    } else {
      leds[i] = CRGB(leds[i].r/fade_scale, leds[i].g/fade_scale, leds[i].b/fade_scale);
    }
  FastLED.show(); 
}
// Calcular el promedio de una matriz int, dado el puntero de inicio y la longitud
int compute_average(int *avgs, int len) {
  int sum = 0;
  for (int i = 0; i < len; i++)
    sum += avgs[i];

  return (int)(sum / len);

}

//Insertar un valor en una matriz y desplazarlo hacia abajo eliminando
// el primer valor si la matriz ya está llena
void insert(int val, int *avgs, int len) {
  for (int i = 0; i < len; i++) {
    if (avgs[i] == -1) {
      avgs[i] = val;
      return;
    }  
  }

  for (int i = 1; i < len; i++) {
    avgs[i - 1] = avgs[i];
  }
  avgs[len - 1] = val;
}

//Función importada de la web de arduino.
//Básicamente mapa, pero con una curva en la escala (puede ser no uniforme).
float fscale( float originalMin, float originalMax, float newBegin, float
    newEnd, float inputValue, float curve){

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;


  // parámetro de la curva de condición
  // rango limite

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ;
  curve = pow(10, curve); 

  // Comprobar valores de entrada fuera de rango
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Referencia cero los valores
  OriginalRange = originalMax - originalMin;
  if (newEnd > newBegin){ 
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd; 
    invFlag = 1;
  }
  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   
  if (originalMin > originalMax ) {
    return 0;
  }
  if (invFlag == 0){  
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else    
  {   
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange); 
  }
  return rangedValue;
}
