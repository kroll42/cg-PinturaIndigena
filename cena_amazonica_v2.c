#include <GL/glut.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>

// definir m_pi
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float eye_openness = 1.0;
float lip_pucker = 0.0; // Para o "biquinho"
bool wink = false, kiss = false;
Mix_Chunk* kissSound = NULL;
Mix_Music* waterLoop = NULL;

float fishX = 1.2; // Começando do lado direito
float fishWave = 0.0;

#define NUM_SMALL_FISH 5
float smallFishX[NUM_SMALL_FISH];
float smallFishY[NUM_SMALL_FISH];
bool smallFishDirection[NUM_SMALL_FISH]; // true = esquerda para direita, false = direita para esquerda

float waterWave = 0.0; // Para efeito de movimento da água

// Mutex para sincronização entre threads
pthread_mutex_t animation_mutex = PTHREAD_MUTEX_INITIALIZER;

void drawCircle(float cx, float cy, float r, int num_segments) {
    glBegin(GL_POLYGON);
    for (int i = 0; i < num_segments; i++) {
        float theta = 2.0f * 3.1415926f * i / num_segments;
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

void drawFish(float x, float y, float wave, bool facingLeft) {
    // Corpo com ondulação senoidal
    glColor3f(0.7, 0.1, 0.1); // Vermelho escuro
    glBegin(GL_POLYGON);
    for (int i = 0; i < 100; i++) {
        float angle = 2.0f * M_PI * i / 100;
        float dx = 0.25 * cosf(angle);
        float dy = 0.1 * sinf(angle) + 0.02 * sinf(10 * dx + wave);
        if (!facingLeft) dx = -dx; // Espelhar horizontalmente
        glVertex2f(x + dx, y + dy);
    }
    glEnd();

    // Cauda
    glColor3f(0.4, 0.0, 0.0);
    glBegin(GL_TRIANGLES);
    if (facingLeft) {
        glVertex2f(x - 0.25, y);
        glVertex2f(x - 0.35, y + 0.08);
        glVertex2f(x - 0.35, y - 0.08);
    } else {
        glVertex2f(x + 0.25, y);
        glVertex2f(x + 0.35, y + 0.08);
        glVertex2f(x + 0.35, y - 0.08);
    }
    glEnd();

    // Olho
    glColor3f(1, 1, 1);
    float eyeX = facingLeft ? x + 0.15 : x - 0.15;
    drawCircle(eyeX, y + 0.03, 0.015, 20);
    glColor3f(0, 0, 0);
    drawCircle(eyeX, y + 0.03, 0.008, 20);
}

void drawSmallFish(float x, float y, bool facingLeft) {
    glColor3f(0.2, 0.5, 0.8); // Azul claro
    glBegin(GL_POLYGON);
    for (int i = 0; i < 30; i++) {
        float angle = 2.0f * M_PI * i / 30;
        float dx = 0.08 * cosf(angle);
        float dy = 0.04 * sinf(angle);
        if (!facingLeft) dx = -dx;
        glVertex2f(x + dx, y + dy);
    }
    glEnd();

    // Cauda simples
    glBegin(GL_TRIANGLES);
    if (facingLeft) {
        glVertex2f(x - 0.08, y);
        glVertex2f(x - 0.12, y + 0.03);
        glVertex2f(x - 0.12, y - 0.03);
    } else {
        glVertex2f(x + 0.08, y);
        glVertex2f(x + 0.12, y + 0.03);
        glVertex2f(x + 0.12, y - 0.03);
    }
    glEnd();
}

void drawTrees() {
    // Árvore principal (já existente)
    glColor3f(0.4f, 0.2f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(-0.7, -0.2);
    glVertex2f(-0.65, -0.2);
    glVertex2f(-0.65, 0.6);
    glVertex2f(-0.7, 0.6);
    glEnd();

    // Copa da árvore principal
    glColor3f(0.0f, 0.6f, 0.0f);
    drawCircle(-0.675, 0.7, 0.15, 30);

    // Árvore 2
    glColor3f(0.4f, 0.2f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(0.5, -0.2);
    glVertex2f(0.55, -0.2);
    glVertex2f(0.55, 0.5);
    glVertex2f(0.5, 0.5);
    glEnd();
    glColor3f(0.0f, 0.6f, 0.0f);
    drawCircle(0.525, 0.6, 0.12, 30);

    // Árvore 3 (mais ao fundo)
    glColor3f(0.3f, 0.15f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(-0.3, -0.2);
    glVertex2f(-0.27, -0.2);
    glVertex2f(-0.27, 0.4);
    glVertex2f(-0.3, 0.4);
    glEnd();
    glColor3f(0.0f, 0.5f, 0.0f);
    drawCircle(-0.285, 0.5, 0.1, 30);

    // Árvore 4
    glColor3f(0.4f, 0.2f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(0.8, -0.2);
    glVertex2f(0.83, -0.2);
    glVertex2f(0.83, 0.45);
    glVertex2f(0.8, 0.45);
    glEnd();
    glColor3f(0.0f, 0.6f, 0.0f);
    drawCircle(0.815, 0.55, 0.11, 30);
}

void drawWater() {
    // Água com efeito de movimento
    glBegin(GL_QUAD_STRIP);
    for (float x = -1.0; x <= 1.0; x += 0.1) {
        float wave1 = 0.02 * sin(5 * x + waterWave);
        float wave2 = 0.015 * sin(8 * x + waterWave * 1.5);
        
        glColor3f(0.1f, 0.4f, 0.9f); // Azul mais escuro no fundo
        glVertex2f(x, -1.0);
        
        glColor3f(0.3f, 0.6f, 1.0f); // Azul mais claro na superfície
        glVertex2f(x, -0.2 + wave1 + wave2);
    }
    glEnd();
}

void drawBackground() {
    // Céu/floresta verde
    glColor3f(0.0f, 0.4f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(-1, 1);
    glVertex2f(1, 1);
    glVertex2f(1, -0.2);
    glVertex2f(-1, -0.2);
    glEnd();

    drawWater();
    drawTrees();
}

void drawWoman() {
    // ===================== CABELO =====================
    glColor3f(0.0, 0.0, 0.0); // Preto
    glBegin(GL_POLYGON);
    glVertex2f(-0.3, 0.3);
    glVertex2f(0.3, 0.3);
    glVertex2f(0.25, -0.4);
    glVertex2f(-0.25, -0.4);
    glEnd();

    // ===================== ROSTO =====================
    glColor3f(0.8, 0.6, 0.4); // Tom de pele mais próximo à imagem
    drawCircle(0.0, 0.0, 0.28, 100); // Cabeça

    // ===================== PESCOÇO =====================
    glColor3f(0.8, 0.6, 0.4);
    glBegin(GL_QUADS);
    glVertex2f(-0.08, -0.28);
    glVertex2f(0.08, -0.28);
    glVertex2f(0.06, -0.35);
    glVertex2f(-0.06, -0.35);
    glEnd();

    // ===================== OLHOS =====================
    glColor3f(1, 1, 1);
    drawCircle(-0.08, 0.06, 0.025, 50); // Olho esquerdo (sempre aberto)
    drawCircle(0.08, 0.06, 0.025, 50);  // Olho direito
    
    glColor3f(0, 0, 0);
    drawCircle(-0.08, 0.06, 0.015, 50); // Pupila esquerda
    
    // Olho direito que pisca
    glPushMatrix();
    glScalef(1.0, eye_openness, 1.0);
    drawCircle(0.08, 0.06, 0.015, 50); // Pupila direita
    glPopMatrix();

    // ===================== BOCA COM BIQUINHO =====================
    glColor3f(0.8, 0.2, 0.2);
    if (lip_pucker > 0) {
        // Boca fazendo biquinho
        drawCircle(0.0, -0.13, 0.03 + lip_pucker * 0.02, 20);
    } else {
        // Boca normal
        glBegin(GL_POLYGON);
        glVertex2f(-0.05, -0.12);
        glVertex2f(0.00, -0.14);
        glVertex2f(0.05, -0.12);
        glEnd();
    }

    // ===================== BRAÇOS =====================
    glColor3f(0.8, 0.6, 0.4);
    // Braço esquerdo
    glBegin(GL_QUADS);
    glVertex2f(-0.22, -0.35);
    glVertex2f(-0.18, -0.35);
    glVertex2f(-0.35, -0.7);
    glVertex2f(-0.39, -0.7);
    glEnd();
    
    // Braço direito
    glBegin(GL_QUADS);
    glVertex2f(0.18, -0.35);
    glVertex2f(0.22, -0.35);
    glVertex2f(0.39, -0.7);
    glVertex2f(0.35, -0.7);
    glEnd();

    // Mãos
    drawCircle(-0.37, -0.72, 0.04, 20);
    drawCircle(0.37, -0.72, 0.04, 20);

    // ===================== CORPO =====================
    glColor3f(0.8, 0.6, 0.4);
    glBegin(GL_POLYGON);
    glVertex2f(-0.22, -0.35);
    glVertex2f(0.22, -0.35);
    glVertex2f(0.18, -0.65);
    glVertex2f(-0.18, -0.65);
    glEnd();

    // ===================== TOP DE COURO (POSIÇÃO AJUSTADA) =====================
    glColor3f(0.4, 0.2, 0.1); // Marrom escuro
    glBegin(GL_QUADS);
    glVertex2f(-0.15, -0.35);
    glVertex2f(0.15, -0.35);
    glVertex2f(0.10, -0.45);
    glVertex2f(-0.10, -0.45);
    glEnd();

    // ===================== PERNAS =====================
    glColor3f(0.8, 0.6, 0.4);
    // Perna esquerda
    glBegin(GL_QUADS);
    glVertex2f(-0.12, -0.65);
    glVertex2f(-0.08, -0.65);
    glVertex2f(-0.10, -0.95);
    glVertex2f(-0.14, -0.95);
    glEnd();
    
    // Perna direita
    glBegin(GL_QUADS);
    glVertex2f(0.08, -0.65);
    glVertex2f(0.12, -0.65);
    glVertex2f(0.14, -0.95);
    glVertex2f(0.10, -0.95);
    glEnd();

    // Pés
    glColor3f(0.7, 0.5, 0.3);
    glBegin(GL_LINES);
    drawCircle(-0.12, -0.97, 0.03, 20);
    drawCircle(0.12, -0.97, 0.03, 20);

    // ===================== SAIA DE PALHA (POSIÇÃO AJUSTADA) =====================
    glColor3f(0.6, 0.4, 0.1); // Marrom claro
    for (float i = -0.18; i < 0.18; i += 0.02) {
        glBegin(GL_TRIANGLES);
        glVertex2f(i, -0.45);
        glVertex2f(i + 0.01, -0.65);
        glVertex2f(i + 0.02, -0.45);
        glEnd();
    }

    // ===================== TATUAGENS =====================
    glColor3f(0.2, 0.1, 0.05);
    glBegin(GL_LINES);
    // Tatuagens no braço esquerdo
    for (float y = -0.4; y > -0.6; y -= 0.05) {
        glVertex2f(-0.25, y);
        glVertex2f(-0.15, y);
    }
    // Tatuagens no braço direito
    for (float y = -0.4; y > -0.6; y -= 0.05) {
        glVertex2f(0.15, y);
        glVertex2f(0.25, y);
    }
    glEnd();

    // ===================== BRINCO/PENA =====================
    glColor3f(1, 1, 0); // Base
    drawCircle(-0.25, 0.0, 0.03, 20);

    glColor3f(1, 0, 0); // Pena vermelha
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.26, -0.03);
    glVertex2f(-0.24, -0.03);
    glVertex2f(-0.25, -0.08);
    glEnd();

    glColor3f(0, 0, 1); // Pena azul
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.26, -0.05);
    glVertex2f(-0.24, -0.05);
    glVertex2f(-0.25, -0.11);
    glEnd();

    // ===================== FAIXA NA CABEÇA =====================
    glColor3f(0.9, 0.9, 0.2);
    glBegin(GL_QUADS);
    glVertex2f(-0.2, 0.2);
    glVertex2f(0.2, 0.2);
    glVertex2f(0.18, 0.18);
    glVertex2f(-0.18, 0.18);
    glEnd();
}

void display() {
    pthread_mutex_lock(&animation_mutex);
    
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    drawBackground();

    drawFish(fishX, -0.4, fishWave, false); // Aruanã se movendo para a esquerda

    for (int i = 0; i < NUM_SMALL_FISH; i++) {
        drawSmallFish(smallFishX[i], smallFishY[i], smallFishDirection[i]);
    }

    drawWoman(); // Sempre por cima

    pthread_mutex_unlock(&animation_mutex);
    glutSwapBuffers();
}

// Função de timer para animação contínua
void timer(int value) {
    pthread_mutex_lock(&animation_mutex);
    
    // Aruanã se movendo para a esquerda
    fishX -= 0.002;
    if (fishX < -1.2) fishX = 1.2;
    fishWave += 0.1;

    // Peixes pequenos - metade para cada lado
    for (int i = 0; i < NUM_SMALL_FISH; i++) {
        if (smallFishDirection[i]) {
            // Movendo para a direita
            smallFishX[i] += 0.003 + 0.0005 * i;
            if (smallFishX[i] > 1.2) smallFishX[i] = -1.5 - 0.2 * i;
        } else {
            // Movendo para a esquerda
            smallFishX[i] -= 0.003 + 0.0005 * i;
            if (smallFishX[i] < -1.2) smallFishX[i] = 1.5 + 0.2 * i;
        }
        smallFishY[i] = -0.5 + 0.3 * sin(smallFishX[i] * 3 + i);
    }
    
    // Efeito de movimento da água
    waterWave += 0.05;
    
    pthread_mutex_unlock(&animation_mutex);
    
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); // ~60 FPS
}

void* animate(void* arg) {
    while (1) {
        pthread_mutex_lock(&animation_mutex);
        
        if (wink) {
            eye_openness = 0.2;
            pthread_mutex_unlock(&animation_mutex);
            usleep(300000);
            pthread_mutex_lock(&animation_mutex);
            eye_openness = 1.0;
            wink = false;
            glutPostRedisplay();
        }
        
        if (kiss) {
            lip_pucker = 0.8;
            if (kissSound) Mix_PlayChannel(-1, kissSound, 0);
            pthread_mutex_unlock(&animation_mutex);
            usleep(400000);
            pthread_mutex_lock(&animation_mutex);
            lip_pucker = 0.0;
            kiss = false;
            glutPostRedisplay();
        }
        
        pthread_mutex_unlock(&animation_mutex);
        usleep(50000); // Checa ações a cada 50ms
    }
    return NULL;
}

void keyboard(unsigned char key, int x, int y) {
    pthread_mutex_lock(&animation_mutex);
    if (key == 'w' || key == 'W') {
        wink = true;
        printf("Piscada ativada!\n"); // Debug
    }
    if (key == 'k' || key == 'K') {
        kiss = true;
        printf("Beijinho ativado!\n"); // Debug
    }
    pthread_mutex_unlock(&animation_mutex);
}

void initSound() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Erro no SDL_mixer: %s\n", Mix_GetError());
        // Não sair do programa se não conseguir carregar o áudio
    } else {
        kissSound = Mix_LoadWAV("kiss.wav");
        waterLoop = Mix_LoadMUS("water.wav");

        if (!kissSound) {
            printf("Aviso: Não foi possível carregar kiss.wav\n");
        }
        if (!waterLoop) {
            printf("Aviso: Não foi possível carregar water.wav\n");
        } else {
            Mix_PlayMusic(waterLoop, -1); // Loop infinito
        }
    }
}

void initFish() {
    // Inicializar posições dos peixes pequenos
    for (int i = 0; i < NUM_SMALL_FISH; i++) {
        if (i < NUM_SMALL_FISH / 2) {
            // Primeira metade se move da esquerda para direita
            smallFishX[i] = -1.5 - 0.3 * i;
            smallFishDirection[i] = true;
        } else {
            // Segunda metade se move da direita para esquerda
            smallFishX[i] = 1.5 + 0.3 * (i - NUM_SMALL_FISH / 2);
            smallFishDirection[i] = false;
        }
        smallFishY[i] = -0.5 + 0.3f * sin(i); // Posição vertical variada na água
    }
}

void initLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat lightPos[] = { 1.0f, 1.0f, 1.0f, 0.0f }; // Direcional
    GLfloat lightColor[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
}

int main(int argc, char** argv) {
    // GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Mulher Indígena Animada - Versão Melhorada");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);
    initLighting();
    glMatrixMode(GL_MODELVIEW);

    // SDL
    SDL_Init(SDL_INIT_AUDIO);
    initSound();
    
    // Inicializar peixes ANTES do loop principal
    initFish();

    // Eventos
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    
    // Usar timer do GLUT para animação contínua
    glutTimerFunc(16, timer, 0);

    // Thread para ações específicas (piscar/beijar)
    pthread_t animThread;
    pthread_create(&animThread, NULL, animate, NULL);

    printf("Pressione 'w' para piscar o olho direito, 'k' para beijinho\n");

    glutMainLoop();

    // Limpeza
    if (kissSound) Mix_FreeChunk(kissSound);
    if (waterLoop) Mix_FreeMusic(waterLoop);
    Mix_CloseAudio();
    SDL_Quit();
    pthread_mutex_destroy(&animation_mutex);

    return 0;
}