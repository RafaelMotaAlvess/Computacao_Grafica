#include <GL/freeglut.h>
#include <string>

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

    // Aplica transformações
    glTranslatef(g_tx, g_ty, g_tz);
    glRotatef(g_rx, 1,0,0);
    glRotatef(g_ry, 0,1,0);
    glRotatef(g_rz, 0,0,1);
    glScalef(g_scale, g_scale, g_scale);

    // Cubo
    drawCubeColored();

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

    // Declara callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(onKeyboard);
    glutSpecialFunc(onSpecial);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMotion);
    glutIdleFunc(onIdle);

    glutMainLoop();
    return 0;
}
