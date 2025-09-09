// Carregador de arquivos OBJ com suporte a v, vt, vn e f.
// Expõe uma função para ler o arquivo, calcular normais por vértice (fallback) e
// gerar uma display list OpenGL com preenchimento e iluminação por normal.

#pragma once

#include <GL/freeglut.h>
#include <string>
#include <vector>

// Para simplificar leitura do código neste trabalho acadêmico
using namespace std;

// Representa um canto de triângulo com índices separados do OBJ
// v: índice de posição, vt: índice de coordenada de textura, vn: índice de normal
struct CantoTri {
    int v, vt, vn;
};

// Lê um arquivo .obj (v, vt, vn, f), triangula faces em fan, calcula normais por vértice (fallback)
// e constrói uma display list para renderização com iluminação. Também devolve os buffers carregados
// caso queira inspecionar os dados.
bool carregarOBJParaDisplayList(
    const string& caminho,
    vector<float>& vertices,              // xyz
    vector<unsigned int>& indicesPos,     // indices somente de posição (para cálculo de normais)
    vector<float>& normaisCalculadas,     // nx ny nz por vértice (fallback)
    vector<float>& normaisOBJ,            // vn do arquivo
    vector<float>& uvs,                   // vt do arquivo (u v)
    vector<CantoTri>& triangulos,         // lista de triângulos (3 cantos por triângulo)
    GLuint& displayListOut                // id da display list gerada
);
