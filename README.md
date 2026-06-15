M1KE-OS
========

Resumen
-------
M1KE-OS es un proyecto de sistema operativo pequeño y modular desarrollado por M1KE-27. Contiene el bootloader, kernel básico, drivers, sistema de ficheros en RAM y una interfaz gráfica mínima.

Características principales
-------------------------
- Boot con multiboot
- Kernel monolítico compacto con soporte básico de drivers (teclado, ratón, serial, timer)
- Sistema de ficheros en RAM (`ramfs`)
- Gestión de procesos y sistema de llamadas
- Pequeña GUI y shell para pruebas

Estructura del repositorio
--------------------------
- `boot/` - código de arranque
- `kernel/` - código del kernel y subsistemas (drivers, mm, fs, gui)
- `tools/`, `apps/`, `iso/` - utilidades y artefactos de build

Compilación y prueba
--------------------
Para compilar y generar los artefactos del proyecto en una máquina Linux con las herramientas necesarias instaladas:

```bash
cd "/home/m1ke/Proyectos 2026/m1keOS"
make all
```

El `Makefile` produce los binarios/objetos en `obj/` y puede generar la imagen ISO en `iso/` si se tienen las herramientas necesarias.

Contribuciones y licencia
-------------------------
Este repositorio contiene el trabajo desarrollado por el autor principal listado en `ABOUT.md`.
# M1KE-PROJECT