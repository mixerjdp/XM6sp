
Host SSH: 192.168.31.130
Nombre de la máquina: WIN-MGCF55NP3HV
Puerto: 22
Usuario: Juan
Contraseña: juanjose
Comando desde terminal:

ssh Juan@192.168.31.130
Si la resolución por nombre local funciona, también puede usar:

ssh Juan@WIN-MGCF55NP3HV
Qué esperar:

La VM responde por SSH en 22
El servidor es OpenSSH_for_Windows_10.0 Win32-OpenSSH-GitHub
El acceso por llave no está configurado todavía, así que entra por contraseña
Notas útiles:

La primera vez puede pedir confirmar la huella del host, hay que aceptar.
Si no conecta, revisar que la VM siga con la IP 192.168.31.130 o hacer Test-NetConnection 192.168.31.130 -Port 22 en Windows.
Si quieren dejarlo más limpio después, se puede configurar acceso por llave y quitar contraseña.