# PSP Target Shooter 3D

Shooter arcade 3D en primera persona para PSP real y PPSSPP. Compilado nativamente con PSPSDK/PSPDEV; no usa Unity ni Unreal.

## Contenido

- Escena 3D con camara en primera persona, cielo, terreno, arboles y rocas.
- Pistola 3D descargada de un pack de modelos CC0; solo se muestra el arma, sin manos ni brazos.
- Objetivos 3D que cruzan el campo de tiro: caja, barril, lata y botella.
- Disparo hitscan, mira central, retroceso de la pistola y fogonazo.
- Fisica arcade al acertar: impulso, giro, gravedad y caida.
- Audio sintetizado para disparo y recarga.
- Puntuacion, aciertos, precision, municion, recarga y ronda de 90 segundos.
- Controles PSP y HUD dibujado mediante primitivas GU.

## Controles

| Control | Accion |
|---|---|
| Analogico / cruceta | Apuntar |
| R | Disparar |
| Cuadrado | Recargar |
| START | Pausa / continuar |
| SELECT | Reiniciar ronda |
| X | Iniciar / volver a jugar |
| Triangulo | Salir |

## Modelos 3D descargados

Los OBJ incluidos en `assets/models/` se obtuvieron de `Mcamento8/omnimod-3d-library`, una biblioteca de mallas Wavefront OBJ CC0:

- KayKit / Kay Lousberg: Gun_Pistol, Barrel_A y crate.
- Kenney: soda-can, soda-bottle, tree-large y rocks-large.

Los archivos de credito y avisos de licencia estan en `ASSET_CREDITS.txt` y `assets/models/LICENSE_*.txt`. Las mallas se convierten en buffers de vertices GU con `tools/obj2c.py` durante el build y quedan compiladas dentro de EBOOT.PBP; no se requieren archivos de modelos externos en la Memory Stick.

## Compilar

Requiere el contenedor `pspdev/pspdev:latest` o una instalacion PSPDEV/PSPSDK con Python 3.

```sh
python3 tools/obj2c.py
make clean
make
```

El resultado es `EBOOT.PBP`. GitHub Actions lo compila automaticamente en cada push y publica el artefacto `PSP-3D-Target-Shooter-EBOOT`.

## Instalar

Copia `EBOOT.PBP` a `PSP/GAME/TargetShooter/EBOOT.PBP` en la Memory Stick o abre el archivo con PPSSPP.

Esta es una version low-poly pensada para el limite de rendimiento de PSP. El ejecutable debe probarse en PPSSPP y en hardware PSP para validar visualizacion, audio y sensibilidad real de los controles.
