# Inspiracion estetica de WaveForms Live para el Analizador Logico

WaveForms Live es un osciloscopio web de Digilent. Solo nos interesa su **estetica visual** (colores, tipografia, iconos, layout) para aplicarla a nuestro analizador logico.

## Paleta de colores

| Color | Hex | Uso en WaveForms Live | Para nuestro analizador |
|-------|-----|----------------------|------------------------|
| Fondo oscuro | `#222` | Fondos de pagina principal | Fondo general del canvas |
| Fondo paneles | `#343434` | Paneles laterales, cards | Panel de controles |
| Fondo loader | `#535353` | Pantalla de carga | Color intermedio |
| Texto principal | `#FFF` | Titulos, labels | Nombres de canales, mediciones |
| Texto secundario | `#CCC` | Descripciones, valores | Etiquetas de eje, sub-textos |
| Texto tenue | `#999` | Placeholders, hints | Divisiones menores, bordes |
| Borde/linea | `#3A3A3A` | Separadores, bordes de input | Grid del canvas, separadores |
| Azul primario | `#387ef5` | Botones principales, links | Boton RUN, estado conectado |
| Azul hover | `#3474e1` | Hover de botones | Hover en controles |
| Rojo error | `#f53d3d` | Errores, alertas | Indicador de desconexion, error |
| Rojo hover | `#e13838` | Hover rojo | Hover en STOP |
| Verde OK | `#32db64` | Exito, conectado | Indicador de trigger armado |
| Verde hover | `#2ec95c` | Hover verde | Checks, enable states |
| Verde tenue | `#61ac71` | Secundario verde | Mediciones correctas |
| Amarillo canal | `#FFFF00` | CH1 en osciloscopio | D0 (primer canal digital) |
| Cian canal | `#00FFFF` | CH2 en osciloscopio | D1 (segundo canal) |

## Tipografia

- **Fuente principal**: Roboto (regular 400, medium 500, bold 700, light 300)
- **Fuente mono**: No usan, pero recomendamos `'Roboto Mono', 'Consolas', monospace` para los valores de tiempo y mediciones
- **Tamanos**: h1=28px, h2=24px, body=14px, small=12px, label=11px

## Estilo de iconos

- **Line-art minimalista**: SVG monocromatico de 24x24
- **Grosor de linea**: ~2px, stroke-linecap="round"
- **Sin relleno** (o relleno del mismo color que el trazo)
- **Color unico por icono**: normalmente `#3A3A3A` o `#CCC` sobre fondo oscuro
- **Ejemplos**: triangulo play para RUN, cuadrado para STOP, flecha ondulante para SINGLE, linea ascendente para RISING, descendente para FALLING

## Layout

```
┌────────────────────────────────────────────────────┐
│ Barra superior: logo + estado conexion + SR       │  ← altura ~56px
├────────────────────────────────────────────────────┤
│                                                     │
│   ┌──────────┐  ┌────────────────────────────┐    │
│   │ Controles │  │                            │    │
│   │ Canales   │  │     CANVAS PRINCIPAL       │    │
│   │ Trigger   │  │     (formas digitales)     │    │
│   │ Timebase  │  │                            │    │
│   │           │  │                            │    │
│   └──────────┘  └────────────────────────────┘    │
│                                                     │
│   ┌────────────────────────────────────────────┐    │
│   │ Timeline / buffer overview (barra inferior)│    │
│   └────────────────────────────────────────────┘    │
│                                                     │
│   ┌────────────────────────────────────────────┐    │
│   │ Decodificacion / Mediciones                │    │
│   └────────────────────────────────────────────┘    │
├────────────────────────────────────────────────────┤
│ Barra inferior: estado + botones de accion         │  ← altura ~44px
└────────────────────────────────────────────────────┘
```

- **Padding general**: 16px entre paneles
- **Border-radius**: 4px en cards y botones
- **Sombras**: ninguna (flat design)
- **Scroll**: solo si es necesario, con scrollbar delgada (`::-webkit-scrollbar: width 6px`)

## Sensacion general

- **Oscuro pero no negro**: fondos en `#222` a `#535353`, no `#000`
- **Plano (flat)**: sin sombras, sin gradientes, sin 3D
- **Legible**: texto blanco/CC contrasta bien con fondo oscuro
- **Sobrio**: solo colores en los elementos funcionales (botones, canales, estados)
- **Inspirado en instrumentos reales**: recuerda a un osciloscopio Tektronix o Rigol en modo nocturno

## Referencias visuales directas

| Elemento | Icono SVG en WaveForms Live |
|----------|---------------------------|
| RUN | `run.svg` — triangulo play |
| STOP | `stop.svg` — cuadrado |
| SINGLE | `single.svg` — flecha con un solo pulso |
| Rising trigger | `rising.svg` — linea con flecha hacia arriba |
| Falling trigger | `falling.svg` — linea con flecha hacia abajo |
| Settings | `settings.svg` — engranaje |
| Zoom in | `zoom-in.svg` — lupa con + |
| Zoom out | `zoom-out.svg` — lupa con - |
| Refresh | `refresh.svg` — flecha circular |
| Save | `save.svg` — diskette |
| Power | `power.svg` — icono de poder |
| Connect | `wifi.svg` — senal wifi |
| Channels | `align-center.svg` — lineas apiladas |

## Lo que NO copiar

- El logo SVG de Digilent
- Los colores de la marca (azul corporativo #4e8ef7)
- Ionic components (tabs, cards, headers propietarios)
- La animacion de carga con logo grande
