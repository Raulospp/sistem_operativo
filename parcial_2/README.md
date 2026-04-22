# Laboratorio #2 — Sistemas Operativos
**Universidad Santiago de Cali, 2026A**  
**Estudiante:** Mateo Salas

---

## Estructura del proyecto

```
parcial_2/
├── 01_proceso/
│   └── process_mgmt.c          → PCB + fork/wait + Process Image
├── 02_ipc_sincronizacion/
│   ├── memoria_compartida.c    → IPC: Shared Memory (shmget/shmat)
│   └── fifo_shm.c              → IPC: FIFO + Shared Memory combinados
├── 03_memoria/
│   ├── particion_fija.c        → Particionamiento fijo (4 bloques iguales)
│   └── particion_dinamica.c    → Particionamiento dinámico: First/Best/Worst Fit
└── 04_scheduling_deadlocks/
    └── processes.c             → Scheduling (FCFS, RR, SPN, CFS, MLFQ)
                                   + Algoritmo del Banquero
                                   + Detección de deadlock (DFS)
```

---

## Requisitos

| Módulo | Plataforma |
|---|---|
| `processes.c`, `particion_fija.c`, `particion_dinamica.c` | Windows o Linux |
| `process_mgmt.c`, `memoria_compartida.c`, `fifo_shm.c` | **Solo Linux** (usan fork, shmget, mkfifo) |

**Compilador:** `gcc` con flags `-Wall -std=c99`

---

## Cómo compilar y ejecutar

### En Windows (PowerShell) — solo módulos 3 y 4

```powershell
cd W:\SO\sistem_operativo\parcial_2

# Módulo 4 — Principal evaluable
gcc -Wall -std=c99 04_scheduling_deadlocks\processes.c -o scheduler_lab
.\scheduler_lab.exe

# Módulo 3A — Partición fija
gcc -Wall -std=c99 03_memoria\particion_fija.c -o pfija
.\pfija.exe

# Módulo 3B — Partición dinámica
gcc -Wall -std=c99 03_memoria\particion_dinamica.c -o pdinamica
.\pdinamica.exe
```

### En Linux / WSL — todos los módulos

```bash
cd /ruta/a/parcial_2

# Módulo 1
gcc -Wall -std=c99 01_proceso/process_mgmt.c -o process_mgmt
./process_mgmt

# Módulo 2A
gcc -Wall -std=c99 02_ipc_sincronizacion/memoria_compartida.c -o shm_demo
./shm_demo

# Módulo 2B
gcc -Wall -std=c99 02_ipc_sincronizacion/fifo_shm.c -o fifo_demo
./fifo_demo

# Módulo 3A
gcc -Wall -std=c99 03_memoria/particion_fija.c -o pfija
./pfija

# Módulo 3B
gcc -Wall -std=c99 03_memoria/particion_dinamica.c -o pdinamica
./pdinamica

# Módulo 4 — Principal
gcc -Wall -std=c99 04_scheduling_deadlocks/processes.c -o scheduler_lab
./scheduler_lab
```

> Si quedó una clave de shared memory residual de una corrida anterior que falló, limpiarla con: `ipcrm -M 1234` y `ipcrm -M 5678`

---

## Módulo 1 — `process_mgmt.c`

### Conceptos demostrados
- **PCB (Process Control Block):** struct con PID, PPID, estado, prioridad, registros simulados
- **Process Image:** segmentos texto, datos globales y pila
- **Ciclo de vida:** NUEVO → LISTO → EJECUCION → TERMINADO
- **`fork()`** para crear hijos, **`wait()`** para sincronizar

### Salida esperada
```
=== MODULO 1: Anatomia de Proceso ===

[Padre] PCB inicial:
  PCB[pid=XXXX] padre=YYYY | nombre=Padre        | estado=EJECUCION   | prioridad=5 | cpu=0

[Padre] Imagen del proceso:
  Texto  : main() { fork(); fork(); wait(); }
  Datos  : [100, 200, ...]
  Pila   : tope=1, ret=0xDEAD

[Hijo1 pid=XXXX] ejecutando...
  PCB[pid=XXXX] padre=YYYY | nombre=Hijo1        | estado=EJECUCION   | prioridad=3 | cpu=5
[Hijo1] terminando.
[Hijo2 pid=XXXX] ejecutando...
  PCB[pid=XXXX] padre=YYYY | nombre=Hijo2        | estado=EJECUCION   | prioridad=2 | cpu=8
[Hijo2] terminando.
[Padre] Hijo pid=XXXX termino con codigo 0
[Padre] Hijo pid=XXXX termino con codigo 0

[Padre] PCB final:
  PCB[pid=XXXX] padre=YYYY | nombre=Padre        | estado=TERMINADO   | prioridad=5 | cpu=2
```
> Los PIDs son asignados por el SO y varían en cada ejecución.

---

## Módulo 2A — `memoria_compartida.c`

### Conceptos demostrados
- **shmget / shmat / shmdt / shmctl:** ciclo completo de shared memory POSIX
- **Sincronización manual** con campo `listo` (semáforo simulado de 3 estados)
- **Patrón productor–consumidor** con 3 procesos: 1 productor + 2 consumidores
- **malloc** para arreglos de resultados en heap

### Flujo de sincronización
```
listo=0 → Productor escribe (a,b) → listo=1
listo=1 → Consumidor1 lee, calcula a*b → listo=2
listo=2 → Consumidor2 lee, calcula a+b → listo=0
```

### Salida esperada
```
[Productor] Par generado: a=1  b=3
[Consumidor1] 1 * 3 = 3
[Consumidor2] 1 + 3 = 4
[Productor] Par generado: a=2  b=4
[Consumidor1] 2 * 4 = 8
[Consumidor2] 2 + 4 = 6
... (5 pares en total)

[Consumidor1] Resultados finales (mul):
  [0] = 3
  [1] = 8
  [2] = 15
  [3] = 24
  [4] = 35

[Consumidor2] Resultados finales (sum):
  [0] = 4
  [1] = 6
  [2] = 8
  [3] = 10
  [4] = 12

[Padre] Memoria compartida liberada. Fin.
```

---

## Módulo 2B — `fifo_shm.c`

### Conceptos demostrados
- **FIFO (named pipe):** canal de comunicación entre procesos vía sistema de archivos
- **mkfifo / open / read / write / unlink:** API POSIX de pipes con nombre
- **Combinación FIFO + Shared Memory:** el canal transporta datos, la memoria guarda resultados
- **Nota:** ambos consumidores compiten por el mismo FIFO (simplificación educativa)

### Diferencia con 2A
| | 2A Shared Memory | 2B FIFO + Shared Memory |
|---|---|---|
| Canal de datos | Memoria compartida directa | FIFO (archivo especial) |
| Sincronización | Campo `listo` manual | Bloqueo natural del FIFO |
| Uso real | Procesos en mismo host | Más cercano a pipes de shell |

### Salida esperada
```
[Productor] Enviado: a=1  b=3
[Consumidor1] 1 * 3 = 3
[Consumidor2] 1 + 3 = 4
... (5 pares)

[Consumidor1] Array final (multiplicacion):
  [0] = 3  ...

[Consumidor2] Array final (suma):
  [0] = 4  ...

[Padre] Todos los procesos finalizaron. Recursos liberados.
```

---

## Módulo 3A — `particion_fija.c`

### Conceptos demostrados
- Memoria dividida en **4 particiones iguales de 256 bytes** (total 1024)
- Asignación **First Fit secuencial**
- Rechazo si la solicitud supera el tamaño de partición

### Salida esperada
```
=== MODULO 3A: Particionamiento Fijo ===
[Alloc] Proceso 1 asignado a particion 0 (200 bytes)
[Alloc] Proceso 2 asignado a particion 1 (150 bytes)
[Alloc] Proceso 3 asignado a particion 2 (250 bytes)

Estado de memoria (4 particiones x 256 bytes):
  Particion 0 | Proceso 1 | 200 bytes
  Particion 1 | Proceso 2 | 150 bytes
  Particion 2 | Proceso 3 | 250 bytes
  Particion 3 | LIBRE

[Free] Proceso 2 liberado de particion 1

Estado de memoria (4 particiones x 256 bytes):
  Particion 0 | Proceso 1 | 200 bytes
  Particion 1 | LIBRE
  Particion 2 | Proceso 3 | 250 bytes
  Particion 3 | LIBRE

[Alloc] Proceso 4 asignado a particion 1 (100 bytes)

Estado de memoria (4 particiones x 256 bytes):
  Particion 0 | Proceso 1 | 200 bytes
  Particion 1 | Proceso 4 | 100 bytes
  Particion 2 | Proceso 3 | 250 bytes
  Particion 3 | LIBRE
```

---

## Módulo 3B — `particion_dinamica.c`

### Conceptos demostrados
- Bloques de tamaño variable (sin fragmentación fija)
- **First Fit:** primer bloque libre suficiente
- **Best Fit:** bloque más pequeño que cabe (minimiza fragmentación interna)
- **Worst Fit:** bloque más grande disponible (maximiza fragmento residual)
- **Compactación:** consolida bloques libres dispersos en uno solo al final

### Comparación de estrategias (con P1=200, P2=300, P3=100)

| Estrategia | Ventaja | Desventaja |
|---|---|---|
| First Fit | Rápido O(n) | Fragmentación al inicio |
| Best Fit | Menos desperdicio interno | Más lento, fragmentos pequeños inutilizables |
| Worst Fit | Fragmentos residuales grandes (reusables) | Puede fragmentar bloques grandes necesarios |

### Salida esperada (fragmento First Fit)
```
-- First Fit --
[FirstFit] Proceso 1 -> bloque 0 (inicio=0)
[FirstFit] Proceso 2 -> bloque 1 (inicio=200)
[FirstFit] Proceso 3 -> bloque 2 (inicio=500)

Estado de memoria:
  Bloque 0 | inicio=0   | tam=200 | Proceso 1
  Bloque 1 | inicio=200 | tam=300 | Proceso 2
  Bloque 2 | inicio=500 | tam=100 | Proceso 3
  Bloque 3 | inicio=600 | tam=424 | LIBRE

[Free] Proceso 2 liberado
[Compact] Memoria compactada. Libre desde 300 (724 bytes)
```

---

## Módulo 4 — `processes.c` (Principal evaluable)

### Procesos de prueba
| PID | Nombre | Ráfaga | Nice | Prioridad |
|---|---|---|---|---|
| 1 | EditorTexto | 8 | 0 | 3 |
| 2 | Compilador | 4 | -5 | 1 |
| 3 | Navegador | 12 | 5 | 4 |
| 4 | Servidor | 6 | -2 | 2 |

---

### 4A — FCFS (First Come First Served)
Ejecuta en orden de llegada, sin interrupciones.

**Salida esperada:**
```
=== FCFS (First Come First Served) ===
  EditorTexto  | fin=  8 | TAT=  8 | WT=  0
  Compilador   | fin= 12 | TAT= 12 | WT=  8
  Navegador    | fin= 24 | TAT= 24 | WT= 12
  Servidor     | fin= 30 | TAT= 30 | WT= 24
  Promedio          TAT=18.5  WT=11.0
```
> **TAT** (Turnaround Time) = tiempo_fin - tiempo_llegada  
> **WT** (Waiting Time) = TAT - ráfaga_total

---

### 4B — Round Robin (quantum=2)
Cada proceso recibe máximo 2 unidades de CPU; si no termina, vuelve al final.

**Salida esperada:**
```
=== ROUND ROBIN (quantum=2) ===
  [t=  2] EditorTexto  ejecuta   | restante=6
  [t=  4] Compilador   ejecuta   | restante=2
  [t=  6] Navegador    ejecuta   | restante=10
  [t=  8] Servidor     ejecuta   | restante=4
  [t= 10] EditorTexto  ejecuta   | restante=4
  [t= 12] Compilador   terminado | TAT= 12 | WT=  8 | RT=  4
  [t= 14] Navegador    ejecuta   | restante=8
  [t= 16] Servidor     ejecuta   | restante=2
  [t= 18] EditorTexto  ejecuta   | restante=2
  [t= 20] Navegador    ejecuta   | restante=6
  [t= 22] Servidor     terminado | TAT= 22 | WT= 16 | RT=  6
  [t= 24] EditorTexto  terminado | TAT= 24 | WT= 16 | RT=  0
  [t= 26] Navegador    ejecuta   | restante=4
  [t= 28] Navegador    ejecuta   | restante=2
  [t= 30] Navegador    terminado | TAT= 30 | WT= 18 | RT=  4
  Promedio          TAT=22.0  WT=14.5
```
> **RT** (Response Time) = tiempo de primera ejecución - tiempo de llegada

---

### 4C — SPN / SJF (Shortest Process Next)
Ordena por ráfaga más corta primero. No preemptivo.

**Salida esperada:**
```
=== SPN / SJF (Shortest Process Next) ===
  Compilador   | fin=  4 | TAT=  4 | WT=  0
  Servidor     | fin= 10 | TAT= 10 | WT=  4
  EditorTexto  | fin= 18 | TAT= 18 | WT= 10
  Navegador    | fin= 30 | TAT= 30 | WT= 18
  Promedio          TAT=15.5  WT=8.0
```
> SPN tiene el **menor WT promedio posible** para este conjunto de procesos.

---

### 4D — CFS (Completely Fair Scheduler con min-heap)
Simula el scheduler de Linux usando un min-heap en lugar del árbol Rojo-Negro real. Selecciona siempre el proceso con menor `vruntime`. El time slice depende del `nice`:

| nice | slice |
|---|---|
| < 0 | 4 (más CPU) |
| = 0 | 2 |
| > 0 | 1 (menos CPU) |

**Salida esperada:**
```
=== CFS (min-heap por vruntime, simula arbol R-N) ===
  [t=  4] Compilador   slice=4   | vruntime=   4 | restante=0
  [t=  4] Compilador   terminado | vruntime=   4 | TAT=  4 | WT=  0
  [t=  8] Servidor     slice=4   | vruntime=   4 | restante=2
  [t= 10] EditorTexto  slice=2   | vruntime=   2 | restante=6
  ...
  Promedio          TAT=X.X  WT=X.X
```
> El orden de ejecución varía según los vruntime acumulados. El proceso de nice=-5 (Compilador) recibe slices más largos.

---

### 4E — MLFQ (Multilevel Feedback Queue)
3 colas con quantums crecientes. Procesos CPU-bound degradan a colas más lentas.

| Cola | Quantum | Tipo de proceso |
|---|---|---|
| 0 | 2 | Interactivos / cortos |
| 1 | 4 | Medios |
| 2 | 8 | Batch / CPU-bound |

**Salida esperada:**
```
=== MLFQ (3 colas con degradacion) ===
  [t=  2] Cola0 | EditorTexto  (restante=6) -> Cola1
  [t=  4] Cola0 | Compilador   (restante=2) -> Cola1
  [t=  6] Cola0 | Navegador    (restante=10) -> Cola1
  [t=  8] Cola0 | Servidor     (restante=4) -> Cola1
  [t= 12] Cola1 | EditorTexto  (restante=2) -> Cola2
  [t= 14] Cola1 | Compilador   terminado | TAT= 14 | WT= 10
  [t= 18] Cola1 | Navegador    (restante=6) -> Cola2
  [t= 22] Cola1 | Servidor     terminado | TAT= 22 | WT= 16
  [t= 30] Cola2 | EditorTexto  terminado | TAT= 30 | WT= 22
  [t= 36] Cola2 | Navegador    terminado | TAT= 36 | WT= 24
  Promedio          TAT=25.5  WT=18.0
```

---

### 4F — Algoritmo del Banquero (Evitación de Deadlock)

**Datos del ejemplo:**
```
Available: [A=3, B=3, C=2]

Proceso  Max       Alloc     Need
P0       [7 5 3]   [0 1 0]   [7 4 3]
P1       [3 2 2]   [2 0 0]   [1 2 2]
P2       [9 0 2]   [3 0 2]   [6 0 0]
P3       [2 2 2]   [2 1 1]   [0 1 1]
```

**Salida esperada:**
```
=== ALGORITMO DEL BANQUERO (Evitacion de Deadlock) ===
  ...tablas Need...

  ESTADO SEGURO. Secuencia de finalizacion: P1 P3 P0 P2
```

**Traza de la secuencia segura:**
1. P1: need=[1,2,2] ≤ work=[3,3,2] ✓ → work=[5,3,2]
2. P3: need=[0,1,1] ≤ work=[5,3,2] ✓ → work=[7,4,3]
3. P0: need=[7,4,3] ≤ work=[7,4,3] ✓ → work=[7,5,3]
4. P2: need=[6,0,0] ≤ work=[7,5,3] ✓ → todos terminados

---

### 4G — Detección de Deadlock (DFS)

**Grafo del ejemplo (basado en el tablero de clase):**
```
P0 → solicita R1 → asignado a P1
P1 → solicita R0 → asignado a P2
P2 → solicita R1  (cierra el ciclo: P1→R0→P2→R1→P1)
```

**Salida esperada:**
```
=== DETECCION DE DEADLOCK (DFS en grafo de asignacion) ===
  Grafo: P0->R1->P1->R0->P2->R1 (ciclo esperado)
  Resultado: DEADLOCK DETECTADO (ciclo en grafo)
```

---

## Métricas de rendimiento — Comparación de algoritmos

| Algoritmo | TAT promedio | WT promedio | Preemptivo | Inanición |
|---|---|---|---|---|
| FCFS | 18.5 | 11.0 | No | No |
| Round Robin (q=2) | ~22.0 | ~14.5 | Sí | No |
| SPN/SJF | **15.5** | **8.0** | No | Sí (procesos largos) |
| CFS | Variable | Variable | Sí | No |
| MLFQ | ~25.5 | ~18.0 | Sí | Posible en cola 0 |

> SPN tiene el menor tiempo de espera promedio teórico, pero puede causar inanición en procesos largos. CFS es el más justo en la práctica (usado en Linux).

---

## Conceptos clave del curso

| Concepto | Archivo | Línea clave |
|---|---|---|
| PCB struct | `process_mgmt.c`, `processes.c` | `typedef struct { int pid; ... } PCB` |
| Process Image | `process_mgmt.c` | `typedef struct { char segmento_texto[]; ... } ImagenProceso` |
| Preemptive vs Non-preemptive | `processes.c` | RR y CFS son preemptivos; FCFS y SPN no |
| Condiciones de Coffman | `processes.c` comentarios | Exclusión mutua, retener-esperar, sin preempción, espera circular |
| Estado seguro (Banquero) | `processes.c` → `algoritmo_banquero()` | Secuencia P1→P3→P0→P2 |
| Ciclo = Deadlock | `processes.c` → `detectar_deadlock()` | DFS con array `en_pila[]` |
