#include <stdio.h>              //Biblioteca padrão C
#include <math.h>               //Biblioteca funções matemáticas
#include "pico/stdlib.h"        //Biblioteca padrão Pico
#include "hardware/i2c.h"       //Biblioteca I2C
#include "hardware/adc.h"       //Biblioteca ADC
#include "hardware/pio.h"       //Biblioteca PIO
#include "hardware/timer.h"     //Biblioteca Timer
#include "hardware/clocks.h"    //Biblioteca Clock
#include "hardware/pwm.h"       //Biblioteca PWM
#include "ws2812.pio.h"         //Incluir nossa maquina PIO para a matriz de leds
#include "inc/ssd1306.h"        //Incluir nossa biblioteca para controle e envio de dados para o display
#include "inc/font.h"           //Incluir nossa biblioteca que armazena os numeros, e letras para o display

#define VREF 3.3        //Tensão de referência do ADC
#define SPL_MAX 120     //SPL máximo do microfone a 3,3V
#define ADC_MAX 4095    //Resolução do ADC
#define I2C_PORT i2c1   //I2C1
#define I2C_SDA 14      //Pino SDA (dados)
#define I2C_SCL 15      //Pino SCL (clock)
#define endereco 0x3C   //Endereço do display
#define IS_RGBW false   //Maquina PIO para RGBW
#define NUM_PIXELS 25   //Quantidade de LEDs na matriz
#define NUM_NUMBERS 11  //Quantidade de numeros na matriz
#define BOTAO_A 5       //Pino do botão A
#define WS2812_PIN 7    //Pino do WS2812
#define LED_PIN_RED 13  //Pino do led vermelho
#define MIC_PIN 28      //Pino do microfone
#define BUZZER_PIN 21   //Pino do buzzer
#define BUZZER_FREQ 2000 //Frequência do som em Hz
const uint limiar = 3000;     //predefinição do Limiar -> aproximadamente 80dB SPL

uint32_t volatile tempo_real = 0;       //Guarda o tempo real do ruído
uint32_t volatile silencio_tempo = 0;   //Guarda o tempo do silencio
bool volatile alerta_sonoro = false;    //Controle do estado do buzzer
uint volatile numero = 0;               //Variável para inicializar o numero com 0, indicando a rua 0 (WS2812B)

//Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 20; //Intensidade do vermelho
uint8_t led_g = 0; //Intensidade do verde
uint8_t led_b = 0; //Intensidade do azul

//Função para ligar um LED
static inline void put_pixel(uint32_t pixel_grb){
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

//Função para converter cores RGB para um valor de 32 bits
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

//Função para envio dos dados para a matriz de leds
void set_one_led(uint8_t r, uint8_t g, uint8_t b, int numero){
    //Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    //Define todos os LEDs com a cor especificada
    for(int i = 0; i < NUM_PIXELS; i++){
        if(led_numeros[numero][i]){     //Chama a matriz de leds com base no numero passado
            put_pixel(color);           //Liga o LED com um no buffer
        }else{
            put_pixel(0);               //Desliga os LEDs com zero no buffer
        }
    }
}

ssd1306_t ssd; //Inicializa a estrutura do display

//Função para inicializar as pinagens, e toda configuração de comunicação com os periféricos
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

    //Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    //I2C IInicialização. Usando 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); //Envio do pino para I2C SDA (dados)
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); //Envio do pino para I2C SCL (clock)
    gpio_pull_up(I2C_SDA); // Pull up para data line
    gpio_pull_up(I2C_SCL); // Pull up para clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);  //Limpa o display
    ssd1306_send_data(&ssd);

    //Inicializa o ADC
    adc_init();
    adc_gpio_init(MIC_PIN);  //Configura GPIO28 como entrada ADC para o microfone

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);   //Define o pino do buzzer como PWM
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN); //Pega o slice do pino do buzzer
    //Calcula o divisor do clock para atingir a frequência desejada
    float clock_div = 125.0;  //Clock divisor ajustável
    uint16_t wrap_value = (125000000 / (clock_div * BUZZER_FREQ)) - 1; //125 MHz é o clock base
    pwm_set_clkdiv(slice_num, clock_div);   //Define o clock divisor
    pwm_set_wrap(slice_num, wrap_value);    //Define o valor de wrap
    pwm_set_gpio_level(BUZZER_PIN, wrap_value / 2); //50% do ciclo de trabalho (onda quadrada)
}

//Função para iniciar o buzzer por meio de PWM
void iniciar_buzzer(){
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, true);
    //Aguarda 1 segundo para manter o som ativo
    sleep_ms(1000);

    //Desativa o PWM
    pwm_set_enabled(slice_num, false);
    //Aguarda 2 segundos antes de poder tocar novamente
    sleep_ms(2000);
}

//Desliga o buzzer
void parar_buzzer(){
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, false);
}

//Função para converter o valor do ADC para dB SPL
float converter_adc_para_db(int adc_value) {
    float v_out = (adc_value / (float)ADC_MAX) * VREF; //Converte ADC para tensão
    float spl = 20 * log10((v_out / VREF) * SPL_MAX);  //Converte para dB SPL
    return spl;
}

bool buzzer_ligado = false; //Controle para alternar os bipes

//Função para ler o valor do microfone e controlar ações do sistema
uint16_t ler_microfone(){
    //Lê o valor do microfone (canal 2)
    adc_select_input(2);
    uint16_t valor_microfone = adc_read();
    uint32_t periodo = to_ms_since_boot(get_absolute_time());

    //Controle de perturbação no andar, considerando 3 segundos de tolerância
    if(valor_microfone > limiar){
        if(!alerta_sonoro){
            // Se não há alerta e o ruído começou agora, registra o tempo inicial
            tempo_real = periodo;
            alerta_sonoro = true;
        }
        
        //Se o ruído continua acima do limiar e já passaram 3 segundos desde o início do ruído
        if((periodo - tempo_real) >= 3000){
            char bufferRua[20];
            ssd1306_fill(&ssd, false);                          //Limpa o display antes de escrever   
            ssd1306_draw_string(&ssd, "VIGILANCIA", 30, 10);    //Escreve no display
    
            sprintf(bufferRua, "Rua %d", numero);               //Converte o numero da rua para string
            ssd1306_draw_string(&ssd, bufferRua, 10, 30);       //Escreve no display
            ssd1306_draw_string(&ssd, "ALERTA RUIDO", 10, 50);  //Escreve no display
            ssd1306_send_data(&ssd);                            //Atualiza o display    

            printf("Perturbação no andar %d\nGRAVANDO...\n\n", numero);
            sleep_ms(500);
            gpio_put(LED_PIN_RED, 1);   //Ativa o LED vermelho indicando início da gravação

            //Alterna o estado do buzzer para criar bipes intercalados
            if(periodo % 1000 < 500){ //Alterna a cada 500ms
                if (!buzzer_ligado) {
                    iniciar_buzzer();
                    buzzer_ligado = true;
                }
            }else{
                if(buzzer_ligado){
                    parar_buzzer();
                    buzzer_ligado = false;
                }
            }
        }

        silencio_tempo = 0; //Reinicia o tempo de silêncio
    }else{            
        //Se o ruído parou, inicia a contagem do silêncio
        if(alerta_sonoro){
            if(silencio_tempo == 0){
                silencio_tempo = periodo;
            }else if(periodo - silencio_tempo >= 3000){
                gpio_put(LED_PIN_RED, 0);   //Desativa o LED vermelho indicando fim do trecho da gravação
                printf("Registrando dados de gravação e horário.\n\n");
                parar_buzzer(); //Garante que o buzzer seja desligado
                alerta_sonoro = false;
                buzzer_ligado = false; //Reseta o estado do buzzer
            }
        }
    }
    return valor_microfone;
}

//Debounce do botão (evita leituras falsas)
bool debounce_botao(uint gpio){
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    //Verifica se o botão foi pressionado e se passaram 200ms
    if (gpio_get(gpio) == 0 && (tempo_atual - ultimo_tempo) > 200){ //200ms de debounce
        ultimo_tempo = tempo_atual;
        return true;
    }
    return false;
}

//Função de interrupção com Debouncing
void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

        // Caso o botão A seja pressionado
        if(gpio == BOTAO_A && !alerta_sonoro && debounce_botao(BOTAO_A)){
            gpio_put(LED_PIN_RED, 0);    //Desliga o LED vermelho
            numero++;   //Incrementa o valor da rua (matriz de leds)
            if (numero == 10){
                numero = 0; //Retorna ao andar 0
            }
        set_one_led(led_r, led_g, led_b, numero);
        }
}

int main(){
    //Chamamos as função de inicialização
    inicializar_GPIOs();

    //Configuramos as interrupções nos botão A
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    set_one_led(led_r, led_g, led_b, 0); //Inicia a simulação monitorando o rua de endereço 0

    while(true){
        //Variáveis usadas sendo usadas para armazenar os dados
        uint16_t valor_microfone= 0;
        float decibeis = 0.0;
        char bufferRua[20];
        char bufferDecibeis[20];

        valor_microfone = ler_microfone();                     //Le o valor do microfone
        decibeis = converter_adc_para_db((float)valor_microfone);  //Calcula o decibeis com base no valor lido

        ssd1306_fill(&ssd, false);                          //Limpa o display antes de escrever   
        ssd1306_draw_string(&ssd, "VIGILANCIA", 30, 10);    //Escreve no display

        sprintf(bufferRua, "Rua %d", numero);               //Converte o numero da rua para string
        ssd1306_draw_string(&ssd, bufferRua, 10, 30);       //Escreve no display

        sprintf(bufferDecibeis, "Decibeis %.2f", decibeis); //Converte o decibeis para string
        ssd1306_draw_string(&ssd, bufferDecibeis, 10, 50);  //Escreve no display
        ssd1306_send_data(&ssd);                            //Atualiza o display    

    }
}
