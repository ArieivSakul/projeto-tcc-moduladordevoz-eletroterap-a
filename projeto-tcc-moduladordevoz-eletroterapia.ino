/**
 * Codigo TENS SoftwareSerial Estavel (Logica Invertida)
 *
 * Esta é a versão final e estável do protótipo TENS.
 *
 * SOLUÇÕES IMPLEMENTADAS:
 * 1.  (Voz) Usa Pinos 2 e 3 (SoftwareSerial) para o Módulo de Voz, que provou ser funcional no treinamento.
 * 2.  (PWM) Usa Pino 11 (Timer 2) para o sinal PWM (TENS), evitando o conflito de temporização com o SoftwareSerial.
 * 3.  (Ruído) Inclui o Resistor de 1kΩ (Gate) e Capacitor de 100nF (Decoupling) para estabilidade elétrica.
 * 4.  (Relé) Usa LÓGICA INVERTIDA (LOW para Ligar) para o Módulo Relé, resolvendo a falha de ativação.
 * 5.  (Botão) Mantém o botão no Pino 5 para inicialização manual forçada do módulo de voz.
 */

#include <SoftwareSerial.h>
#include "VoiceRecognitionV3.h"

// --- Pinos de Controle ---
// 1. SoftwareSerial (Voz)
const int voiceRxPin = 2; // RX do Arduino (Conecta ao TXD do Módulo)
const int voiceTxPin = 3; // TX do Arduino (Conecta ao RXD do Módulo)
// SoftwareSerial myVoiceSerial(voiceRxPin, voiceTxPin); // Objeto SoftwareSerial (REMOVIDO - Redundante)

// 2. Hardware TENS
const int pulsePin = 11;  // Pino PWM (Timer 2) para o MOSFET
const int relaiPin = 4;   // Pino digital para o Módulo Relé

// 3. Controles Manuais
const int frequencyPotPin = A0;
const int intensityPotPin = A1;
const int manualLoadButtonPin = 5; // Botão para forçar inicialização (Pino 5 -> GND)

// Objeto do Módulo de Voz, agora usando o SoftwareSerial
// VR myVR(&myVoiceSerial); // (INCORRETO)
VR myVR(voiceRxPin, voiceTxPin); // (CORRETO) - A biblioteca espera os pinos

// Códigos de retorno do módulo de voz (obtidos no treinamento)
const uint8_t CMD_LIGAR = 0;
const uint8_t CMD_DESLIGAR = 1;
const uint8_t CMD_AUMENTAR = 2;
const uint8_t CMD_DIMINUIR = 3;

// Variáveis de controle de estado
bool isTensOn = false;
int currentIntensity = 0;
int currentFrequency = 0;

// Constantes para controle de voz e limites
const int MAX_INTENSITY = 255;
const int MIN_INTENSITY = 0;
const int STEP_INTENSITY = 25;
const int MAX_FREQUENCY = 120;
const int MIN_FREQUENCY = 2;

// --- Funções ---

/**
 * @brief Força a inicialização (Clear + Load) do Módulo de Voz.
 * Esta função é chamada automaticamente no setup e manualmente pelo botão.
 */
void forceLoadModule() {
  Serial.println(F("--- Tentando Inicializacao Manual do Modulo (Pinos 2/3) ---"));

  // 1. Limpa registros (acorda o módulo)
  if (myVR.clear() == 0) {
    Serial.println(F("Comando CLEAR enviado. Aguardando 500ms para estabilizacao..."));
    delay(500); // Delay crucial após o clear
  } else {
    Serial.println(F("ERRO: Modulo nao respondeu ao comando CLEAR."));
    return;
  }

  // 2. Carrega os comandos treinados
  uint8_t records[4] = {CMD_LIGAR, CMD_DESLIGAR, CMD_AUMENTAR, CMD_DIMINUIR};
  if (myVR.load(records, 4) >= 0) {
    Serial.println(F("SUCESSO: Modulo de Voz ativado e comandos carregados."));
  } else {
    Serial.println(F("ERRO: Falha ao carregar comandos. O modulo pode estar danificado."));
  }
}

void setup() {
  // Inicia a comunicação serial para debug (Monitor Serial nos pinos 0 e 1)
  Serial.begin(9600);

  // Inicia a comunicação com o Módulo de Voz (SoftwareSerial nos pinos 2 e 3)
  myVR.begin(9600);

  // Define os pinos
  pinMode(pulsePin, OUTPUT);
  pinMode(relaiPin, OUTPUT);
  pinMode(manualLoadButtonPin, INPUT_PULLUP); // Botão usa resistor pull-up interno

  // --- CORREÇÃO DE LÓGICA INVERTIDA DO RELÉ ---
  // Garante que o relé e o TENS estejam desligados no início.
  // Como o relé pode ser "LOW level trigger", HIGH é o estado OFF.
  digitalWrite(relaiPin, HIGH); // Relé Desligado (Segurança)
  analogWrite(pulsePin, 0);   // PWM Desligado

  Serial.println(F("Sistema de TENS (SoftwareSerial + PWM Pino 11) ativado."));
  
  // Tenta a inicialização automática do módulo de voz
  forceLoadModule();
}

void loop() {
  // 1. Verifica o botão de inicialização manual
  // Se o pino 5 for para LOW (botão pressionado), força a reinicialização do módulo
  if (digitalRead(manualLoadButtonPin) == LOW) {
    forceLoadModule();
    delay(500); // Evita múltiplas leituras (debounce)
  }

  // 2. Tenta reconhecer o comando de voz
  // Aumentamos o timeout para 500ms para dar tempo ao modulo de processar
  uint8_t buf[64];
  int ret = myVR.recognize(buf, 500); 

  if (ret > 0) {
    // Se a voz for reconhecida, execute a acao
    switch (buf[1]) {
      case CMD_LIGAR:
        isTensOn = true;
        // --- CORREÇÃO DE LÓGICA INVERTIDA ---
        digitalWrite(relaiPin, LOW); // LOW para ATIVAR o relé
        
        // --- CORREÇÃO DE TEMPORIZAÇÃO (NOVO) ---
        // Adicionamos um atraso de 500ms para permitir que o relé
        // estabilize ANTES que o circuito TENS (Pino 11) comece a gerar ruído.
        delay(500); 
        
        Serial.println(F("Comando: LIGAR - TENS Ativado."));
        break;

      case CMD_DESLIGAR:
        isTensOn = false;
        // --- CORREÇÃO DE LÓGICA INVERTIDA ---
        digitalWrite(relaiPin, HIGH); // HIGH para DESATIVAR o relé
        analogWrite(pulsePin, 0);
        Serial.println(F("Comando: DESLIGAR - TENS Desativado."));
        break;

      case CMD_AUMENTAR:
        if (isTensOn) {
          currentIntensity += STEP_INTENSITY;
          if (currentIntensity > MAX_INTENSITY) {
            currentIntensity = MAX_INTENSITY;
          }
          Serial.print(F("Comando: AUMENTAR - Intensidade (Voz): "));
          Serial.println(currentIntensity);
        }
        break;

      case CMD_DIMINUIR:
        if (isTensOn) {
          currentIntensity -= STEP_INTENSITY;
          if (currentIntensity < MIN_INTENSITY) {
            currentIntensity = MIN_INTENSITY;
          }
          Serial.print(F("Comando: DIMINUIR - Intensidade (Voz): "));
          Serial.println(currentIntensity);
        }
        break;

      default:
        // Codigo nao reconhecido
        break;
    }
  }

  // 3. Leitura dos potenciometros (Controle Manual)
  int potIntensityValue = analogRead(intensityPotPin);
  int manualIntensity = map(potIntensityValue, 0, 1023, MIN_INTENSITY, MAX_INTENSITY);
  currentFrequency = map(analogRead(frequencyPotPin), 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY);

  // 4. Aplicacao do Sinal TENS
  if (isTensOn) {
    // A intensidade manual (potenciômetro) tem prioridade sobre o comando de voz
    analogWrite(pulsePin, manualIntensity);

    if (currentFrequency > 0) {
      long delayTime = 1000000L / (currentFrequency * 2L);
      delayMicroseconds(delayTime);
    }
  } else {
    // Garante que tudo esteja desligado
    analogWrite(pulsePin, 0);
    digitalWrite(relaiPin, HIGH); // HIGH é o estado OFF (Desligado)
  }
}
