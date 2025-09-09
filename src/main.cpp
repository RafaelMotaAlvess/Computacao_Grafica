#include <GL/freeglut.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

static void drawCubeColored();

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

static std::vector<float> g_vertices; // xyz intercalado
static std::vector<unsigned int> g_indices; // triplas de índices 0-based
static std::vector<float> g_vnormals; // normais por vértice (xyz intercalado)
static GLuint g_objList = 0;
static bool g_objLoaded = false;

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static void computeFaceNormal(const float* a, const float* b, const float* c, float n[3]) {
    const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    n[0] = uy * vz - uz * vy;
    n[1] = uz * vx - ux * vz;
    n[2] = ux * vy - uy * vx;
    const float len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len > 1e-8f) { n[0]/=len; n[1]/=len; n[2]/=len; }
}

// Carrega .obj básico (apenas 'v' e 'f' triangulares). Faces com slashes são suportadas.
static bool loadObjToDisplayList(const std::string& path) {
    g_vertices.clear();
    g_indices.clear();
    g_vnormals.clear();

    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Falha ao abrir OBJ: " << path << "\n";
        return false;
    }

    std::string line;
    std::vector<float> tempVerts; tempVerts.reserve(1000);
    std::vector<unsigned int> tempIdx; tempIdx.reserve(1000);

    auto parseIndex = [&](const std::string& s, int vcount)->int {
        size_t slash = s.find('/');
        std::string sub = (slash == std::string::npos ? s : s.substr(0, slash));
        if (sub.empty()) return -1;
        int idx1 = 0;
        try { idx1 = std::stoi(sub); } catch (...) { return -1; }
        int idx0 = 0;
        if (idx1 > 0) idx0 = idx1 - 1;            // 1..N -> 0..N-1
        else if (idx1 < 0) idx0 = vcount + idx1;  // -1 é o último
        else return -1; // 0 não é válido
        if (idx0 < 0 || idx0 >= vcount) return -1;
        return idx0;
    };

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string tok; ls >> tok;
        if (tok == "v") {
            float x=0,y=0,z=0; ls >> x >> y >> z;
            tempVerts.push_back(x); tempVerts.push_back(y); tempVerts.push_back(z);
        } else if (tok == "f") {
            // Lê todos os vértices da face e triangula em fan: (v0,v1,v2), (v0,v2,v3), ...
            std::vector<std::string> faceTokens;
            std::string fstr;
            while (ls >> fstr) faceTokens.push_back(fstr);
            if (faceTokens.size() < 3) continue;
            std::vector<int> vind; vind.reserve(faceTokens.size());
            const int vcount = (int)(tempVerts.size() / 3);
            for (const auto& s : faceTokens) {
                int idx0 = parseIndex(s, vcount);
                if (idx0 >= 0) vind.push_back(idx0);
            }
            if ((int)vind.size() >= 3) {
                for (size_t k = 2; k < vind.size(); ++k) {
                    tempIdx.push_back((unsigned int)vind[0]);
                    tempIdx.push_back((unsigned int)vind[k-1]);
                    tempIdx.push_back((unsigned int)vind[k]);
                }
            }
        } else {
            // Ignora outras linhas
        }
    }
    in.close();

    if (tempVerts.empty() || tempIdx.empty()) {
        std::cerr << "OBJ vazio ou sem faces: " << path << "\n";
        return false;
    }

    g_vertices.swap(tempVerts);
    g_indices.swap(tempIdx);

    // Calcula normais por vértice (média ponderada por área das faces adjacentes)
    g_vnormals.assign(g_vertices.size(), 0.0f);
    for (size_t i = 0; i + 2 < g_indices.size(); i += 3) {
        const unsigned int ia = g_indices[i+0] * 3u;
        const unsigned int ib = g_indices[i+1] * 3u;
        const unsigned int ic = g_indices[i+2] * 3u;
        if (ia+2 >= g_vertices.size() || ib+2 >= g_vertices.size() || ic+2 >= g_vertices.size()) continue;
        const float* A = &g_vertices[ia];
        const float* B = &g_vertices[ib];
        const float* C = &g_vertices[ic];
        float n[3]; computeFaceNormal(A, B, C, n);
        g_vnormals[ia+0] += n[0]; g_vnormals[ia+1] += n[1]; g_vnormals[ia+2] += n[2];
        g_vnormals[ib+0] += n[0]; g_vnormals[ib+1] += n[1]; g_vnormals[ib+2] += n[2];
        g_vnormals[ic+0] += n[0]; g_vnormals[ic+1] += n[1]; g_vnormals[ic+2] += n[2];
    }
    for (size_t v = 0; v + 2 < g_vnormals.size(); v += 3) {
        float nx = g_vnormals[v+0], ny = g_vnormals[v+1], nz = g_vnormals[v+2];
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-8f) { g_vnormals[v+0] = nx/len; g_vnormals[v+1] = ny/len; g_vnormals[v+2] = nz/len; }
        else { g_vnormals[v+0] = 0; g_vnormals[v+1] = 0; g_vnormals[v+2] = 1; }
    }

    if (g_objList != 0) {
        glDeleteLists(g_objList, 1);
        g_objList = 0;
    }

    // Cria display list com triângulos preenchidos e normais por VÉRTICE (suavização)
    g_objList = glGenLists(1);
    glNewList(g_objList, GL_COMPILE);
    glPushMatrix();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    glColor3f(0.85f, 0.85f, 0.9f);

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i + 2 < g_indices.size(); i += 3) {
        const unsigned int ia = g_indices[i+0];
        const unsigned int ib = g_indices[i+1];
        const unsigned int ic = g_indices[i+2];
        const float* A = &g_vertices[ia*3u];
        const float* B = &g_vertices[ib*3u];
        const float* C = &g_vertices[ic*3u];
        const float* Na = &g_vnormals[ia*3u];
        const float* Nb = &g_vnormals[ib*3u];
        const float* Nc = &g_vnormals[ic*3u];
        glNormal3fv(Na); glVertex3fv(A);
        glNormal3fv(Nb); glVertex3fv(B);
        glNormal3fv(Nc); glVertex3fv(C);
    }
    glEnd();
    glPopMatrix();
    glEndList();

    std::cout << "OBJ carregado: " << path << " | Verts: " << (g_vertices.size()/3) << " Tris: " << (g_indices.size()/3) << "\n";
    return true;
}

static void drawOBJorFallback() {
    if (g_objLoaded && g_objList != 0) {
        // Apenas desenha o OBJ; configuração de luz fica no display()
        glCallList(g_objList);
    } else {
        // Sem OBJ, desenha o cubo padrão
        drawCubeColored();
    }
}

static void drawBitmapText2D(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    for (unsigned char c : text) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, c);
}

static void resetTransform() {
    g_tx = 0.0f; g_ty = 0.0f; g_tz = -3.0f;
    g_rx = 0.0f; g_ry = 0.0f; g_rz = 0.0f;
    g_scale = 1.0f;
}

// Desenha um cubo colorido (apenas para testes)
static void drawCubeColored() {
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

static void drawAxesGizmo() {
    // Pequena viewport  para orientação de X Y Z no canto superior direito
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
    auto line = [&](const std::string& s){ glColor3f(1,1,1); drawBitmapText2D((float)x, (float)y, s); y -= 18; };

    line("Comandos:");
    line("W/S: Transladar +Y/-Y");
    line("A/D: Transladar -X/+X");
    line("Q/E: Aproximar/Afastar (Z)");
    line("Setas: Rotacionar em X/Y");
    line("Z/X: Rotacionar em Z");
    line("+/−: Aumentar/Diminuir escala");
    line("Mouse Esq: arrastar p/ rotacionar");
    line("Mouse Dir: arrastar p/ transladar");
    line("Scroll: Aproximar/Afastar");
    line("R: Resetar");

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
}

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

    // Aplica transformações
    glTranslatef(g_tx, g_ty, g_tz);
    glRotatef(g_rx, 1,0,0);
    glRotatef(g_ry, 0,1,0);
    glRotatef(g_rz, 0,0,1);
    glScalef(g_scale, g_scale, g_scale);

    // Dsenha o arquivo obj
    drawOBJorFallback();

    // Gizmo de eixos (fixo na tela)
    drawAxesGizmo();

    // Overlay de ajuda
    drawHelpOverlay();

    glutSwapBuffers();
}

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
    std::string path = (argc > 1 ? std::string(argv[1]) : std::string("data/porsche.obj"));
    if (fileExists(path)) {
        g_objLoaded = loadObjToDisplayList(path);
    } else {
        std::cerr << "Arquivo OBJ nao encontrado: " << path << " (mostrando cubo de teste)\n";
    }

    glutMainLoop();
    return 0;
}
