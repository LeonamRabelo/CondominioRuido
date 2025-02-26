## 🎛️ Projeto final - SISTEMA DE MONITORAMENTO DE PERTURBAÇÃO SONORA PARA CONDOMÍNIOS

Proposta de simulação do projeto de monitoramento de sinais sonoros em condomínios, utilizando a placa BitDogLab.
Com o uso do microfone conectado ao microcontrolador da BitDogLab, será realizada a captação dos sons do ambiente. O sinal analógico será convertido via ADC (Conversor Analógico-Digital) e analisado para determinar se o nível sonoro excede um limiar pré-definido, simulando um limite de decibéis.

## ⚙️ Configuração dos Pinos

| Componente                    | Pino Raspberry Pi Pico |
|--------------------------------|------------------------|
| Botão A                       | GPIO5                  |
| LED Vermelho                  | GPIO13                 |
| I2C SDA (Display SSD1306)     | GPIO14                 |
| I2C SCL (Display SSD1306)     | GPIO15                 |
| Microfone (ADC)               | GPIO28                 |
| Matriz de LEDs WS2812B         | GPIO7                  |
| Buzzer (PWM)                  | GPIO21                 |

## 🚀 Como Executar

### **Requisitos**
- SDK do Raspberry Pi Pico instalado e configurado
- VS Code com as extensões: 'Raspberry Pi Pico Project', 'CMake' configurado

### **Execução na Placa BitDogLab:**
1. Importe o projeto (pasta) utilizando a extensão Raspberry Pi Pico Project.
2. Compile o projeto para gerar o arquivo `.uf2`.
3. Envie o arquivo `.uf2` para a placa Raspberry Pi Pico.

## 🎮 Controles e Funcionalidade

✅ **Captação e Análise de Ruído**
   - O microfone capta o som ambiente e converte o sinal analógico para digital via ADC, que posteriormente será convertido para a medida decibéis.
   - O sistema analisa a intensidade sonora e compara com um limiar pré-definido.

✅ **Sinalização de Perturbação Sonora**
   - Se o nível de ruído exceder o limite, por um tempo constante determinado (3 segundos) o LED vermelho acende, simulando ativação da gravação da câmera do local, registrando o incidente.
   - O buzzer emite um som de alerta notificando perturbação (simulado na BitDogLab).
   - A matriz de LEDs WS2812B indica qual sensor está sendo monitorado, simulando que cada rua tenha um sensor, e que cada rua tenha um endereço enumerado de 0 a 9.

✅ **Interface de Exibição**
   - O display SSD1306 exibe em tempo real o nível sonoro captado na medida de decibéis.
   - Informações sobre alertas e notificações são mostradas no display.

✅ **Registro de Eventos**
   - O sistema simula gravação do registro, incluindo gravação por meio da comunicação serial USB, que pode armazenar os eventos de perturbação sonora para posterior análise.

🎥 **Demonstração do Projeto**
🔗 Vídeo de demonstração: []

---

👨‍💻 **Autor**
Desenvolvido por **Leonam S. Rabelo**