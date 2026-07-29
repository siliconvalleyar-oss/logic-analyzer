# Contributing

## Codigo de conducta

Mantener un ambiente respetuoso y constructivo. No se tolera lenguaje ofensivo
o discriminacion de ningun tipo.

## Reportar bugs

Usar GitHub Issues con el siguiente formato:

```markdown
**Descripcion**: Una linea clara del problema
**Pasos para reproducir**:
1. Conectar pines X, Y, Z
2. Ejecutar `./logic_server -r 500000`
3. Hacer clic en RUN
4. Observar que...

**Comportamiento esperado**: ...
**Comportamiento actual**: ...
**Entorno**: Raspberry Pi 4 / Pi 2W / x86_64 sim
**Logs**:
```
[2026-07-28 21:30:00] [ERROR] [GPIO] mmap failed: Permission denied
```
```

## Proponer cambios

1. Fork el repositorio
2. Crear branch: `git checkout -b feat/nombre-cambio`
3. Commit con conventional commits:
   - `feat:` Nueva funcionalidad
   - `fix:` Correccion de bug
   - `docs:` Cambios en documentacion
   - `perf:` Mejora de rendimiento
   - `refactor:` Refactorizacion
   - `test:` Tests
   - `chore:` Mantenimiento
4. Push: `git push origin feat/nombre-cambio`
5. Abrir Pull Request describiendo:
   - Que cambia
   - Por que es necesario
   - Como se probo
   - Captura de pantalla si aplica (UI)

## Estandares de codigo

Ver `docs/REQUISITOS.md` para estandares detallados de:
- Comentarios Doxygen/JSDoc en funciones publicas
- Convencion de nombres (snake_case, PascalCase)
- Longitud de lineas (100 C++, 120 JS/Python)
- Manejo de errores con logging
- Logging estructurado con Logger

## Tests

```bash
# Compilar tests
make test-build

# Correr tests
make test

# Verificar cobertura (si aplica)
make test-coverage
```

## Versionado

El proyecto sigue `v1.0.0` a `v1.9.9` con ciclo patch 0-9 estricto:
- `v1.0.0` a `v1.0.9` — patches
- `v1.1.0` — siguiente minor
- Cada commit significativo lleva tag
- Tag y archivo `VERSION` deben coincidir

## Licencia

Al contribuir, aceptas que tu codigo se publica bajo MIT (ver `LICENSE`).
