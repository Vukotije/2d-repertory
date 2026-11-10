#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <ctime>

#include "../Header/Util.h"

// All positions and sizes are in virtual coordinates: 1600 x 900, origin in the top left corner.
// drawQuad() maps them to NDC, and mouse_callback() maps the cursor back into this space.
#define VIRTUAL_W 1600.0f
#define VIRTUAL_H 900.0f

#define ROWS 6
#define COLS 10
#define NUM_SEATS (ROWS * COLS)

// Hall layout
#define SEAT_W 72.0f
#define SEAT_H 60.0f
#define SEATS_X 336.0f // top left corner of the first seat
#define SEATS_Y 234.0f
#define SEAT_STEP 102.0f // distance between neighbouring seats
#define AISLE_X 150.0f // x of the aisle leading from the entrance to the rows
#define DOOR_X 96.0f
#define DOOR_W 108.0f
#define WALL_H 24.0f

#define PERSON_SPEED 300.0f // virtual pixels per second
#define MOVIE_SECONDS 20.0
#define FRAMES_PER_COLOR 12 // frames between screen colour changes

// Seat statuses
#define FREE 0
#define RESERVED 1
#define BOUGHT 2

enum Phase { RESERVATION, ENTERING, MOVIE, LEAVING };

struct Person {
    float x, y; // current position (centre)
    float seatX, seatY; // centre of the target seat
    float r, g, b; // shirt colour
    bool active; // started walking
    bool done; // reached the seat (entering) or the exit (leaving)
};

int screenWidth = 800;
int screenHeight = 800;

int seats[NUM_SEATS];
Person people[NUM_SEATS];
int numPeople = 0;

Phase phase = RESERVATION;
bool doorOpen = false;
float spawnTimer = 0.0f; // seconds until the next person starts walking
double movieStartTime = 0.0;
int movieFrame = 0;
float screenR = 1.0f, screenG = 1.0f, screenB = 1.0f; // cinema screen colour

unsigned int basicShader;
unsigned int seatTexture;
unsigned int personTexture;
unsigned int carpetTexture;
unsigned int curtainTexture;
unsigned int nameTagTexture;

float seatX(int i) { return SEATS_X + (i % COLS) * SEAT_STEP; }
float seatY(int i) { return SEATS_Y + (i / COLS) * SEAT_STEP; }

void resetCinema() {
    // Initial state: all seats free, hall empty, door closed, screen white
    for (int i = 0; i < NUM_SEATS; ++i) seats[i] = FREE;
    numPeople = 0;
    phase = RESERVATION;
    doorOpen = false;
    screenR = screenG = screenB = 1.0f;
}

void buySeats(int n) {
    // Finds the first n adjacent free seats, scanning from the rightmost seat of the last row
    for (int row = ROWS - 1; row >= 0; --row) {
        int freeInARow = 0;
        for (int col = COLS - 1; col >= 0; --col) {
            if (seats[row * COLS + col] == FREE) freeInARow++;
            else freeInARow = 0;

            if (freeInARow == n) {
                for (int k = 0; k < n; ++k) seats[row * COLS + col + k] = BOUGHT;
                return;
            }
        }
    }
}

void startProjection() {
    // Indices of all reserved and bought seats
    int taken[NUM_SEATS];
    int numTaken = 0;
    for (int i = 0; i < NUM_SEATS; ++i) {
        if (seats[i] != FREE) taken[numTaken++] = i;
    }

    // Fisher-Yates shuffle, so people get random seats
    for (int i = numTaken - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = taken[i];
        taken[i] = taken[j];
        taken[j] = temp;
    }

    // Random number of people, between 1 and the number of taken seats
    numPeople = numTaken > 0 ? 1 + rand() % numTaken : 0;

    float shirts[5][3] = {
        {0.94f, 0.91f, 0.86f}, {0.37f, 0.82f, 0.75f}, {1.0f, 0.56f, 0.69f}, {0.71f, 0.88f, 0.29f}, {1.0f, 0.64f, 0.23f}
    };

    for (int i = 0; i < numPeople; ++i) {
        int shirt = rand() % 5;
        people[i].x = AISLE_X;
        people[i].y = -30.0f; // just outside the entrance, above the top edge
        people[i].seatX = seatX(taken[i]) + SEAT_W / 2;
        people[i].seatY = seatY(taken[i]) + SEAT_H / 2 - 6.0f;
        people[i].r = shirts[shirt][0];
        people[i].g = shirts[shirt][1];
        people[i].b = shirts[shirt][2];
        people[i].active = false;
        people[i].done = false;
    }

    phase = ENTERING;
    doorOpen = true;
    spawnTimer = 0.0f;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Seats can only change during the reservation phase
    if (action != GLFW_PRESS || phase != RESERVATION) return;

    // Accept digits and Enter from both the main keyboard and the numpad
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) buySeats(key - GLFW_KEY_0);
    if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_9) buySeats(key - GLFW_KEY_KP_0);
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) startProjection();
}

void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS || phase != RESERVATION) return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    // Window pixels -> virtual coordinates
    float mouseX = xpos / screenWidth * VIRTUAL_W;
    float mouseY = ypos / screenHeight * VIRTUAL_H;

    for (int i = 0; i < NUM_SEATS; ++i) {
        if (mouseX > seatX(i) && mouseX < seatX(i) + SEAT_W &&
            mouseY > seatY(i) && mouseY < seatY(i) + SEAT_H) {
            if (seats[i] == FREE) seats[i] = RESERVED;
            else if (seats[i] == RESERVED) seats[i] = FREE;
            return;
        }
    }
}

void preprocessTexture(unsigned& texture, const char* filepath) {
    texture = loadImageToTexture(filepath);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Repeat wrapping, needed for the tiled carpet
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Nearest filtering keeps pixel art sharp when scaled up
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void formVAO(float* vertices, size_t size, unsigned int& VAO) {
    unsigned int VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);

    // Attribute 0: position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Attribute 1: texture coordinates
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

// Draws the unit quad at the given virtual position and size.
// With texture == 0 a flat colour is drawn, otherwise the texture is tinted by the colour.
void drawQuad(float x, float y, float w, float h, float r, float g, float b, float a,
              unsigned texture = 0, float repeatX = 1.0f, float repeatY = 1.0f) {
    // Virtual (y down) -> NDC (y up)
    float left = x / VIRTUAL_W * 2 - 1;
    float bottom = 1 - (y + h) / VIRTUAL_H * 2;

    glUniform2f(glGetUniformLocation(basicShader, "uPos"), left, bottom);
    glUniform2f(glGetUniformLocation(basicShader, "uSize"), w / VIRTUAL_W * 2, h / VIRTUAL_H * 2);
    glUniform4f(glGetUniformLocation(basicShader, "uColor"), r, g, b, a);
    glUniform2f(glGetUniformLocation(basicShader, "uTexRepeat"), repeatX, repeatY);
    glUniform1i(glGetUniformLocation(basicShader, "useTexture"), texture != 0);

    if (texture != 0) glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void drawHall() {
    // Carpet tiled across the whole display, 48 x 48 per tile
    drawQuad(0, 0, VIRTUAL_W, VIRTUAL_H, 1, 1, 1, 1, carpetTexture, VIRTUAL_W / 48, VIRTUAL_H / 48);

    // Red aisle runner with a gold edge
    drawQuad(AISLE_X - 48, WALL_H, 96, VIRTUAL_H, 0.88f, 0.69f, 0.25f, 1);
    drawQuad(AISLE_X - 42, WALL_H, 84, VIRTUAL_H, 0.54f, 0.10f, 0.13f, 1);

    // Top wall and the entrance opening
    drawQuad(0, 0, VIRTUAL_W, WALL_H, 0.11f, 0.06f, 0.14f, 1);
    drawQuad(DOOR_X, 0, DOOR_W, WALL_H, 0.36f, 0.21f, 0.38f, 1);
}

void drawScreen() {
    // Frame and screen
    drawQuad(396, 36, 906, 66, 0.11f, 0.06f, 0.14f, 1);
    drawQuad(402, 42, 894, 54, screenR, screenG, screenB, 1);

    // Curtains on both sides
    drawQuad(336, 30, 60, 84, 1, 1, 1, 1, curtainTexture);
    drawQuad(1302, 30, 60, 84, 1, 1, 1, 1, curtainTexture);

    // Marquee bulbs: even and odd ones alternate three times a second
    int blink = (int)(glfwGetTime() * 3);
    for (int i = 0; i < 25; ++i) {
        if ((i + blink) % 2 == 1) drawQuad(408 + i * 36, 24, 12, 12, 1.0f, 0.91f, 0.63f, 1);
        else drawQuad(408 + i * 36, 24, 12, 12, 0.48f, 0.35f, 0.12f, 1);
    }
}

void drawSeats() {
    // One greyscale seat texture, tinted by status
    for (int i = 0; i < NUM_SEATS; ++i) {
        if (seats[i] == FREE) drawQuad(seatX(i), seatY(i), SEAT_W, SEAT_H, 0.18f, 0.42f, 0.87f, 1, seatTexture); // blue
        else if (seats[i] == RESERVED) drawQuad(seatX(i), seatY(i), SEAT_W, SEAT_H, 0.95f, 0.76f, 0.19f, 1, seatTexture); // yellow
        else drawQuad(seatX(i), seatY(i), SEAT_W, SEAT_H, 0.84f, 0.23f, 0.20f, 1, seatTexture); // red
    }
}

void drawPeople() {
    for (int i = 0; i < numPeople; ++i) {
        Person& p = people[i];

        // Leaving: hide those who are already out. Otherwise: show those who have started walking.
        bool visible = (phase == LEAVING) ? !p.done : (p.active || phase == MOVIE);
        if (!visible) continue;

        // Small hop while walking
        float hop = 0.0f;
        if (p.active && !p.done) hop = round(fabs(sin(glfwGetTime() * 14)) * 2) * 3;

        drawQuad(p.x - 30, p.y - 18 - hop, 60, 42, p.r, p.g, p.b, 1, personTexture);
    }
}

void drawDoor() {
    // Closed: across the opening. Open: swung into the hall.
    if (doorOpen) drawQuad(DOOR_X - 12, WALL_H, 12, DOOR_W, 0.69f, 0.48f, 0.24f, 1);
    else drawQuad(DOOR_X, 6, DOOR_W, 12, 0.69f, 0.48f, 0.24f, 1);
}

void drawOverlay() {
    // During the movie the screen casts a faint tint over the hall
    if (phase == MOVIE) drawQuad(0, 0, VIRTUAL_W, VIRTUAL_H, screenR, screenG, screenB, 0.12f);

    // Dark grey overlay at 0.5 opacity, only during reservation
    if (phase == RESERVATION) drawQuad(0, 0, VIRTUAL_W, VIRTUAL_H, 0.25f, 0.25f, 0.25f, 0.5f);
}

void drawNameTag() {
    // Semi-transparent name tag in the bottom right corner
    drawQuad(VIRTUAL_W - 495, VIRTUAL_H - 72, 471, 48, 1, 1, 1, 0.6f, nameTagTexture);
}

// Moves value towards target by at most step. Returns true once it arrives.
bool moveToward(float& value, float target, float step) {
    if (fabs(target - value) <= step) {
        value = target;
        return true;
    }
    if (target > value) value += step;
    else value -= step;
    return false;
}

void update(float dt) {
    // Scaled by frame time, so the walking speed stays constant
    float step = PERSON_SPEED * dt;

    // People start walking one at a time, 0.45 s apart
    if (phase == ENTERING || phase == LEAVING) {
        spawnTimer -= dt;
        for (int i = 0; i < numPeople && spawnTimer <= 0; ++i) {
            if (!people[i].active) {
                people[i].active = true;
                spawnTimer = 0.45f;
            }
        }
    }

    bool allDone = true;
    for (int i = 0; i < numPeople; ++i) {
        Person& p = people[i];
        if (p.active && !p.done) {
            // Entering: down the aisle to the row, then along the row to the seat
            if (phase == ENTERING && moveToward(p.y, p.seatY, step)) p.done = moveToward(p.x, p.seatX, step);
            // Leaving: the same path in reverse
            if (phase == LEAVING && moveToward(p.x, AISLE_X, step)) p.done = moveToward(p.y, -30.0f, step);
        }
        if (!p.done) allDone = false;
    }

    if (phase == ENTERING && allDone) {
        // Everyone is seated: close the door and start the movie
        doorOpen = false;
        phase = MOVIE;
        movieStartTime = glfwGetTime();
        movieFrame = 0;
    }
    else if (phase == MOVIE) {
        if (movieFrame % FRAMES_PER_COLOR == 0) {
            screenR = rand() % 256 / 255.0f;
            screenG = rand() % 256 / 255.0f;
            screenB = rand() % 256 / 255.0f;
        }
        movieFrame++;

        if (glfwGetTime() - movieStartTime >= MOVIE_SECONDS) {
            // Movie over: white screen, open the door, start leaving
            screenR = screenG = screenB = 1.0f;
            doorOpen = true;
            phase = LEAVING;
            spawnTimer = 0.0f;
            for (int i = 0; i < numPeople; ++i) {
                people[i].active = false;
                people[i].done = false;
            }
        }
    }
    else if (phase == LEAVING && allDone) {
        // Hall is empty: back to the initial state
        resetCinema();
    }
}

int main()
{
    // OpenGL 3.3 core profile
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    // Required for a core profile context on macOS
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Full screen on the primary monitor
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "2D Repertory - Cinema", monitor, NULL);
    if (window == NULL) return endProgram("The window could not be created.");
    glfwMakeContextCurrent(window);
    // VSync off, so the frame limiter alone sets the frame rate
    glfwSwapInterval(0);
    // Actual window size, used to map the cursor (may differ from the video mode, e.g. on macOS)
    glfwGetWindowSize(window, &screenWidth, &screenHeight);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);

    // Film camera cursor
    GLFWcursor* cursor = loadImageToCursor("Resources/camera.png");
    glfwSetCursor(window, cursor);

    if (glewInit() != GLEW_OK) return endProgram("GLEW could not be initialised.");

    // Alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    preprocessTexture(seatTexture, "Resources/seat.png");
    preprocessTexture(personTexture, "Resources/person.png");
    preprocessTexture(carpetTexture, "Resources/carpet.png");
    preprocessTexture(curtainTexture, "Resources/curtain.png");
    preprocessTexture(nameTagTexture, "Resources/nametag.png");

    // Unit square, origin in the bottom left; scaled and positioned through uniforms
    float verticesQuad[] = {
     // x     y        s     t
        0.0f, 1.0f,    0.0f, 1.0f, // top left vertex
        0.0f, 0.0f,    0.0f, 0.0f, // bottom left vertex
        1.0f, 0.0f,    1.0f, 0.0f, // bottom right vertex
        1.0f, 1.0f,    1.0f, 1.0f, // top right vertex
    };

    // The VAO must exist before createShader(), whose program validation needs a bound VAO
    unsigned int VAOquad;
    formVAO(verticesQuad, sizeof(verticesQuad), VAOquad);

    // Single program and VAO for the whole scene, bound once
    basicShader = createShader("Shaders/basic.vert", "Shaders/basic.frag");
    glUseProgram(basicShader);
    glUniform1i(glGetUniformLocation(basicShader, "uTex0"), 0);

    srand(time(NULL));
    resetCinema();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    double lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double initFrameTime = glfwGetTime();
        float dt = initFrameTime - lastFrameTime;
        lastFrameTime = initFrameTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }

        update(dt);

        glClear(GL_COLOR_BUFFER_BIT);

        // Back to front
        drawHall();
        drawScreen();
        drawSeats();
        drawPeople();
        drawDoor();
        drawOverlay();
        drawNameTag(); // above the overlay

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame limiter: 75 FPS
        while (glfwGetTime() - initFrameTime < 1 / 75.0) {}
    }

    glDeleteProgram(basicShader);
    glfwDestroyCursor(cursor);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
