Como o usuário preenche no portal (WiFiManager)
No portal vai aparecer 4 campos:
Rele: tempo aceso (segundos)
Rele modo (0..4)
Discord webhook
Notificacao modo (0..4)
Tabela de modos:
0 = sempre
1 = dia (06:00–18:00)
2 = noite (18:00–06:00)
3 = fora do horário comercial
4 = dentro do horário comercial
Se você quiser que o portal tenha um “select bonitinho” ao invés de digitar 0..4, dá pra fazer, mas exige HTML custom do WiFiManager (fica um pouco mais chato). Do jeito acima é o mais estável.
Reset das configurações usando botão do ESP
Segura o botão BOOT por 5 segundos durante o funcionamento:
limpa Preferences (NVS)
limpa Wi-Fi salvo do WiFiManager (resetSettings)
reinicia o ESP
Também deixei um extra:
Se segurar BOOT logo no boot (~1s), ele abre o portal sem precisar apagar nada.
Como o ESP sabe a hora (gratuito)
Usei NTP com:
pool.ntp.org
time.google.com
a.ntp.br
E apliquei timezone Bahia (UTC-3) via:
setenv("TZ", "BRT3", 1); tzset();
