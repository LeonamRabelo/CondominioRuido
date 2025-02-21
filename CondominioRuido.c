#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define IS_RGBW false
#define NUM_PIXELS 25
#define NUM_NUMBERS 11
#define BOTAO_A 5
#define BOTAO_B 6
#define WS2812_PIN 7
#define LED_PIN_RED 13
#define MIC_PIN 28
#define BUZZER_PIN 21
#define BUZZER_FREQ 2000 // Frequência do som em Hz

const uint limiar = 3500;     //Limiar
//const uint amostras_por_segundo = 8000; //Frequência de amostragem (8 kHz)
uint32_t volatile tempo_real = 0;
uint32_t volatile silencio_tempo = 0;
uint32_t volatile botao_tempo = 0;
bool volatile espera_ativa = false;
//bool volatile botao_espera_ativa = false;
bool volatile alerta_sonoro = false; // Controle do estado do buzzer
uint volatile numero = 0;//variavel para inicializar o numero com 0, vai ser alterada nas interrupções (volatile)
//volatile uint64_t intervalo_us;
static uint32_t volatile last_time = 0; // Armazena o tempo do último evento (em microssegundos)
uint32_t volatile tempo_espera = 0;  // Guarda o tempo que começou a espera de 30s


ssd1306_t ssd; //Inicializa a estrutura do display

//Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 20; //Intensidade do vermelho
uint8_t led_g = 0; //Intensidade do verde
uint8_t led_b = 0; //Intensidade do azul

static inline void put_pixel(uint32_t pixel_grb){
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b){
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

bool led_numeros[NUM_NUMBERS][NUM_PIXELS] = {
    //Número 0
    {
    0, 1, 1, 1, 0,      
    0, 1, 0, 1, 0, 
    0, 1, 0, 1, 0,   
    0, 1, 0, 1, 0,  
    0, 1, 1, 1, 0   
    },

    //Número 1
    {0, 1, 1, 1, 0,      
    0, 0, 1, 0, 0, 
    0, 0, 1, 0, 0,    
    0, 1, 1, 0, 0,  
    0, 0, 1, 0, 0   
    },

    //Número 2
    {0, 1, 1, 1, 0,      
    0, 1, 0, 0, 0, 
    0, 1, 1, 1, 0,    
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0   
    },

    //Número 3
    {0, 1, 1, 1, 0,      
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0,    
    0, 0, 0, 1, 0,  
    0, 1, 1, 1, 0   
    },

    //Número 4
    {0, 1, 0, 0, 0,      
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0,    
    0, 1, 0, 1, 0,     
    0, 1, 0, 1, 0   
    },

    //Número 5
    {0, 1, 1, 1, 0,      
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0,   
    0, 1, 0, 0, 0,  
    0, 1, 1, 1, 0   
    },

    //Número 6
    {0, 1, 1, 1, 0,      
    0, 1, 0, 1, 0, 
    0, 1, 1, 1, 0,    
    0, 1, 0, 0, 0,  
    0, 1, 1, 1, 0   
    },

    //Número 7
    {0, 1, 0, 0, 0,      
    0, 0, 0, 1, 0,   
    0, 1, 0, 0, 0,    
    0, 0, 0, 1, 0,  
    0, 1, 1, 1, 0  
    },

    //Número 8
    {0, 1, 1, 1, 0,      
    0, 1, 0, 1, 0, 
    0, 1, 1, 1, 0,    
    0, 1, 0, 1, 0,  
    0, 1, 1, 1, 0   
    },

    //Número 9
    {0, 1, 1, 1, 0,      
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0,    
    0, 1, 0, 1, 0,  
    0, 1, 1, 1, 0   
    },

    //APAGAR OS LEDS, representado pelo número (posição) 10
    {0, 0, 0, 0, 0,      
    0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0,    
    0, 0, 0, 0, 0,  
    0, 0, 0, 0, 0   
    }
};

void set_one_led(uint8_t r, uint8_t g, uint8_t b, int numero){
    //Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    //Define todos os LEDs com a cor especificada
    for(int i = 0; i < NUM_PIXELS; i++){
        if(led_numeros[numero][i]){ //Chama a matriz de leds com base no numero passado
            put_pixel(color); //Liga o LED com um no buffer
        }else{
            put_pixel(0);  //Desliga os LEDs com zero no buffer
        }
    }
}

void inicializar_GPIOs(){
    stdio_init_all();

    //Inicializa pino do led
    gpio_init(LED_PIN_RED);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);
    gpio_put(LED_PIN_RED, 0);

    //Inicializa pino do botao
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    //Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);  //Limpa o display
    ssd1306_send_data(&ssd);

    //Inicializa o ADC
    adc_init();
    adc_gpio_init(MIC_PIN);  //Configura GPIO28 como entrada ADC para o microfone

    //Define o intervalo entre amostras (em microsegundos)
    //intervalo_us = 1000000/amostras_por_segundo;
}

void iniciar_buzzer(){
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    //Calcula o divisor do clock para atingir a frequência desejada
    float clock_div = 125.0;  // lock divisor ajustável
    uint16_t wrap_value = (125000000 / (clock_div * BUZZER_FREQ)) - 1; //125 MHz é o clock base
    pwm_set_clkdiv(slice_num, clock_div);
    pwm_set_wrap(slice_num, wrap_value);
    pwm_set_gpio_level(BUZZER_PIN, wrap_value / 2); //50% do ciclo de trabalho (onda quadrada)

    pwm_set_enabled(slice_num, true);
    //Aguarda 1 segundo para manter o som ativo
    sleep_ms(1000);

    //Desativa o PWM corretamente
    pwm_set_gpio_level(BUZZER_PIN, 0);
    pwm_set_enabled(slice_num, false);
    //Aguarda 2 segundos antes de poder tocar novamente
    sleep_ms(2000);
}

//Desliga o buzzer
void parar_buzzer(){
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, false);
}

bool buzzer_ligado = false; // Controle para alternar os bipes

void ler_microfone(){
    // Lê o valor do microfone (canal 2)
    adc_select_input(2);
    uint16_t valor_microfone = adc_read();
    uint32_t periodo = to_ms_since_boot(get_absolute_time());

    // Se está no período de espera, impede qualquer ativação do alerta sonoro
    if(espera_ativa && (periodo - tempo_espera < 30000)){
        return; // Sai da função sem ativar alerta
    }else{
        espera_ativa = false; // Passou os 30s, pode ativar novamente
    }

    // Controle de perturbação no andar, considerando 3 segundos de tolerância
    if(valor_microfone > limiar){
        if(!alerta_sonoro){
            // Se não há alerta e o ruído começou agora, registra o tempo inicial
            tempo_real = periodo;
            alerta_sonoro = true;
        }
        
        // Se o ruído continua acima do limiar e já passaram 3 segundos desde o início do ruído
        if((periodo - tempo_real) >= 3000){
            printf("Perturbação no andar %d\nGRAVANDO...\n\n", numero);
            sleep_ms(500);
            gpio_put(LED_PIN_RED, 1);   // Ativa o LED vermelho indicando início da gravação

            // Alterna o estado do buzzer para criar bipes intercalados
            if (periodo % 1000 < 500) { // Alterna a cada 500ms
                if (!buzzer_ligado) {
                    iniciar_buzzer();
                    buzzer_ligado = true;
                }
            }else{
                if (buzzer_ligado) {
                    parar_buzzer();
                    buzzer_ligado = false;
                }
            }
        }

        silencio_tempo = 0; // Reinicia o tempo de silêncio
    }else{            
        // Se o ruído parou, inicia a contagem do silêncio
        if(alerta_sonoro){
            if(silencio_tempo == 0){
                silencio_tempo = periodo;
            }else if(periodo - silencio_tempo >= 3000){
                printf("Registrando dados de gravação e horário.\n\n");
                gpio_put(LED_PIN_RED, 0);   // Desativa o LED vermelho (representando a gravação finalizada)
                parar_buzzer(); // Garante que o buzzer seja desligado
                alerta_sonoro = false;
                buzzer_ligado = false; // Reseta o estado do buzzer
            }
        }
    }
}


//Função de interrupção com Debouncing
void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - last_time > 200){ //200 ms de debouncing
        last_time = current_time;

        // Caso o botão A seja pressionado
        if (gpio == BOTAO_A && !alerta_sonoro) {
            numero++;   //Incrementa o valor do andar (matriz de leds)
            espera_ativa = false;   //Caso mudar o sensor, desliga a espera
            set_one_led(led_r, led_g, led_b, numero);
            if (numero == 9){
                numero = 0; //Retorna ao andar 0
            }
        }

        //Caso o botão B seja pressionado, desliga o alerta e inicia os 30s de espera
        if(gpio == BOTAO_B){
            parar_buzzer();
            alerta_sonoro = false;
            espera_ativa = true;
            tempo_espera = to_ms_since_boot(get_absolute_time()); //Armazena o tempo que começou a pausa
        }
    }
}


int main(){
    inicializar_GPIOs();

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    set_one_led(led_r, led_g, led_b, 0); //Inicia a simulação monitorando o andar 0 ou terreo

    while(true){
        char buffer[20];
        ssd1306_fill(&ssd, false);  //Limpa o display antes de escrever   
        ssd1306_draw_string(&ssd, "VIGILANCIA", 30, 10);       
        sprintf(buffer, "ANDAR: %d", numero);
        ssd1306_draw_string(&ssd, buffer, 10, 30);
        ssd1306_send_data(&ssd);  //Atualiza o display    

        ler_microfone();
        
        // if(alerta_sonoro){
        // iniciar_buzzer();   //Manter o buzzer em loop acionado
        // }
    }
}
