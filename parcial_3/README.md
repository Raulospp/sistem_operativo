# Mini-SO — Parcial 3

**Sistemas Operativos · USC 2026A · Docente: Diego Fernando Loaiza Buitrago**

Simulación funcional de un sistema operativo básico en ANSI C: procesos, memoria, sistema de archivos y kernel coordinador. Incluye un dashboard web opcional para visualizar y operar el SO desde el navegador.

---

## 1. Objetivos cubiertos

| Enunciado | Implementación |
|---|---|
| Operaciones de archivos (crear, abrir, leer, escribir, cerrar, eliminar) | `files.c` |
| Tabla de archivos abiertos por proceso | Campo `pid_dueno` en `Archivo` |
| Periféricos básicos de E/S | `perifericos.c` (teclado, pantalla, impresora) |
| Integración con memory.c y processes.c | Mismo binario, mismo `tipos.h` |
| Ciclo principal del kernel | `kernel.c` (modo interactivo + modo remoto) |
| Modularidad / separación de responsabilidades | 5 archivos `.c` + 1 `.h` |
| Bloqueo por archivo en uso | `pid_dueno` + cola `cola_espera[]` |
| Estado global del sistema | `mostrar_estado_global()` + `dump_estado_json()` |
| Liberación de recursos | `terminar_proceso()` libera memoria + cierra archivos en cascada |
| Finalización limpia | `finalizar_sistema()` purga colas e imprime resumen |

---

## 2. Arquitectura

```
┌────────────────────────────────────────────────────────────┐
│                          kernel.c                          │
│  inicializa · loop principal · dispatcher · dump JSON      │
└────────────┬───────────────────┬───────────────┬───────────┘
             │                   │               │
       ┌─────▼─────┐       ┌─────▼─────┐   ┌─────▼─────┐
       │ memory.c  │       │processes.c│   │  files.c  │
       │first-fit  │       │PCB + cola │   │FS en RAM  │
       │split/merge│       │FCFS · RR  │   │cola wait  │
       └───────────┘       └─────┬─────┘   └─────┬─────┘
                                 │               │
                                 │   bloquear /  │
                                 │  desbloquear  │
                                 └───────────────┘
                          ┌──────────────────────┐
                          │    perifericos.c     │
                          │ teclado/pant/imp     │
                          └──────────────────────┘
                                    ▲
                                    │ usados por kernel.c
```

Todas las cabeceras compartidas viven en `tipos.h`.

### Modo dashboard (opcional)

```
┌──────────┐  POST /cmd  ┌──────────┐  append  ┌──────────────┐
│ browser  │────────────▶│ serve.py │─────────▶│ comandos.txt │
│ dashboard│             └──────────┘          └──────┬───────┘
│  .html   │  GET estado.json                         │ poll 200ms
│          │◀─────────── kernel escribe ──────────────▼──────────
│          │             ┌────────────────────────────────────┐
└──────────┘             │  kernel.exe --remote               │
                         │  procesar_linea_remota() dispatch  │
                         └────────────────────────────────────┘
```

---

## 3. Estructura de archivos

```
parcial_3/
├── tipos.h            # Estructuras y prototipos compartidos
├── memory.c           # Particionamiento dinamico first-fit
├── processes.c        # Tabla PCB + cola circular + scheduler
├── files.c            # FS en RAM con cola de espera por archivo
├── perifericos.c      # E/S: teclado, pantalla, impresora
├── kernel.c           # Coordinador, menu interactivo y modo remoto
├── Makefile           # gcc -std=c99 -Wall
├── dashboard.html     # Visor web + control deck
├── serve.py           # HTTP server (dashboard + endpoint /cmd)
├── serve.bat          # Launcher de serve.py para Windows
└── Plantilla Informe- Sisoper 2026A_3.docx  # plantilla del informe
```

---

## 4. Compilación

```
gcc -std=c99 -Wall -Wextra -o kernel kernel.c memory.c processes.c files.c perifericos.c
```

O con make:

```
make            # compila ./kernel(.exe)
make run        # compila + ejecuta interactivo
make clean      # borra binario, estado.json, comandos.txt
```

Probado con `gcc 15.2.0` (MinGW64 en Windows 11) y bajo el estándar C99 + flags estrictos sin warnings.

---

## 5. Ejecución

### Modo A · Terminal interactiva

```
./kernel
```

Muestra un menú con 14 opciones (crear/terminar proceso, las 6 operaciones de archivos, imprimir, planificar FCFS/RR, etc.). Es el modo que se demuestra al docente sin necesidad de navegador.

### Modo B · Dashboard web

```
# terminal 1
./kernel --remote

# terminal 2
python serve.py      # o   .\serve.bat   en Windows

# luego abrir
http://localhost:8000/dashboard.html
```

El dashboard reemplaza al menú de texto: muestra **memoria**, **procesos**, **archivos** en vivo (refresh 1.5s) y un **Control Deck** con paleta de 13 verbos + accesos rápidos para enviar órdenes al kernel sin tocar la terminal.

---

## 6. Estructuras de datos clave (`tipos.h`)

| Estructura | Campos relevantes | Donde se usa |
|---|---|---|
| `PCB`         | `pid`, `nombre`, `estado`, `prioridad`, `memoria_asignada`, `archivo_esperado` | `processes.c` |
| `Bloque`      | `start`, `size`, `pid_dueno`, `libre` | `memory.c` |
| `Archivo`     | `nombre`, `contenido`, `tamanio`, `pid_dueno`, `cola_espera[]`, `cola_count` | `files.c` |
| `EstadoProceso` | `LIBRE, NUEVO, LISTO, EJECUCION, BLOQUEADO, TERMINADO` | global |
| `TrabajoImpresion` | `pid`, `texto[128]` | `perifericos.c` |

### Convenciones

- Identificadores en español, `snake_case` para funciones, `PascalCase` para `struct`, `UPPER_CASE` para `#define`.
- Tablas estáticas con tamaños del `tipos.h` — **no se usa `malloc`** salvo para casos puntuales (no aplica aquí). Esto simplifica el cleanup y elimina fugas de memoria.

---

## 7. Decisiones de diseño justificadas

### 7.1 First-fit para asignación de memoria
`memory.c` recorre `bloques[]` linealmente y asigna el primero que entra. Complejidad O(n) con n ≤ 32. Se eligió frente a best-fit/worst-fit por simplicidad y porque para 32 bloques las diferencias prácticas son despreciables; además la fragmentación externa se controla con `fusionar_libres()` al liberar.

### 7.2 Concurrencia por dueño único + cola de espera
Cada archivo guarda `pid_dueno` (−1 = libre) y un arreglo `cola_espera[]` con los PIDs que pidieron `abrir` mientras estaba tomado. Cuando el dueño hace `cerrar_archivo()`, el siguiente de la cola **automáticamente se vuelve dueño** y el kernel lo pasa de `BLOQUEADO` a `LISTO` vía `desbloquear_proceso()`.

Esto modela en pequeño un **mutex con cola FIFO** (como `pthread_mutex_t` o `pthread_cond_t` en POSIX). Es la respuesta a la pregunta conceptual del informe sobre escrituras simultáneas.

### 7.3 Limpieza en cascada al terminar un proceso
`terminar_proceso(pid)`:
1. Llama `liberar_archivos_de(pid)`: cierra los archivos que tenía abiertos. Si había cola, el siguiente queda como dueño y entra a `LISTO`.
2. Llama `liberar_memoria(pid)`: marca todos los bloques del PID como libres y los fusiona con vecinos.
3. Saca el PID de la cola de listos.
4. Marca el slot como `LIBRE`.

Esto evita zombies, fugas de memoria simulada y archivos huérfanos.

### 7.4 `tipos.h` aunque sea opcional
El enunciado dice que los `.h` son opcionales, pero con 5 `.c` que se llaman entre sí (kernel llama a todo; files llama a processes; processes llama a memory y a files) lo más limpio es una cabecera compartida con `extern` para los arrays globales y prototipos de las funciones públicas. Las funciones privadas quedan `static` dentro de cada `.c`.

### 7.5 Dashboard como ciudadano de segunda
El sistema funciona **sin** dashboard (modo interactivo cubre el enunciado completo). El dashboard es un complemento: el kernel exporta `estado.json` después de cada operación; un HTML estático lo lee y refresca. Para enviar comandos se usa un puente HTTP en Python (`serve.py`) que escribe a `comandos.txt` y el kernel hace polling. Esto desacopla totalmente la UI del kernel sin meter sockets en C.

---

## 8. Protocolo del modo remoto

`serve.py` escribe líneas en `comandos.txt` con formato **tab-separado**:

```
VERBO<TAB>arg1<TAB>arg2<TAB>arg3
```

El kernel mantiene un offset por bytes y solo procesa lo nuevo. Si el archivo se trunca o desaparece, el offset se resetea automáticamente.

### Catálogo de verbos

| Verbo            | Argumentos                  | Función invocada                |
|------------------|-----------------------------|---------------------------------|
| `CREAR_PROC`     | `nombre`, `prioridad`, `mem`| `crear_proceso()`               |
| `TERMINAR_PROC`  | `pid`                       | `terminar_proceso()`            |
| `CREAR_ARCH`     | `nombre`                    | `crear_archivo()`               |
| `ABRIR_ARCH`     | `pid`, `nombre`             | `abrir_archivo()`               |
| `LEER_ARCH`      | `pid`, `nombre`             | `leer_archivo()`                |
| `ESCRIBIR_ARCH`  | `pid`, `nombre`, `texto`    | `escribir_archivo()`            |
| `CERRAR_ARCH`    | `pid`, `nombre`             | `cerrar_archivo()`              |
| `ELIMINAR_ARCH`  | `pid`, `nombre`             | `eliminar_archivo()`            |
| `IMPRIMIR`       | `pid`, `texto`              | `impresora_imprimir()`          |
| `FLUSH_IMP`      | —                           | `impresora_flush()`             |
| `PLAN_FCFS`      | —                           | `planificar_fcfs()`             |
| `PLAN_RR`        | —                           | `planificar_rr(QUANTUM_DEFAULT)`|
| `ESTADO`         | —                           | `mostrar_estado_global()`       |
| `APAGAR`         | —                           | `finalizar_sistema()` + exit    |

### Formato de `estado.json`

```json
{
  "mem_total": 1024,
  "mem_libre": 524,
  "mem_usada": 500,
  "bloques":  [ {"i":0, "start":0, "size":200, "libre":0, "pid":1}, ... ],
  "procesos": [ {"pid":1, "nombre":"edit", "estado":"LISTO", "prioridad":1, "memoria":200, "espera_archivo":-1}, ... ],
  "archivos": [ {"i":0, "nombre":"doc.txt", "tam":10, "dueno":1, "cola":[2]} ],
  "impresora_pendientes": 0
}
```

---

## 9. Escenario de prueba

Demuestra todos los requisitos en menos de 10 órdenes:

| # | Acción (dashboard o terminal)        | Efecto observable                              |
|---|--------------------------------------|------------------------------------------------|
| 1 | Crear proceso `edit` prio 1 mem 200B | Bloque #0 ocupado, P1 LISTO                    |
| 2 | Crear proceso `comp` prio 2 mem 300B | Bloque #1 ocupado, P2 LISTO                    |
| 3 | Crear archivo `doc.txt`              | Archivo en disco, dueño = —                    |
| 4 | P1 abre `doc.txt`                    | Dueño = P1                                     |
| 5 | P1 escribe "hola mundo"              | `tam` = 10                                     |
| 6 | P2 abre `doc.txt`                    | **P2 → BLOQUEADO**, cola = [P2]                |
| 7 | P1 cierra `doc.txt`                  | **Dueño pasa a P2 automáticamente**, P2 LISTO  |
| 8 | Terminar P1                          | Bloque #0 libre, fusión con vecinos            |
| 9 | Apagar SO                            | Cleanup en cascada, memoria 1024/1024 libre    |

---

## 10. Limitaciones conocidas y posibles extensiones

- **Scheduler ilustrativo**: FCFS/RR muestran orden de servicio pero no consumen CPU real (el kernel es interactivo). Una extensión sería convertir el scheduler en un hilo que avanza `rafaga_restante` mientras el menú está idle.
- **Sin paginación / memoria virtual**: solo particionamiento contiguo. Una extensión natural es agregar una tabla de páginas y MMU simulada.
- **Sin permisos en archivos**: cualquier proceso puede leer cualquier archivo aunque otro lo tenga abierto. Se podría añadir `perm_lectura` / `perm_escritura` por archivo.
- **Polling en modo remoto**: 200 ms es buen balance entre latencia y CPU. Para producción se usaría `inotify`/`ReadDirectoryChangesW` o un socket TCP directo en el kernel.

---

## 11. Créditos

Estudiante: **Mateo Salas**. Reutiliza módulos del [Lab #2](../parcial_2/) entregado en abril.

Compilación verificada en Windows 11 (MinGW64). El binario y el dashboard son idénticos en Linux/macOS — solo cambia el script de servidor (usar `python serve.py` directamente).
