#include <GL/freeglut.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cstring>

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

static std::vector<float> g_vertices;       // posições: xyz intercalado
static std::vector<unsigned int> g_indices; // índices (por posição) 0-based, para cálculo de normais fallback
static std::vector<float> g_vnormals;       // normais calculadas por vértice (xyz intercalado)

// Dados vindos diretamente do OBJ
static std::vector<float> g_onormals;       // normais do arquivo (vn) xyz intercalado
static std::vector<float> g_texcoords;      // coordenadas de textura (vt) uv intercalado

// Triângulos (fan-triangulated) com índices separados para v, vt, vn
struct Corner { int v, vt, vn; };
static std::vector<Corner> g_triangles;     // a cada 3 entries = 1 tri

// Textura simples (procedural) para demonstrar mapeamento UV
static GLuint g_texID = 0;
static bool g_texEnabled = true;            // alternar com tecla 'T'

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

// Utilitário para converter índices OBJ (que podem ser negativos) para 0-based positivos
static int toIndex0Based(int idxFromObj, int count) {
    if (idxFromObj > 0) return idxFromObj - 1;     // 1..N -> 0..N-1
    if (idxFromObj < 0) return count + idxFromObj; // -1 -> último
    return -1; // 0 é inválido
}

// Cria textura xadrez (checkerboard) para demonstrar UVs sem depender de carregar imagem externa
static void createCheckerTexture(int w = 64, int h = 64, int check = 8) {
    if (g_texID != 0) {
        glDeleteTextures(1, &g_texID);
        g_texID = 0;
    }
    std::vector<unsigned char> img((size_t)w * h * 3u, 0);
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
    // Se houver suporte a mipmaps por extensão, podemos gerar mipmap
    // Em fixed-function antigo, há gluBuild2DMipmaps disponível via GLU
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, img.data());
}

// Carrega .obj com suporte a v, vt, vn e f (triangulação em fan)
static bool loadObjToDisplayList(const std::string& path) {
    g_vertices.clear();
    g_indices.clear();
    g_vnormals.clear();
    g_onormals.clear();
    g_texcoords.clear();
    g_triangles.clear();

    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Falha ao abrir OBJ: " << path << "\n";
        return false;
    }

    std::string line;
    std::vector<float> tempVerts; tempVerts.reserve(1000);
    std::vector<float> tempVNs;  tempVNs.reserve(1000);
    std::vector<float> tempVTs;  tempVTs.reserve(1000);
    std::vector<unsigned int> tempIdx; tempIdx.reserve(1000);

    auto parseCorner = [&](const std::string& s, int vcount, int vtcount, int vncount)->Corner {
        Corner c{ -1, -1, -1 };
        // Formatos possíveis: v | v/vt | v//vn | v/vt/vn
        // Vamos dividir até 3 partes
        int parts[3] = {0,0,0};
        int nparts = 0;
        // Copiar para buffer mutável
        char buf[128];
        std::memset(buf, 0, sizeof(buf));
        std::strncpy(buf, s.c_str(), sizeof(buf)-1);
        char* p = buf; char* save = nullptr;
        for (char* tok = ::strtok_r(p, "/", &save); tok != nullptr; tok = ::strtok_r(nullptr, "/", &save)) {
            if (nparts < 3) {
                parts[nparts] = std::atoi(tok);
            }
            ++nparts;
        }
        // Se havia // (v//vn) o strtok ignora vazio; precisamos detectar
        // Heurística: contar slashes diretos no string
        int slashCount = 0; for (char ch : s) if (ch == '/') ++slashCount;
        bool hasDoubleSlash = (slashCount >= 2 && nparts == 2 && s.find("//") != std::string::npos);
        if (nparts >= 1) {
            int v0 = toIndex0Based(parts[0], vcount);
            c.v = (v0 >= 0 && v0 < vcount) ? v0 : -1;
        }
        if (hasDoubleSlash) {
            // v//vn
            int vn0 = toIndex0Based(parts[1], vncount);
            c.vn = (vn0 >= 0 && vn0 < vncount) ? vn0 : -1;
        } else if (nparts == 2) {
            // v/vt
            int vt0 = toIndex0Based(parts[1], vtcount);
            c.vt = (vt0 >= 0 && vt0 < vtcount) ? vt0 : -1;
        } else if (nparts >= 3) {
            // v/vt/vn
            int vt0 = toIndex0Based(parts[1], vtcount);
            int vn0 = toIndex0Based(parts[2], vncount);
            c.vt = (vt0 >= 0 && vt0 < vtcount) ? vt0 : -1;
            c.vn = (vn0 >= 0 && vn0 < vncount) ? vn0 : -1;
        }
        return c;
    };

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string tok; ls >> tok;
        if (tok == "v") {
            float x=0,y=0,z=0; ls >> x >> y >> z;
            tempVerts.push_back(x); tempVerts.push_back(y); tempVerts.push_back(z);
        } else if (tok == "vn") {
            float x=0,y=0,z=0; ls >> x >> y >> z;
            tempVNs.push_back(x); tempVNs.push_back(y); tempVNs.push_back(z);
        } else if (tok == "vt") {
            float u=0,v=0; ls >> u >> v; // ignorar w se houver
            tempVTs.push_back(u); tempVTs.push_back(v);
        } else if (tok == "f") {
            // Lê todos os vértices da face e triangula em fan: (v0,v1,v2), (v0,v2,v3), ...
            std::vector<std::string> faceTokens;
            std::string fstr;
            while (ls >> fstr) faceTokens.push_back(fstr);
            if (faceTokens.size() < 3) continue;
            const int vcount  = (int)(tempVerts.size() / 3);
            const int vtcount = (int)(tempVTs.size()  / 2);
            const int vncount = (int)(tempVNs.size()  / 3);

            std::vector<Corner> corners; corners.reserve(faceTokens.size());
            for (const auto& s : faceTokens) corners.push_back(parseCorner(s, vcount, vtcount, vncount));

            // Coleta índices de posição (para cálculo de normais fallback)
            std::vector<int> vind; vind.reserve(corners.size());
            for (const Corner& c : corners) if (c.v >= 0) vind.push_back(c.v);

            // Triangula em fan utilizando os cantos completos (v,vt,vn)
            if ((int)corners.size() >= 3) {
                for (size_t k = 2; k < corners.size(); ++k) {
                    g_triangles.push_back(corners[0]);
                    g_triangles.push_back(corners[k-1]);
                    g_triangles.push_back(corners[k]);
                }
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
    g_onormals.swap(tempVNs);
    g_texcoords.swap(tempVTs);

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

    // Cria display list com triângulos preenchidos utilizando dados de OBJ (vn/vt) quando disponíveis
    g_objList = glGenLists(1);
    glNewList(g_objList, GL_COMPILE);
    glPushMatrix();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    glColor3f(1.0f, 1.0f, 1.0f); // branco para não interferir na textura (modulate)

    glBegin(GL_TRIANGLES);
    if (!g_triangles.empty()) {
        const bool hasVN = !g_onormals.empty();
        const bool hasVT = !g_texcoords.empty();
        for (size_t i = 0; i + 2 < g_triangles.size(); i += 3) {
            for (int corner = 0; corner < 3; ++corner) {
                const Corner& c = g_triangles[i + corner];
                // normal: preferir vn do arquivo; senão, usar normal por vértice calculada
                if (hasVN && c.vn >= 0) {
                    const float* N = &g_onormals[(size_t)c.vn * 3u];
                    glNormal3fv(N);
                } else if (c.v >= 0) {
                    const float* N = &g_vnormals[(size_t)c.v * 3u];
                    glNormal3fv(N);
                }
                // texcoord
                if (hasVT && c.vt >= 0) {
                    const float* T = &g_texcoords[(size_t)c.vt * 2u];
                    glTexCoord2fv(T);
                }
                // posição
                if (c.v >= 0) {
                    const float* P = &g_vertices[(size_t)c.v * 3u];
                    glVertex3fv(P);
                }
            }
        }
    } else {
        // Fallback (não deveria ocorrer se indices foram preenchidos), usa apenas os índices de posição
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
    }
    glEnd();
    glPopMatrix();
    glEndList();

    std::cout << "OBJ carregado: " << path
              << " | V: " << (g_vertices.size()/3)
              << " VT: " << (g_texcoords.size()/2)
              << " VN: " << (g_onormals.size()/3)
              << " | Tris: " << (g_triangles.size()/3) << "\n";
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
    line("T: Alternar textura ON/OFF");
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

    // Textura
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
        case 't': case 'T': g_texEnabled = !g_texEnabled; break;
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
    std::string path = (argc > 1 ? std::string(argv[1]) : std::string("data/teddy.obj"));
    if (fileExists(path)) {
        g_objLoaded = loadObjToDisplayList(path);
    } else {
        std::cerr << "Arquivo OBJ nao encontrado: " << path << " (mostrando cubo de teste)\n";
    }

    // Cria textura de teste
    createCheckerTexture();

    glutMainLoop();
    return 0;
}
