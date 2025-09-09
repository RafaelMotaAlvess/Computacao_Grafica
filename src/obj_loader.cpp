#include "obj_loader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

// Para simplificar leitura do código neste trabalho acadêmico
using namespace std;

// Converte índice do OBJ (pode ser negativo) para 0-based
static int idx0(int idxFromObj, int count) {
    if (idxFromObj > 0) return idxFromObj - 1;
    if (idxFromObj < 0) return count + idxFromObj;
    return -1;
}

// Normal de face
static void calcularNormalFace(const float* a, const float* b, const float* c, float n[3]) {
    const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    n[0] = uy * vz - uz * vy;
    n[1] = uz * vx - ux * vz;
    n[2] = ux * vy - uy * vx;
    const float len = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len > 1e-8f) { n[0]/=len; n[1]/=len; n[2]/=len; }
}

// Divide uma string de face "v/vt/vn" em até 3 partes (pode ficar vazia em v//vn)
static void dividirPorBarra(const string& s, string& a, string& b, string& c) {
    size_t p1 = s.find('/');
    if (p1 == string::npos) {
        a = s; b.clear(); c.clear(); return;
    }
    a = s.substr(0, p1);
    size_t p2 = s.find('/', p1 + 1);
    if (p2 == string::npos) {
        b = s.substr(p1 + 1);
        c.clear();
        return;
    }
    b = s.substr(p1 + 1, p2 - (p1 + 1));
    c = s.substr(p2 + 1);
}

// Converte um token textual para inteiro (retorna false se vazio ou inválido)
static bool lerInt(const string& s, int& out) {
    if (s.empty()) return false;
    try { out = stoi(s); return true; } catch (...) { return false; }
}

// Faz o parse de um token de face: "v", "v/vt", "v//vn" ou "v/vt/vn"
static CantoTri parseCanto(const string& s, int vcount, int vtcount, int vncount) {
    CantoTri canto{ -1, -1, -1 };
    string sv, st, sn;
    dividirPorBarra(s, sv, st, sn);
    int iv = 0, it = 0, in = 0;
    if (lerInt(sv, iv)) canto.v  = idx0(iv, vcount);
    if (lerInt(st, it)) canto.vt = idx0(it, vtcount);
    if (lerInt(sn, in)) canto.vn = idx0(in, vncount);
    if (canto.v  < 0 || canto.v  >= vcount)  canto.v = -1;
    if (canto.vt < 0 || canto.vt >= vtcount) canto.vt = -1;
    if (canto.vn < 0 || canto.vn >= vncount) canto.vn = -1;
    return canto;
}

bool carregarOBJParaDisplayList(
    const string& caminho,
    vector<float>& vertices,
    vector<unsigned int>& indicesPos,
    vector<float>& normaisCalculadas,
    vector<float>& normaisOBJ,
    vector<float>& uvs,
    vector<CantoTri>& triangulos,
    GLuint& displayListOut
) {
    // Limpa saídas
    vertices.clear(); indicesPos.clear(); normaisCalculadas.clear();
    normaisOBJ.clear(); uvs.clear(); triangulos.clear();

    ifstream in(caminho);
    if (!in.is_open()) {
        cerr << "Falha ao abrir OBJ: " << caminho << "\n";
        return false;
    }

    string line;
    vector<float> tempVerts; tempVerts.reserve(1000);
    vector<float> tempVNs;  tempVNs.reserve(1000);
    vector<float> tempVTs;  tempVTs.reserve(1000);
    vector<unsigned int> tempIdx; tempIdx.reserve(1000);

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream ls(line);
        string tok; ls >> tok;
        if (tok == "v") {
            float x=0,y=0,z=0; ls >> x >> y >> z; tempVerts.push_back(x); tempVerts.push_back(y); tempVerts.push_back(z);
        } else if (tok == "vn") {
            float x=0,y=0,z=0; ls >> x >> y >> z; tempVNs.push_back(x); tempVNs.push_back(y); tempVNs.push_back(z);
        } else if (tok == "vt") {
            float u=0,v=0; ls >> u >> v; tempVTs.push_back(u); tempVTs.push_back(v);
        } else if (tok == "f") {
            vector<string> faceTokens; string fstr; while (ls >> fstr) faceTokens.push_back(fstr);
            if (faceTokens.size() < 3) continue;
            const int vcount  = (int)(tempVerts.size() / 3);
            const int vtcount = (int)(tempVTs.size()  / 2);
            const int vncount = (int)(tempVNs.size()  / 3);
            vector<CantoTri> corners; corners.reserve(faceTokens.size());
            for (const auto& s : faceTokens) corners.push_back(parseCanto(s, vcount, vtcount, vncount));
            // Guardar triângulos (fan)
            if ((int)corners.size() >= 3) {
                for (size_t k = 2; k < corners.size(); ++k) {
                    triangulos.push_back(corners[0]);
                    triangulos.push_back(corners[k-1]);
                    triangulos.push_back(corners[k]);
                }
            }
            // Guardar apenas índices de posição para cálculo de normais
            vector<int> vind; vind.reserve(corners.size());
            for (const auto& c : corners) if (c.v >= 0) vind.push_back(c.v);
            if ((int)vind.size() >= 3) {
                for (size_t k = 2; k < vind.size(); ++k) {
                    tempIdx.push_back((unsigned int)vind[0]);
                    tempIdx.push_back((unsigned int)vind[k-1]);
                    tempIdx.push_back((unsigned int)vind[k]);
                }
            }
        }
    }
    in.close();

    if (tempVerts.empty() || tempIdx.empty()) {
    cerr << "OBJ vazio ou sem faces: " << caminho << "\n";
        return false;
    }

    vertices.swap(tempVerts);
    indicesPos.swap(tempIdx);
    normaisOBJ.swap(tempVNs);
    uvs.swap(tempVTs);

    // Normais por vértice (soma de normais de face, depois normaliza)
    normaisCalculadas.assign(vertices.size(), 0.0f);
    for (size_t i = 0; i + 2 < indicesPos.size(); i += 3) {
        const unsigned int ia = indicesPos[i+0] * 3u;
        const unsigned int ib = indicesPos[i+1] * 3u;
        const unsigned int ic = indicesPos[i+2] * 3u;
        if (ia+2 >= vertices.size() || ib+2 >= vertices.size() || ic+2 >= vertices.size()) continue;
        const float* A = &vertices[ia];
        const float* B = &vertices[ib];
        const float* C = &vertices[ic];
        float n[3]; calcularNormalFace(A, B, C, n);
        normaisCalculadas[ia+0] += n[0]; normaisCalculadas[ia+1] += n[1]; normaisCalculadas[ia+2] += n[2];
        normaisCalculadas[ib+0] += n[0]; normaisCalculadas[ib+1] += n[1]; normaisCalculadas[ib+2] += n[2];
        normaisCalculadas[ic+0] += n[0]; normaisCalculadas[ic+1] += n[1]; normaisCalculadas[ic+2] += n[2];
    }
    for (size_t v = 0; v + 2 < normaisCalculadas.size(); v += 3) {
        float nx = normaisCalculadas[v+0], ny = normaisCalculadas[v+1], nz = normaisCalculadas[v+2];
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-8f) { normaisCalculadas[v+0] = nx/len; normaisCalculadas[v+1] = ny/len; normaisCalculadas[v+2] = nz/len; }
        else { normaisCalculadas[v+0] = 0; normaisCalculadas[v+1] = 0; normaisCalculadas[v+2] = 1; }
    }

    // Display list com preenchimento, usando vn/vt quando existem
    if (displayListOut != 0) {
        glDeleteLists(displayListOut, 1);
        displayListOut = 0;
    }
    displayListOut = glGenLists(1);
    glNewList(displayListOut, GL_COMPILE);
    glPushMatrix();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_TRIANGLES);
    if (!triangulos.empty()) {
        const bool hasVN = !normaisOBJ.empty();
        const bool hasVT = !uvs.empty();
        for (size_t i = 0; i + 2 < triangulos.size(); i += 3) {
            for (int k = 0; k < 3; ++k) {
                const CantoTri& c = triangulos[i+k];
                if (hasVN && c.vn >= 0) {
                    const float* N = &normaisOBJ[(size_t)c.vn * 3u]; glNormal3fv(N);
                } else if (c.v >= 0) {
                    const float* N = &normaisCalculadas[(size_t)c.v * 3u]; glNormal3fv(N);
                }
                if (hasVT && c.vt >= 0) {
                    const float* T = &uvs[(size_t)c.vt * 2u]; glTexCoord2fv(T);
                }
                if (c.v >= 0) {
                    const float* P = &vertices[(size_t)c.v * 3u]; glVertex3fv(P);
                }
            }
        }
    }
    glEnd();
    glPopMatrix();
    glEndList();

    cout << "OBJ carregado: " << caminho
              << " | V: " << (vertices.size()/3)
              << " VT: " << (uvs.size()/2)
              << " VN: " << (normaisOBJ.size()/3)
              << " | Tris: " << (triangulos.size()/3) << "\n";

    return true;
}
