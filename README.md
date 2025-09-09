# Visualizador OBJ em OpenGL

Projeto de Computação Gráfica: um visualizador interativo de modelos 3D no formato `.obj`, com iluminação básica, textura procedural (xadrez) e controles de câmera/objeto via teclado e mouse. O carregador suporta vértices (`v`), coordenadas de textura (`vt`), normais (`vn`) e faces (`f`) trianguladas em fan, com cálculo de normais por vértice como fallback.

## Alunos
- Rafael Mota Alves
- Kauan Adami Guerreiro Chaves

## Sobre o projeto
- Renderização em OpenGL clássico com FreeGLUT (display list e pipeline fixo).
- Carregamento de OBJ com `v/vt/vn` e triangulação de faces.
- Iluminação por normal (usa `vn` do arquivo quando existir; caso contrário, calcula por vértice).
- Textura procedural (xadrez) aplicada quando o OBJ possui UV (`vt`).
- Gizmo de eixos e overlay de ajuda na tela.

Arquivos principais:
- `src/main.cpp`: loop principal, interação e renderização.
- `src/obj_loader.cpp/.h`: leitor de OBJ e geração de display list.
- `src/data/`: modelos de exemplo (`.obj`).

## Pré‑requisitos (Linux)
- Compilador C++17 (g++ ou clang)
- CMake >= 3.10
- OpenGL + GLU + FreeGLUT (headers e libs)
  - Ubuntu/Debian: `sudo apt update && sudo apt install -y build-essential cmake freeglut3-dev mesa-common-dev mesa-utils`

## Como compilar e executar

### Opção 1 — CMake (recomendada)
```bash
cmake -S . -B build
cmake --build build -j
# Executar de dentro da pasta build (para encontrar a pasta data/ copiada pelo CMake)
cd build
./main                # usa data/elepham.obj por padrão
# ou informe um .obj específico
./main data/porsche.obj
```
Obs.: Se preferir executar a partir da raiz do repositório, informe o caminho do OBJ explicitamente, por exemplo: `./build/main src/data/elepham.obj`.

## Controles
- W/S: transladar +Y/−Y
- A/D: transladar −X/+X
- Q/E: aproximar/afastar (Z)
- Setas: rotacionar em X/Y
- Z/X: rotacionar em Z
- +/−: aumentar/diminuir escala
- T: alternar textura ON/OFF
- Mouse esquerdo (arrastar): rotacionar
- Mouse direito (arrastar): transladar
- Scroll: aproximar/afastar
- R: resetar transformações
- ESC: sair

## Usando modelos próprios
Coloque seu arquivo `.obj` acessível e passe o caminho como argumento na execução. Se contiver `vt` e `vn`, o programa usará as UVs e normais do arquivo; caso contrário, UVs serão ignoradas e as normais serão calculadas por vértice.
