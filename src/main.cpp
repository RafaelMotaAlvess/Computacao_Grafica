#include <GL/freeglut.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include "obj_loader.h"

using namespace std;

// Desenha um cubo colorido (quando não há OBJ)
static void desenharCuboColorido();

// Tamanho da janela
static int g_width = 800;
static int g_height = 600;

// Estado de transformação do objeto
static float g_tx = 0.0f, g_ty = 0.0f, g_tz = -3.0f; // translação
static float g_rx = 0.0f, g_ry = 0.0f, g_rz = 0.0f;  // rotação (graus)
static float g_scale = 1.0f;                         // escala

// Estado do mouse
static bool g_lmb_down = false; // botão esquerdo arrasta: rotacionar
static bool g_rmb_down = false; // botão direito arrasta: transladar
static int g_last_x = 0, g_last_y = 0;

static vector<float> g_vertices;       // posições: xyz intercalado
static vector<unsigned int> g_indices; // índices (por posição) 0-based, para cálculo de normais fallback
static vector<float> g_vnormals;       // normais calculadas por vértice (xyz intercalado)

// Dados vindos diretamente do OBJ
static vector<float> g_onormals;       // normais do arquivo (vn) xyz intercalado
static vector<float> g_texcoords;      // coordenadas de textura (vt) uv intercalado
static vector<CantoTri> g_triangulos;  // triângulos (3 cantos por triângulo)

// Textura simples (procedural) para demonstrar mapeamento UV
static GLuint g_texID = 0;
static bool g_texEnabled = true;

static GLuint g_objList = 0;
static bool g_objLoaded = false;

static bool arquivoExiste(const string& path) {
    ifstream f(path);
    return f.good();
}

static void criarTexturaXadrez(int w = 64, int h = 64, int check = 8) {
    if (g_texID != 0) {
        glDeleteTextures(1, &g_texID);
        g_texID = 0;
    }
    vector<unsigned char> img((size_t)w * h * 3u, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int cx = (x / check) & 1;
            int cy = (y / check) & 1;
            unsigned char c = (cx ^ cy) ? 230 : 50;
            size_t i = (size_t)(y * w + x) * 3u;
            img[i+0] = c;
            img[i+1] = c;
            img[i+2] = c;
        }
    }
    glGenTextures(1, &g_texID);
    glBindTexture(GL_TEXTURE_2D, g_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, img.data());
}


// Desenha o OBJ carregado ou o cubo colorido como fallback
static void desenharOBJorFallback() {
    if (g_objLoaded && g_objList != 0) {
        // Chamada para desenhar o OBJ
        glCallList(g_objList);
    } else {
        // Sem OBJ, desenha o cubo padrão
        desenharCuboColorido();
    }
}

// Desenha instruções de texto na tela
static void desenharTextoBitmap2D(float x, float y, const string& text) {
    glRasterPos2f(x, y);
    for (unsigned char c : text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
}

// Reseta a transformação do objeto (posição/rotação/escala)
static void resetTransform() {
    g_tx = 0.0f; g_ty = 0.0f; g_tz = -3.0f;
    g_rx = 0.0f; g_ry = 0.0f; g_rz = 0.0f;
    g_scale = 1.0f;
}

// Desenha um cubo colorido
static void desenharCuboColorido() {
    const float s = 0.5f;
    glDisable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);

    // +X (vermelho)
    glColor3f(1,0,0);
    glVertex3f( s,-s,-s); glVertex3f( s,-s, s); glVertex3f( s, s, s);
    glVertex3f( s,-s,-s); glVertex3f( s, s, s); glVertex3f( s, s,-s);

    // -X (amarelo)
    glColor3f(1,1,0);
    glVertex3f(-s,-s, s); glVertex3f(-s,-s,-s); glVertex3f(-s, s,-s);
    glVertex3f(-s,-s, s); glVertex3f(-s, s,-s); glVertex3f(-s, s, s);

    // +Y (verde)
    glColor3f(0,1,0);
    glVertex3f(-s, s,-s); glVertex3f( s, s,-s); glVertex3f( s, s, s);
    glVertex3f(-s, s,-s); glVertex3f( s, s, s); glVertex3f(-s, s, s);

    // -Y (ciano)
    glColor3f(0,1,1);
    glVertex3f(-s,-s, s); glVertex3f( s,-s, s); glVertex3f( s,-s,-s);
    glVertex3f(-s,-s, s); glVertex3f( s,-s,-s); glVertex3f(-s,-s,-s);

    // +Z (azul)
    glColor3f(0,0,1);
    glVertex3f(-s,-s, s); glVertex3f(-s, s, s); glVertex3f( s, s, s);
    glVertex3f(-s,-s, s); glVertex3f( s, s, s); glVertex3f( s,-s, s);

    // -Z (magenta)
    glColor3f(1,0,1);
    glVertex3f( s,-s,-s); glVertex3f( s, s,-s); glVertex3f(-s, s,-s);
    glVertex3f( s,-s,-s); glVertex3f(-s, s,-s); glVertex3f(-s,-s,-s);

    glEnd();
}

// Define orientação de XYZ no canto superior direito
static void drawAxesGizmo() {
    const int size = 100;
    const int margin = 10;
    glViewport(g_width - size - margin, g_height - size - margin, size, size);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective(40.0, 1.0, 0.1, 10.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0, 0, -2.0f);

    // Rotaciona igual ao objeto para mostrar orientação
    glRotatef(g_rx, 1,0,0);
    glRotatef(g_ry, 0,1,0);
    glRotatef(g_rz, 0,0,1);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
      // X - vermelho
      glColor3f(1,0,0); glVertex3f(0,0,0); glVertex3f(0.8f,0,0);
      // Y - verde
      glColor3f(0,1,0); glVertex3f(0,0,0); glVertex3f(0,0.8f,0);
      // Z - azul
      glColor3f(0,0,1); glVertex3f(0,0,0); glVertex3f(0,0,0.8f);
    glEnd();

    // Letras
    glColor3f(1,0,0); glRasterPos3f(0.9f, 0.0f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'X');
    glColor3f(0,1,0); glRasterPos3f(0.0f, 0.9f, 0.0f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Y');
    glColor3f(0,0,1); glRasterPos3f(0.0f, 0.0f, 0.9f); glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'Z');

    // Restaura estado
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();

    // Volta viewport padrão
    glViewport(0, 0, g_width, g_height);
}

// Overlay de ajuda com mapeamento de teclas e mouse
static void drawHelpOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, g_width, 0, g_height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    int x = 10;
    int y = g_height - 10;
    auto line = [&](const string& s){ glColor3f(1,1,1); desenharTextoBitmap2D((float)x, (float)y, s); y -= 18; };

    line("Comandos:");
    line("W/S: Transladar +Y/-Y");
    line("A/D: Transladar -X/+X");
    line("Q/E: Aproximar/Afastar (Z)");
    line("Setas: Rotacionar em X/Y");
    line("Z/X: Rotacionar em Z");
    line("+/−: Aumentar/Diminuir escala");
    line("T: Alternar textura ON/OFF");
    line("Mouse Esq: arrastar p/ rotacionar");
    line("Mouse Dir: arrastar p/ transladar");
    line("Scroll: Aproximar/Afastar");
    line("R: Resetar");

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
}

// Desenha a cena: câmera, luz, transformações e objeto
static void display() {
    glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Projeção
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = (g_height > 0) ? (float)g_width / (float)g_height : 1.0f;
    gluPerspective(60.0, aspect, 0.1, 100.0);

    // ModelView
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Configuração de iluminação no espaço da câmera (antes das transformações do objeto)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glShadeModel(GL_SMOOTH);

    // Luz branca direcional/pontual fixa em relação à câmera
    GLfloat lightPos[4] = {2.0f, 3.0f, 4.0f, 1.0f};
    GLfloat lightCol[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightCol);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightCol);

    // Textura (ativa somente se há UV carregado)
    if (g_texEnabled && g_texID != 0 && !g_texcoords.empty()) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_texID);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    // Aplica transformações
    glTranslatef(g_tx, g_ty, g_tz);
    glRotatef(g_rx, 1,0,0);
    glRotatef(g_ry, 0,1,0);
    glRotatef(g_rz, 0,0,1);
    glScalef(g_scale, g_scale, g_scale);

    // Desenha o arquivo OBJ (ou cubo)
    desenharOBJorFallback();

    // Gizmo de eixos (fixo na tela)
    drawAxesGizmo();

    // Overlay de ajuda
    drawHelpOverlay();

    glutSwapBuffers();
}

// Ajusta viewport quando a janela é redimensionada
static void reshape(int w, int h) {
    g_width = (w <= 0 ? 1 : w);
    g_height = (h <= 0 ? 1 : h);
    glViewport(0, 0, g_width, g_height);
}

// Interação via teclado
static void onKeyboard(unsigned char key, int, int) {
    switch (key) {
        case 'w': case 'W': g_ty += 0.1f; break;
        case 's': case 'S': g_ty -= 0.1f; break;
        case 'a': case 'A': g_tx -= 0.1f; break;
        case 'd': case 'D': g_tx += 0.1f; break;
        case 'q': case 'Q': g_tz += 0.1f; break;
        case 'e': case 'E': g_tz -= 0.1f; break;
        case '+': case '=': g_scale *= 1.1f; break;
        case '-': case '_': g_scale /= 1.1f; break;
        case 'z': case 'Z': g_rz -= 5.0f; break;
        case 'x': case 'X': g_rz += 5.0f; break;
        case 'r': case 'R': resetTransform(); break;
        case 't': case 'T': g_texEnabled = !g_texEnabled; break; // textura ON/OFF
        case 27: /* ESC */ exit(0);
        default: break;
    }
    glutPostRedisplay();
}

// Interação via teclas especiais (setas, PgUp, PgDn, etc)
static void onSpecial(int key, int, int) {
    switch (key) {
        case GLUT_KEY_LEFT:  g_ry -= 5.0f; break;
        case GLUT_KEY_RIGHT: g_ry += 5.0f; break;
        case GLUT_KEY_UP:    g_rx -= 5.0f; break;
        case GLUT_KEY_DOWN:  g_rx += 5.0f; break;
        case GLUT_KEY_PAGE_UP:   g_rz += 5.0f; break;
        case GLUT_KEY_PAGE_DOWN: g_rz -= 5.0f; break;
        default: break;
    }
    glutPostRedisplay();
}

// Interação via mouse
static void onMouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) g_lmb_down = (state == GLUT_DOWN);
    if (button == GLUT_RIGHT_BUTTON) g_rmb_down = (state == GLUT_DOWN);
    if (state == GLUT_DOWN) {
        if (button == 3) g_tz += 0.1f; // scroll up aproxima
        if (button == 4) g_tz -= 0.1f; // scroll down afasta
    }
    g_last_x = x; g_last_y = y;
    glutPostRedisplay();
}

// Mouse arrastando
static void onMotion(int x, int y) {
    const int dx = x - g_last_x;
    const int dy = y - g_last_y;
    if (g_lmb_down) {
        const float sens = 0.3f;
        g_ry += dx * sens;
        g_rx += dy * sens;
    } else if (g_rmb_down) {
        const float sens = 0.01f;
        g_tx += dx * sens;
        g_ty -= dy * sens;
    }
    g_last_x = x; g_last_y = y;
    glutPostRedisplay();
}

static void onIdle() {
    glutPostRedisplay();
}

// Ponto de entrada: inicializa GLUT, registra callbacks, carrega o modelo e textura
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(g_width, g_height);
    glutCreateWindow("Trabalho M1 - Rafael Mota e Kauan Adami");

    // Define preenchimento por padrão
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Declara callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(onKeyboard);
    glutSpecialFunc(onSpecial);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
    glutIdleFunc(onIdle);

    // Carrega OBJ se existir
    string caminho = (argc > 1 ? string(argv[1]) : string("data/elepham.obj"));
    if (arquivoExiste(caminho)) {
        // Usa o módulo obj_loader para ler v/vt/vn e criar a display list
        g_objLoaded = carregarOBJParaDisplayList(
            caminho,
            g_vertices, g_indices, g_vnormals, g_onormals, g_texcoords, g_triangulos,
            g_objList
        );
    } else {
        cerr << "Arquivo OBJ nao encontrado: " << caminho << " (mostrando cubo de teste)\n";
    }

    // Cria textura de teste
    criarTexturaXadrez();

    glutMainLoop();
    return 0;
}
