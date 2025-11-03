#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace std;
using namespace glm;

// ---- Game Constants ----
const int GRID_W = 10;
const int GRID_H = 20;
const int GRID_D = 10;
const int GAME_OVER_HEIGHT = 14;
const float CUBE_SIZE = 1.0f;
float fallSpeed = 0.6f;

// ---- Game State ----
struct Cube {
    vec3 pos;
    vec3 albedo;
    float metallic;
    float roughness;
};
vector<Cube> grid;
vector<Cube> currentPiece;
vec3 pieceCenter;
bool paused = false, gameOver = false;
int score = 0;
float lastFall = 0.0f;

// --- NEW: PBR Material System ---
struct PBRMaterial {
    string name;
    vec3 albedo;
    float metallic;
    float roughness;
};

const vector<PBRMaterial> materials = {
    {"Plastic", vec3(0.8f, 0.1f, 0.1f), 0.0f, 0.5f},
    {"Gold",    vec3(1.0f, 0.71f, 0.29f), 1.0f, 0.2f},
    {"Jade",    vec3(0.54f, 0.89f, 0.63f), 0.1f, 0.1f},
    {"Copper",  vec3(0.95f, 0.64f, 0.54f), 1.0f, 0.35f},
    {"Rubber",  vec3(0.1f, 0.1f, 0.1f), 0.0f, 0.9f},
};
int currentMaterialIndex = 0;


// ---- Rendering ----
GLuint cubeShaderProgram;
GLuint simpleShaderProgram;
GLuint cubeVAO = 0, cubeVBO = 0;
GLuint gridVAO = 0, gridVBO = 0;
GLuint limitPlaneVAO = 0, limitPlaneVBO = 0;

// ---- Camera ----
float camAngle = 0.5f;
float camDist = 25.0f;
float camHeight = 15.0f;

// ---- Window ----
int windowWidth = 1280;
int windowHeight = 800;

// ---- Tetromino Shapes ----
const vector<vector<vec3>> TETROMINOES = {
    {vec3(0,0,0), vec3(1,0,0), vec3(-1,0,0), vec3(-2,0,0)},
    {vec3(0,0,0), vec3(-1,0,0), vec3(1,0,0), vec3(1,-1,0)},
    {vec3(0,0,0), vec3(-1,0,0), vec3(1,0,0), vec3(-1,-1,0)},
    {vec3(0,0,0), vec3(1,0,0), vec3(-1,0,0), vec3(0,-1,0)},
    {vec3(0,0,0), vec3(0,-1,0), vec3(-1,-1,0), vec3(1,0,0)},
    {vec3(0,0,0), vec3(0,-1,0), vec3(1,-1,0), vec3(-1,0,0)},
    {vec3(0,0,0), vec3(1,0,0), vec3(0,-1,0), vec3(1,-1,0)}
};

// ---- Function Prototypes ----
string loadShaderFromFile(const string& filePath);
GLuint compileShader(GLenum type, const char* src);
GLuint makeProgram(const string& vertexPath, const string& fragmentPath, bool isCubeShader);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void applyMaterialToCurrentPiece();
void spawnPiece();
bool checkCollision(vec3 moveDir, const vector<Cube>& piece);
void mergePiece();
void updateScoreDisplay();
void initGrid();
void initLimitPlane();
void initCube();
void drawCube(const Cube& cube, const mat4& view, const mat4& proj);
void drawGrid(const mat4& view, const mat4& proj);
void drawLimitPlane(const mat4& view, const mat4& proj);

// ---- Main Loop ----
int main() {
    srand(time(0));

    if (!glfwInit()) { cerr << "Failed to initialize GLFW" << endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "3D Tetris (PBR)", NULL, NULL);
    if (!window) { cerr << "Failed to create window" << endl; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { cerr << "Failed to init GLAD" << endl; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    cubeShaderProgram = makeProgram("shaders/pbr.vert", "shaders/pbr.frag", true);
    simpleShaderProgram = makeProgram("shaders/simple.vert", "shaders/simple.frag", false);

    initCube();
    initGrid();
    initLimitPlane();

    cout << "--- 3D Tetris (PBR Edition) ---" << endl;
    cout << "Controls: WASD/Arrows (Move), QE (Rotate), JL (Camera), M (Material), Enter (Hard Drop)" << endl;
    updateScoreDisplay();
    cout << "\nMaterial: " << materials[currentMaterialIndex].name << flush;

    vec3 lightPositions[] = {
        vec3(GRID_W / 2.0f, GRID_H, GRID_D / 2.0f),
        vec3(GRID_W + 5.0f, 5.0f, GRID_D / 2.0f),
        vec3(-5.0f, 5.0f, GRID_D / 2.0f),
        vec3(GRID_W / 2.0f, 5.0f, GRID_D + 5.0f)
    };
    vec3 lightColors[] = {
        vec3(150.0f, 150.0f, 150.0f),
        vec3(50.0f, 50.0f, 0.0f),
        vec3(0.0f, 50.0f, 50.0f),
        vec3(50.0f, 0.0f, 50.0f)
    };

    glUseProgram(cubeShaderProgram);
    glUniform3fv(glGetUniformLocation(cubeShaderProgram, "lightPositions"), 4, value_ptr(lightPositions[0]));
    glUniform3fv(glGetUniformLocation(cubeShaderProgram, "lightColors"), 4, value_ptr(lightColors[0]));

    spawnPiece();

    while (!glfwWindowShouldClose(window)) {
        if (!paused && !gameOver && (float)glfwGetTime() - lastFall > fallSpeed) {
            if (checkCollision(vec3(0, -1, 0), currentPiece)) {
                mergePiece();
                if (!gameOver) spawnPiece();
            } else {
                for (auto &b : currentPiece) b.pos.y -= 1.0f;
                pieceCenter.y -= 1.0f;
            }
            lastFall = (float)glfwGetTime();
        }

        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        vec3 camPos = vec3(camDist * cos(camAngle), camHeight, camDist * sin(camAngle));
        mat4 view = lookAt(camPos, vec3(GRID_W/2.0f, GAME_OVER_HEIGHT/2.0f, GRID_D/2.0f), vec3(0, 1, 0));
        float aspectRatio = (float)windowWidth / (float)windowHeight;
        mat4 proj = perspective(radians(45.0f), aspectRatio, 0.1f, 100.0f);

        glUseProgram(cubeShaderProgram);
        glUniform3fv(glGetUniformLocation(cubeShaderProgram, "viewPos"), 1, value_ptr(camPos));

        drawGrid(view, proj);
        drawLimitPlane(view, proj);
        for (const auto &b : grid) {
            drawCube(b, view, proj);
        }
        if(!gameOver) {
            for (const auto &b : currentPiece) {
                drawCube(b, view, proj);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO); glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &gridVAO); glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &limitPlaneVAO); glDeleteBuffers(1, &limitPlaneVBO);
    glDeleteProgram(cubeShaderProgram);
    glDeleteProgram(simpleShaderProgram);
    glfwTerminate();
    cout << endl;
    return 0;
}

// ---- Helper and Gameplay Functions ----

string loadShaderFromFile(const string& filePath) {
    ifstream shaderFile;
    shaderFile.exceptions(ifstream::failbit | ifstream::badbit);
    try {
        shaderFile.open(filePath);
        stringstream shaderStream;
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
        return shaderStream.str();
    } catch (ifstream::failure& e) {
        cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << filePath << endl;
        return "";
    }
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, NULL, log); cerr << "Shader compilation error in " << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << " shader:\n" << log << endl; }
    return s;
}

GLuint makeProgram(const string& vertexPath, const string& fragmentPath, bool isCubeShader) {
    string vertexCode = loadShaderFromFile(vertexPath);
    string fragmentCode = loadShaderFromFile(fragmentPath);
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    GLuint v = compileShader(GL_VERTEX_SHADER, vShaderCode);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fShaderCode);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);

    if (isCubeShader) {
        glBindAttribLocation(prog, 0, "aPos");
        glBindAttribLocation(prog, 1, "aNormal");
    } else {
        glBindAttribLocation(prog, 0, "aPos");
    }

    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(prog, 1024, NULL, log); cerr << "Shader linking error:\n" << log << endl; }
    glDeleteShader(v); glDeleteShader(f);
    return prog;
}

void updateScoreDisplay() {
    cout << "\rScore: " << score << "    " << flush;
}

// --- NEW: Function to change the current piece's material ---
void applyMaterialToCurrentPiece() {
    const PBRMaterial& newMaterial = materials[currentMaterialIndex];
    for(auto& cube : currentPiece) {
        cube.albedo = newMaterial.albedo;
        cube.metallic = newMaterial.metallic;
        cube.roughness = newMaterial.roughness;
    }
    cout << "\nMaterial: " << newMaterial.name << flush;
    updateScoreDisplay(); // Redraw score after printing material name
}


// --- MODIFIED: spawnPiece now uses the selected material ---
void spawnPiece() {
    currentPiece.clear();
    pieceCenter = vec3(floor(GRID_W / 2.0f), GRID_H - 2, floor(GRID_D / 2.0f));
    int pieceType = rand() % TETROMINOES.size();
    const auto& shape = TETROMINOES[pieceType];

    const PBRMaterial& currentMaterial = materials[currentMaterialIndex];

    for(const auto& offset : shape) {
        currentPiece.push_back({
            pieceCenter + offset,
            currentMaterial.albedo,
            currentMaterial.metallic,
            currentMaterial.roughness
        });
    }

    if (checkCollision(vec3(0), currentPiece)) {
        gameOver = true;
        cout << "\n\n      GAME OVER\n   Final Score: " << score << "\n   Press 'R' to Restart\n\n" << endl;
    }
}

bool checkCollision(vec3 moveDir, const vector<Cube>& piece) {
    for (const auto& cube : piece) {
        vec3 nextPos = cube.pos + moveDir;
        if (nextPos.x < 0 || round(nextPos.x) >= GRID_W || nextPos.z < 0 || round(nextPos.z) >= GRID_D || nextPos.y < 0) return true;
        for (const auto& landedCube : grid) {
            if (round(nextPos.x) == round(landedCube.pos.x) && round(nextPos.y) == round(landedCube.pos.y) && round(nextPos.z) == round(landedCube.pos.z)) return true;
        }
    }
    return false;
}

void mergePiece() {
    for (const auto& pieceCube : currentPiece) {
        grid.push_back(pieceCube);
        if (pieceCube.pos.y >= GAME_OVER_HEIGHT) {
            gameOver = true;
        }
    }
    if (gameOver) {
        cout << "\n\n      GAME OVER\n   Final Score: " << score << "\n   Press 'R' to Restart\n\n" << endl;
        return;
    }
    score += 10;
    updateScoreDisplay();
    for (int y = 0; y < GRID_H; ++y) {
        int cubeCount = 0;
        for(const auto& c : grid) {
            if (round(c.pos.y) == y) cubeCount++;
        }
        if (cubeCount >= GRID_W * GRID_D) {
            score += 100;
            updateScoreDisplay();
            grid.erase(remove_if(grid.begin(), grid.end(), [y](const Cube& c){ return round(c.pos.y) == y; }), grid.end());
            for(auto& c : grid) {
                if(round(c.pos.y) > y) c.pos.y -= 1.0f;
            }
            y--;
        }
    }
}

// (The init functions are unchanged)
void initGrid() {
    vector<float> gridLines;
    for (int i = 0; i <= GRID_W; ++i) {
        gridLines.push_back(i); gridLines.push_back(0); gridLines.push_back(0);
        gridLines.push_back(i); gridLines.push_back(0); gridLines.push_back(GRID_D);
    }
    for (int i = 0; i <= GRID_D; ++i) {
        gridLines.push_back(0); gridLines.push_back(0); gridLines.push_back(i);
        gridLines.push_back(GRID_W); gridLines.push_back(0); gridLines.push_back(i);
    }
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(float), gridLines.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void initLimitPlane() {
    float planeY = (float)GAME_OVER_HEIGHT;
    float planeVertices[] = {
        0.0f, planeY, 0.0f, (float)GRID_W, planeY, 0.0f, (float)GRID_W, planeY, (float)GRID_D,
        (float)GRID_W, planeY, (float)GRID_D, 0.0f, planeY, (float)GRID_D, 0.0f, planeY, 0.0f,
    };
    glGenVertexArrays(1, &limitPlaneVAO);
    glGenBuffers(1, &limitPlaneVBO);
    glBindVertexArray(limitPlaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, limitPlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void initCube() {
    float verts[] = {
        -0.5f,-0.5f,-0.5f, 0,0,-1,  0.5f,-0.5f,-0.5f, 0,0,-1,  0.5f, 0.5f,-0.5f, 0,0,-1,
         0.5f, 0.5f,-0.5f, 0,0,-1, -0.5f, 0.5f,-0.5f, 0,0,-1, -0.5f,-0.5f,-0.5f, 0,0,-1,
        -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,
         0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f,-0.5f, 0.5f, 0,0,1,
        -0.5f, 0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f,-0.5f,-1,0,0,  -0.5f,-0.5f,-0.5f,-1,0,0,
        -0.5f,-0.5f,-0.5f,-1,0,0,  -0.5f,-0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f, 0.5f,-1,0,0,
         0.5f, 0.5f, 0.5f, 1,0,0,   0.5f, 0.5f,-0.5f, 1,0,0,   0.5f,-0.5f,-0.5f, 1,0,0,
         0.5f,-0.5f,-0.5f, 1,0,0,   0.5f,-0.5f, 0.5f, 1,0,0,   0.5f, 0.5f, 0.5f, 1,0,0,
        -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
         0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f,-0.5f, 0,-1,0,
        -0.5f, 0.5f,-0.5f, 0,1,0,   0.5f, 0.5f,-0.5f, 0,1,0,   0.5f, 0.5f, 0.5f, 0,1,0,
         0.5f, 0.5f, 0.5f, 0,1,0,  -0.5f, 0.5f, 0.5f, 0,1,0,  -0.5f, 0.5f,-0.5f, 0,1,0
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// (The draw functions are unchanged)
void drawCube(const Cube& cube, const mat4& view, const mat4& proj) {
    glUseProgram(cubeShaderProgram);
    mat4 model = translate(mat4(1.0f), cube.pos * CUBE_SIZE + vec3(0.5f, 0.5f, 0.5f));
    glUniformMatrix4fv(glGetUniformLocation(cubeShaderProgram, "model"), 1, GL_FALSE, value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(cubeShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(cubeShaderProgram, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniform3fv(glGetUniformLocation(cubeShaderProgram, "albedo"), 1, value_ptr(cube.albedo));
    glUniform1f(glGetUniformLocation(cubeShaderProgram, "metallic"), cube.metallic);
    glUniform1f(glGetUniformLocation(cubeShaderProgram, "roughness"), cube.roughness);
    glUniform1f(glGetUniformLocation(cubeShaderProgram, "ao"), 1.0f);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void drawGrid(const mat4& view, const mat4& proj) {
    glUseProgram(simpleShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "model"), 1, GL_FALSE, value_ptr(mat4(1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniform3f(glGetUniformLocation(simpleShaderProgram, "color"), 0.3f, 0.3f, 0.3f);
    glUniform1f(glGetUniformLocation(simpleShaderProgram, "alpha"), 0.5f);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, (GRID_W + 1 + GRID_D + 1) * 2);
    glBindVertexArray(0);
}

void drawLimitPlane(const mat4& view, const mat4& proj) {
    glUseProgram(simpleShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "model"), 1, GL_FALSE, value_ptr(mat4(1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(simpleShaderProgram, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniform3f(glGetUniformLocation(simpleShaderProgram, "color"), 1.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(simpleShaderProgram, "alpha"), 0.2f);
    glBindVertexArray(limitPlaneVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

// --- MODIFIED: key_callback now handles the 'M' key ---
void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_RELEASE) return;

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        grid.clear(); score = 0; gameOver = false; paused = false;
        cout << "\n--- Game Reset ---" << endl;
        updateScoreDisplay();
        cout << "\nMaterial: " << materials[currentMaterialIndex].name << flush;
        spawnPiece();
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) paused = !paused;

    if (key == GLFW_KEY_J) camAngle -= 0.05f;
    if (key == GLFW_KEY_L) camAngle += 0.05f;

    // --- NEW: Handle Material Change ---
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        currentMaterialIndex = (currentMaterialIndex + 1) % materials.size();
        if(!gameOver && !paused) {
             applyMaterialToCurrentPiece();
        }
    }

    if (paused || gameOver) return;

    vec3 moveDir(0.0f);
    if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT) moveDir.x = -1.0f;
    if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) moveDir.x = 1.0f;
    if (key == GLFW_KEY_W || key == GLFW_KEY_UP) moveDir.z = -1.0f;
    if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) moveDir.z = 1.0f;
    if (key == GLFW_KEY_SPACE) moveDir.y = -1.0f;

    if (length(moveDir) > 0.0f && !checkCollision(moveDir, currentPiece)) {
        for (auto& b : currentPiece) b.pos += moveDir;
        pieceCenter += moveDir;
    }

    if (key == GLFW_KEY_ENTER) {
        while (!checkCollision(vec3(0, -1, 0), currentPiece)) {
            for (auto& b : currentPiece) b.pos.y -= 1.0f;
            pieceCenter.y -= 1.0f;
        }
        mergePiece();
        if (!gameOver) spawnPiece();
        lastFall = glfwGetTime();
    }
    
    if (action == GLFW_PRESS && (key == GLFW_KEY_Q || key == GLFW_KEY_E)) {
        vector<Cube> rotatedPiece = currentPiece;
        float angle = (key == GLFW_KEY_E) ? 90.0f : -90.0f;
        for(auto& cube : rotatedPiece) {
            vec3 offset = cube.pos - pieceCenter;
            vec3 rotatedOffset = rotateY(offset, radians(angle));
            cube.pos = pieceCenter + round(rotatedOffset);
        }
        if(!checkCollision(vec3(0), rotatedPiece)) {
            currentPiece = rotatedPiece;
        }
    }
}
