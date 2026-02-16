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



---

## ⚙️ 3️⃣ Configurar o Sistema

Quando você liga esse equipamento pela primeira vez ele gera uma rede wifi 
para que você possa se conectar a ela e definir as configurações.

### 📶 Wi-Fi
- Conecte-se na placa. Rede: AlarmUP
- Digite a senha: 1234567890
- Após conexão veja o ip gerado
- abra no navegador. Ex: 192.168.1.4
- Defina as suas configurações e clique em salvar.

---

### 🔔 Relé – Tempo (segundos)
- Defina quantos segundos o relé ficará ligado
- Use **0** se quiser apenas notificação (sem acionar relé)

---

### 🕒 Relé Modo (digite número)

| Número | Modo |
|---------|--------|
| 0 | Sempre |
| 1 | Dia |
| 2 | Noite |
| 3 | Fora horário comercial |
| 4 | Dentro horário comercial |

---

### 📲 Discord Webhook

## 🔔 Como criar um Webhook para receber notificações

1. Abra o **Discord**.
2. Crie um **Servidor** (ou use um já existente).
3. Crie um **Canal de Texto** onde deseja receber as notificações.
4. Vá em:
5. Clique em **Novo Webhook**.
6. Escolha o canal onde as mensagens serão enviadas.
7. Clique em **Copiar URL do Webhook**.
8. Cole essa URL no campo **Discord Webhook** no portal do ESP32.

---

### ⚠️ Observação

Você pode usar esse mesmo canal de notificações e adicionar outras pessoas nele para que elas também recebam os alertas.

Cole a URL completa do webhook, por exemplo: 
https://discord.com/api/webhooks/123456789/





### 🔔 Notificação Modo (0–4)
| Número | Modo |
|---------|--------|
| 0 | Sempre |
| 1 | Dia |
| 2 | Noite |
| 3 | Fora horário comercial |
| 4 | Dentro horário comercial |

---

## 💾 4️⃣ Salvar

- Clique em **Salvar**
- O ESP irá reiniciar
- Conectará automaticamente na sua rede

---

## 📅 Definição de Horário Comercial

### 🗓 Segunda a Sexta-feira
- **07:50 – 12:15**
- **14:20 – 18:20**

---

### 🗓 Sábado
- **07:50 – 13:00**

---

### 🗓 Domingo
- ❌ Não é considerado horário comercial

## 🔄 Reset das Configurações

- Segure o botão **BOOT** por 5 segundos  
→ Apaga Wi-Fi e configurações  
→ Reinicia o sistema

---

✅ Pronto!  
O sistema já está monitorando movimento.

