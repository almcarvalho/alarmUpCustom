# 🚨 Alarme PIR ESP32 (HC-SR501) + Relé + Discord  
**Criado por Lucas Carvalho (@br.lcsistemas)**  
Versão: 2026-02-15

Sistema de alarme inteligente com **ESP32 + Sensor PIR HC-SR501**, com:

- 🔔 Acionamento de relé configurável
- 📲 Notificação via Discord (Webhook)
- 🌐 Configuração via WiFiManager (portal web)
- 🕒 Controle por horário (dia/noite/comercial)
- 🔄 Reset por botão físico
- 🧠 Sincronização automática de horário via NTP
- ⚙️ Tempo do relé customizável. Pode ser **0 segundos (modo somente notificação)**

---

# 📦 Funcionalidades

## ✅ Relé (Interruptor) configurável
- Tempo configurável via portal
- Pode ser:
  - Sempre
  - Apenas de dia
  - Apenas à noite
  - Apenas dentro do horário comercial
  - Apenas fora do horário comercial
- Pode ser configurado como **0 segundos**
  - Nesse caso o relé NÃO é acionado
  - Funciona apenas como sistema de notificação

---

## ✅ Notificação Discord
Envio automático de mensagem via webhook.

